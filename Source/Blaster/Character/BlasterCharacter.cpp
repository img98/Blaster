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
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
}
void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly); //변수복사를 위한 매크로

}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimOffset(DeltaTime);
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // Idle상태이고, 점프중이지 않은 상태의 AO yaw 찾기
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //매프레임마다 aim하고 있는 Yaw를 저장한다.
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation); //두 rot의 순서에 따라 +-반대의 rot이 나오더라!
		AO_Yaw = DeltaAimRotation.Yaw;
		bUseControllerRotationYaw = false; //조준중일때, 캐릭터가 내 컨트롤러의 aim을따라 회전하지 않기위해
		//(=AO에 의한 움직임은 사실 Rotation변화가 아님을 명심하자! 그저 애니메이션이 나오는것일뿐)
	}
	if (Speed > 0.f || bIsInAir) // 뛰거나 점프중일때 (=즉, 이동했을때 =캐릭터가 보는방향이 바뀔 때)
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //매프레임마다 aim하고 있는 Yaw를 저장한다.
		AO_Yaw = 0.f; //이동하면 AimOffset이 0이어야한다...
		bUseControllerRotationYaw = true; //캐릭터가 내 컨트롤러의 aim을따라 회전하기 위해 다시 true
	}

	AO_Pitch = GetBaseAimRotation().Pitch; //pitch는 그대로 조준점의 pitch만 갖다쓰면되서 쉽다.
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
	ACharacter::Jump();
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

bool ABlasterCharacter::IsAiming()
{
	return(Combat && Combat->bAiming);
}
