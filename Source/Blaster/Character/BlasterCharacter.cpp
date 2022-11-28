#include "BlasterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterComponents/CombatComponent.h"

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

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly); //�������縦 ���� ��ũ��

}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

bool ABlasterCharacter::IsAiming()
{
	return(Combat && Combat->bAiming);
}
