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
	bAiming = bIsAiming; //사실 없어도 되는코드, 만일 서버송신이 안되도 내머신에서는 Aiming으로 보이게하는정도?
	ServerSetAiming(bIsAiming); //어쩌피 서버에서는 위 코드와 같은 결과를 갖고오고, 클라에서는 서버요청을 해야하므로 if문 필요없음.
	if (Character) //없어도 괜찮음. ServerSetAiming에 또 있다.
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimingWalkSpeed : BaseWalkSpeed; //에임하는경우 좌측, 에임푸는경우 우측 스피드로
	}
}
void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	if (Character) //서버측에 업데이트 하지 않으면, 서버는 강제로 해당 캐릭터의 위치를 서버측에 동기화시킴(builtin된 무브먼트comp는 동기화까지 신경쓴 컴포넌트이기 때문)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimingWalkSpeed : BaseWalkSpeed; //에임하는경우 좌측, 에임푸는경우 우측 스피드로
	}
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon && Character)
	{
		Character->bUseControllerRotationYaw = true; //무기장착시 움직임이 컨트롤러 Yaw인풋을 따라갈수있도록 재설정
		Character->GetCharacterMovement()->bOrientRotationToMovement = false; //movement방향으로 rotation이 돌지않게 재설정	
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{ 
	bFireButtonPressed = bPressed;
	if (Character && bFireButtonPressed) //bFireButtonPressed체크를 안하면 release할때도 한발더 쏘게되더라
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
	EquippedWeapon->SetOwner(Character); //Owner는 따로 OnRep해주지 않아도, 함수 내에서 서버복사 처리해주니, 클라에서 복사 걱정할필요없다.

	Character->bUseControllerRotationYaw = true; //무기장착시 움직임이 컨트롤러 Yaw인풋을 따라갈수있도록 재설정
	Character->GetCharacterMovement()->bOrientRotationToMovement = false; //movement방향으로 rotation이 돌지않게 재설정	
}

