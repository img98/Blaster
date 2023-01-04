#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "Blaster/Blaster.h"

ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject <USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject <UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false; //캐릭터가 컨트롤러Yaw인풋에 따라 돌지 않게
	GetCharacterMovement()->bOrientRotationToMovement = true; //movement방향으로 rotation을 돌게 하는 속성

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true); //컴포넌트는 RepLifetime할것없이, Replicated설정을 해줘야 한다.

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore); //캐릭터가 다른캐릭터 뒤로 지나갈때 타캐릭터의 카메라시점 바꾸는것 방지
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block); //LineTrace가 통과되는걸 막기위해 Block설정 추가

	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f); //돌때 얼마나 빠르게 돌지
	TurningInPlace = ETurningInPlace::ETIP_NotTurning; //TurnInPlace의 초기설정
	NetUpdateFrequency = 66.f; //초마다 net에서 얼마나 이 액터를 복사할것인지 설정. 보통 빠른 템포의 fps는 66-33 frequency를 사용한다더라
	MinNetUpdateFrequency = 33.f; // 외에도 DefaultEngine.ini에서 NetServerMaxTickRate는 주로 60으로 맞춘다함
}
void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName; 
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip"); //사용할 Montage섹션 정하기
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		//TODO: 히트 각도에 따라 적절한 SectionName 사용하기. 일단 확인용으로 Front만 나오게함.
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");

		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::MulticastHit_Implementation()
{
	PlayHitReactMontage();
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly); //변수복사를 위한 매크로

}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f; //DeltaTime 리셋
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled()) //ENetRole을 피킹해보면 Enum이기에 0=None/1=SimProxy/2=AutoProxy/3=Authority 라는걸 알수있다.
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f) //deltaTime이 어느정도 지나면 움직임 복사함수를 실행(매틱마다 복사할수 없어서 이렇게 해야함)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}

	HideCameraIfCharacterClose();
}

float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (Combat && Combat->EquippedWeapon == nullptr) return;

	bool bIsInAir = GetCharacterMovement()->IsFalling();
	float Speed = CalculateSpeed();
	if (Speed == 0.f && !bIsInAir) // Idle상태이고, 점프중이지 않은 상태의 AO yaw 찾기
	{
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //매프레임마다 aim하고 있는 Yaw를 저장한다.
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation); //두 rot의 순서에 따라 +-반대의 rot이 나오더라!
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw; //InterpAO_Yaw는 AO_yaw가 얼마나 돌아갔는지 저장하는 변수라고 생각하면될듯?
		}
		bUseControllerRotationYaw = true; //aim을 돌리며, 액터를 회전하며, 애니메이션을 틀기위해 true로
		//(=AO에 의한 움직임은 사실 Rotation변화가 아님을 명심하자! 그저 애니메이션이 나오는것일뿐)

		TurnInPlace(DeltaTime); //가만히 멈춘상태에서 시점을 돌렸을때, 캐릭터가 돌게하는 함수
	}
	if (Speed > 0.f || bIsInAir) // 뛰거나 점프중일때 (=즉, 이동했을때 =캐릭터가 보는방향이 바뀔 때)
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //매프레임마다 aim하고 있는 Yaw를 저장한다.
		AO_Yaw = 0.f; //이동하면 AimOffset이 0이어야한다...
		bUseControllerRotationYaw = true; //캐릭터가 내 컨트롤러의 aim을따라 회전하기 위해 다시 true

		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	CalculateAO_Pitch();
}
void ABlasterCharacter::CalculateAO_Pitch() //Pitch를 조정하는 부분을 함수로 추출
{
	AO_Pitch = GetBaseAimRotation().Pitch; //이론상 그냥 조준점의 pitch를 가져가면 되는 간단한 코드인데...
	if (AO_Pitch > 90.f && !IsLocallyControlled()) //로컬머신은 괜찮은데 서버머신에서는 음수로 나오는 각도가 360도에서부터 감소하는식으로 전송되는 글리치가 있었기에, 그걸 해결하려는 코드
	{
		//pitch를 [270,360)에서 [-90,0]으로 맵핑
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}
void ABlasterCharacter::SimProxiesTurn()
{
	//AimOffset의 애니메이션은 RootBone을 직접 조정하는 방식이기에, SimulatedProxy인 대상이 움직이면 덜덜 떨리는것처럼 보인다.
	//이런 버그를 해결하기 위해 SimProxy라면 ABP에서 AimOffset 애니메이션을 블렌드하긴 하지만 RotateRootBone 노드는 거치지 않게 해줄 bool을 사용하자.
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	bRotateRootBone = false;

	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	if (FMath::Abs(ProxyYaw) > TurnThreshhold)
	{
		if (ProxyYaw > TurnThreshhold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < TurnThreshhold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	UE_LOG(LogTemp, Warning, TEXT("%f"), AO_Yaw);
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f) //일정 각도이상 회전시켰다면,
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //이동한것처럼 새로운 StartingAimRotaion찾기
		}
	}
}

void ABlasterCharacter::HideCameraIfCharacterClose() //카메라가 너무 가까워지면 캐릭터를 안보이게해 시야 확보
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (Combat&&Combat->EquippedWeapon&&Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookAround", this, &ABlasterCharacter::LookAround);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
}


void ABlasterCharacter::MoveForward(float Value)
{
	if (Controller != nullptr && Value != 0)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}
void ABlasterCharacter::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::LookAround(float Value)
{
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}
void ABlasterCharacter::EquipButtonPressed()
{
	if (Combat)
	{
		if (HasAuthority()) //서버일 경우, 장착로직 실행
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else //클라이언트의 경우 RPC(Remote Procedure Call)를 보낸다.
		{
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (Combat)
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}

}

void ABlasterCharacter::CrouchButtonPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch(); //ACharacter에는 이미 Crouch()가 내장되어있고, 여기서 bIsCrouched의 복사까지 다 설정해놨다. Jump처럼
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}
void ABlasterCharacter::AimButtonReleased()
{
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}
void ABlasterCharacter::FireButtonReleased()
{
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon) //서버에서만 작동하는 버전 <-RepNotify버전은 서버에서는 이뤄지지않기에 따로 버전이 필요하다.
{
	if (OverlappingWeapon) //overlappingweapon=weapon을 하기전에 값이 있다면. 즉, 이전에 할당한 적이 있다면
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = Weapon;
	if (IsLocallyControlled()) //지금 머신에서 컨트롤하는지 확인가능 = 내가 서버라는것을 알수있다.
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon) 
{	
	//그냥 서버에서 변수를 고치면 모든 머신들의 변수가 변하기에, overlap을 한 머신에서만 호출하는 함수를 만든다.
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon) //endoverlap을 구현하기 위해 추적
	{
		LastWeapon->ShowPickupWidget(false);
	}
}
bool ABlasterCharacter::IsWeaponEquipped()
{
	return(Combat && Combat->EquippedWeapon);
}
AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

bool ABlasterCharacter::IsAiming()
{
	return(Combat && Combat->bAiming);
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
	return Combat->HitTarget;
}