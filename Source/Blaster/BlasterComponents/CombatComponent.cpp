// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}


void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
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
	bFireButtonPressed = bPressed;
	if (Character && bFireButtonPressed) //bFireButtonPressedüũ�� ���ϸ� release�Ҷ��� �ѹߴ� ��ԵǴ���
	{
		Character->PlayFireMontage(bAiming);
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
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

