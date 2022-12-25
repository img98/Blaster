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

	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller; // ?를 사용하여, 설정되지 않았을때는 확실하게, 그후에는 굳이 cast비용이 들지않게 사용가능
	if (Controller)
	{
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD; //이런 문법은 cost소모를 줄여주는데 효과적이다.
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
	bFireButtonPressed = bPressed;	//bAiming과는 달리 서버복사를 하지 않는다 => 나중에 자동소총도 만들건데, 서버복사는 변수의 변화(false->true)를 감지하여 작동하기에 부적절하다.(사실 만들라면 만들수있긴함)
	//서버RPC가 아니라 NetMulticast RPC를 사용해 클라,서버 둘다 작동하게 하자.

	//ServerFire에 있는 Montage재생과 Fire함수 호출은 위 SetAiming과 같은 맥락으로 클라에서 실행하지 않아도 된다.=그래서 삭제했다.

	if (bFireButtonPressed)
	{
		//서버에 명령을 보내야, 서버에서 multicast를 하라고 모든 클라에게 명령할수 있기에 2개 단계를 거치는것!
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		ServerFire(HitResult.ImpactPoint); //ImpactPoint는 NetQuantize와 호환된다.
	}
}
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	MulticastFire(TraceHitTarget);
}
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	//bFireButtonPressed는 명령이 왔음을 알리는것쁀이니, 온전히 클라에서 관리해도 된다.
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
	if (GEngine && GEngine->GameViewport) //viewport를 얻기위해서는 GEnigne이 유효해야함.
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f); //벡터 생성자
	
	FVector CrosshairWorldPosition; 
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld( // 2d인 viewport좌표를 3d인 월드의 point와 direction으로 역투영 deprojection
		UGameplayStatics::GetPlayerController(this, 0), //플레이어 0은, 컨트롤러 소유자 자신을 의미!
		CrosshairLocation,
		CrosshairWorldPosition, //viewport중앙의 world좌표
		CrosshairWorldDirection
	);

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;
		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult, //ref로 건내받았던것에 저장
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
	EquippedWeapon->SetOwner(Character); //Owner는 따로 OnRep해주지 않아도, 함수 내에서 서버복사 처리해주니, 클라에서 복사 걱정할필요없다.

	Character->bUseControllerRotationYaw = true; //무기장착시 움직임이 컨트롤러 Yaw인풋을 따라갈수있도록 재설정
	Character->GetCharacterMovement()->bOrientRotationToMovement = false; //movement방향으로 rotation이 돌지않게 재설정	
}

