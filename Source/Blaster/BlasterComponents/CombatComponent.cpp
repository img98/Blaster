// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Camera/CameraComponent.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if (Character->GetFollowCamera())
		{
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if (Character == nullptr || Character->Controller == nullptr) return;

	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller; // ?�� ����Ͽ�, �������� �ʾ������� Ȯ���ϰ�, ���Ŀ��� ���� cast����� �����ʰ� ��밡��
	if (Controller)
	{
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD; //�̷� ������ cost�Ҹ� �ٿ��ִµ� ȿ�����̴�.
		if (HUD)
		{
			if (EquippedWeapon)
			{
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
			}
			else
			{
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
			}

			//Calculate Crosshair Spread
			//WalkSpeed[0, 600]->[0, 1]
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;
			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
			
			if (Character->GetCharacterMovement()->IsFalling())
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}
			
			if (bAiming)
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f); //�þ������ �ϵ��ڵ�
			}
			else
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 10.f); //�ݵ��Է��� Fire()����

			HUDPackage.CrosshairSpread =
				0.5f + //�⺻ ���ӹ����� �ϵ��ڵ�
				CrosshairVelocityFactor +
				CrosshairInAirFactor -
				CrosshairAimFactor +
				CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}

}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if (EquippedWeapon == nullptr) return;

	if (bAiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, EquippedWeapon->ZoomInterpSpeed); //���� Ǯ���� ������ ���� defaultSpeed��
	}

	if (Character && Character->GetFollowCamera())
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	bAiming = bIsAiming; //��� ��� �Ǵ��ڵ�, ���� �����۽��� �ȵǵ� ���ӽſ����� Aiming���� ���̰��ϴ�����?
	ServerSetAiming(bIsAiming); //��¼�� ���������� �� �ڵ�� ���� ����� �������, Ŭ�󿡼��� ������û�� �ؾ��ϹǷ� if�� �ʿ����.
	if (Character) //��� ������. ServerSetAiming�� �� �ִ�.
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimingWalkSpeed : BaseWalkSpeed; //�����ϴ°�� ����, ����Ǫ�°�� ���� ���ǵ��
	}
}
void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	if (Character) //�������� ������Ʈ ���� ������, ������ ������ �ش� ĳ������ ��ġ�� �������� ����ȭ��Ŵ(builtin�� �����Ʈcomp�� ����ȭ���� �Ű澴 ������Ʈ�̱� ����)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimingWalkSpeed : BaseWalkSpeed; //�����ϴ°�� ����, ����Ǫ�°�� ���� ���ǵ��
	}
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon && Character)
	{
		Character->bUseControllerRotationYaw = true; //���������� �������� ��Ʈ�ѷ� Yaw��ǲ�� ���󰥼��ֵ��� �缳��
		Character->GetCharacterMovement()->bOrientRotationToMovement = false; //movement�������� rotation�� �����ʰ� �缳��	
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{ 
	bFireButtonPressed = bPressed;	//bAiming���� �޸� �������縦 ���� �ʴ´� => ���߿� �ڵ����ѵ� ����ǵ�, ��������� ������ ��ȭ(false->true)�� �����Ͽ� �۵��ϱ⿡ �������ϴ�.(��� ������ ������ֱ���)
	//����RPC�� �ƴ϶� NetMulticast RPC�� ����� Ŭ��,���� �Ѵ� �۵��ϰ� ����.

	//ServerFire�� �ִ� Montage����� Fire�Լ� ȣ���� �� SetAiming�� ���� �ƶ����� Ŭ�󿡼� �������� �ʾƵ� �ȴ�.=�׷��� �����ߴ�.

	if (bFireButtonPressed)
	{
		//������ ����� ������, �������� multicast�� �϶�� ��� Ŭ�󿡰� ����Ҽ� �ֱ⿡ 2�� �ܰ踦 ��ġ�°�!
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		ServerFire(HitResult.ImpactPoint); //ImpactPoint�� NetQuantize�� ȣȯ�ȴ�.

		if (EquippedWeapon)
		{
			CrosshairShootingFactor = 2.f; //�߻� �ݵ� ���� �������� ���������̰� ����
		}
	}
}
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	MulticastFire(TraceHitTarget);
}
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	//bFireButtonPressed�� ����� ������ �˸��°͘A�̴�, ������ Ŭ�󿡼� �����ص� �ȴ�.
	if (EquippedWeapon == nullptr) return;
	if (Character)
	{
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport) //viewport�� ������ؼ��� GEnigne�� ��ȿ�ؾ���.
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f); //���� ������
	
	FVector CrosshairWorldPosition; 
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld( // 2d�� viewport��ǥ�� 3d�� ������ point�� direction���� ������ deprojection
		UGameplayStatics::GetPlayerController(this, 0), //�÷��̾� 0��, ��Ʈ�ѷ� ������ �ڽ��� �ǹ�!
		CrosshairLocation,
		CrosshairWorldPosition, //viewport�߾��� world��ǥ
		CrosshairWorldDirection
	);

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;
		if (Character) //ī�޶󿡼� linetrace�� ���۵ǹǷ�, ī�޶�� ĳ���� ���̿� ��ü�� �ִٸ�(�ڽ� ����) trace�� ĳ���� �ڷε� �Ǵ� �۸�ġ�� �ذ��غ���
		{	//ĳ���ͱ����� �Ÿ��� ���ϰ� Start�� ���Ͽ�, ĳ���� �տ������� trace�� ���۵ǵ��� �ϸ� �ȴ�. 100.f�� ĳ���ͺ��� ���ݴ� ���� �ǹ�
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f); 
		}
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult, //ref�� �ǳ��޾Ҵ��Ϳ� ����
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);
		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>()) //Ʈ���̽� ����� �ְ�, �ش� interface�� �ִٸ�?(implements)
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else
		{
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}
	}
}

void UCombatComponent::EquipWeapon(class AWeapon* WeaponToEquip)
{
	if (Character == nullptr || WeaponToEquip == nullptr) return;

	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	EquippedWeapon->SetOwner(Character); //Owner�� ���� OnRep������ �ʾƵ�, �Լ� ������ �������� ó�����ִ�, Ŭ�󿡼� ���� �������ʿ����.

	Character->bUseControllerRotationYaw = true; //���������� �������� ��Ʈ�ѷ� Yaw��ǲ�� ���󰥼��ֵ��� �缳��
	Character->GetCharacterMovement()->bOrientRotationToMovement = false; //movement�������� rotation�� �����ʰ� �缳��	
}

