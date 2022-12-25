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
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SetHUDCrosshairs(DeltaTime);
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
			FHUDPackage HUDPackage;
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
			HUD->SetHUDPackage(HUDPackage);
		}
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
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult, //ref�� �ǳ��޾Ҵ��Ϳ� ����
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);
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

