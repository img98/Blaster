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

	bUseControllerRotationYaw = false; //ĳ���Ͱ� ��Ʈ�ѷ�Yaw��ǲ�� ���� ���� �ʰ�
	GetCharacterMovement()->bOrientRotationToMovement = true; //movement�������� rotation�� ���� �ϴ� �Ӽ�

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true); //������Ʈ�� RepLifetime�Ұ;���, Replicated������ ����� �Ѵ�.

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore); //ĳ���Ͱ� �ٸ�ĳ���� �ڷ� �������� Ÿĳ������ ī�޶���� �ٲٴ°� ����
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block); //LineTrace�� ����Ǵ°� �������� Block���� �߰�

	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f); //���� �󸶳� ������ ����
	TurningInPlace = ETurningInPlace::ETIP_NotTurning; //TurnInPlace�� �ʱ⼳��
	NetUpdateFrequency = 66.f; //�ʸ��� net���� �󸶳� �� ���͸� �����Ұ����� ����. ���� ���� ������ fps�� 66-33 frequency�� ����Ѵٴ���
	MinNetUpdateFrequency = 33.f; // �ܿ��� DefaultEngine.ini���� NetServerMaxTickRate�� �ַ� 60���� �������
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
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip"); //����� Montage���� ���ϱ�
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly); //�������縦 ���� ��ũ��

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

	if (Speed == 0.f && !bIsInAir) // Idle�����̰�, ���������� ���� ������ AO yaw ã��
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //�������Ӹ��� aim�ϰ� �ִ� Yaw�� �����Ѵ�.
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation); //�� rot�� ������ ���� +-�ݴ��� rot�� ��������!
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw; //InterpAO_Yaw�� AO_yaw�� �󸶳� ���ư����� �����ϴ� ������� �����ϸ�ɵ�?
		}
		bUseControllerRotationYaw = true; //aim�� ������, ���͸� ȸ���ϸ�, �ִϸ��̼��� Ʋ������ true��
		//(=AO�� ���� �������� ��� Rotation��ȭ�� �ƴ��� �������! ���� �ִϸ��̼��� �����°��ϻ�)

		TurnInPlace(DeltaTime); //������ ������¿��� ������ ��������, ĳ���Ͱ� �����ϴ� �Լ�
	}
	if (Speed > 0.f || bIsInAir) // �ٰų� �������϶� (=��, �̵������� =ĳ���Ͱ� ���¹����� �ٲ� ��)
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //�������Ӹ��� aim�ϰ� �ִ� Yaw�� �����Ѵ�.
		AO_Yaw = 0.f; //�̵��ϸ� AimOffset�� 0�̾���Ѵ�...
		bUseControllerRotationYaw = true; //ĳ���Ͱ� �� ��Ʈ�ѷ��� aim������ ȸ���ϱ� ���� �ٽ� true

		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	AO_Pitch = GetBaseAimRotation().Pitch; //�̷л� �׳� �������� pitch�� �������� �Ǵ� ������ �ڵ��ε�...
	if (AO_Pitch > 90.f && !IsLocallyControlled()) //���øӽ��� �������� �����ӽſ����� ������ ������ ������ 360���������� �����ϴ½����� ���۵Ǵ� �۸�ġ�� �־��⿡, �װ� �ذ��Ϸ��� �ڵ�
	{
		//pitch�� [270,360)���� [-90,0]���� ����
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}

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
		if (FMath::Abs(AO_Yaw) < 15.f) //���� �����̻� ȸ�����״ٸ�,
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f); //�̵��Ѱ�ó�� ���ο� StartingAimRotaionã��
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
		if (HasAuthority()) //������ ���, �������� ����
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else //Ŭ���̾�Ʈ�� ��� RPC(Remote Procedure Call)�� ������.
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
		Crouch(); //ACharacter���� �̹� Crouch()�� ����Ǿ��ְ�, ���⼭ bIsCrouched�� ������� �� �����س���. Jumpó��
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


void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon) //���������� �۵��ϴ� ���� <-RepNotify������ ���������� �̷������ʱ⿡ ���� ������ �ʿ��ϴ�.
{
	if (OverlappingWeapon) //overlappingweapon=weapon�� �ϱ����� ���� �ִٸ�. ��, ������ �Ҵ��� ���� �ִٸ�
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = Weapon;
	if (IsLocallyControlled()) //���� �ӽſ��� ��Ʈ���ϴ��� Ȯ�ΰ��� = ���� ������°��� �˼��ִ�.
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon) 
{	
	//�׳� �������� ������ ��ġ�� ��� �ӽŵ��� ������ ���ϱ⿡, overlap�� �� �ӽſ����� ȣ���ϴ� �Լ��� �����.
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon) //endoverlap�� �����ϱ� ���� ����
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