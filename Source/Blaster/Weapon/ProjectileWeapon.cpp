// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	if (MuzzleFlashSocket)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector ToTarget = HitTarget - SocketTransform.GetLocation(); //��ǥ���� ���� ����
		FRotator TargetRotation = ToTarget.Rotation();
		if (ProjectileClass && InstigatorPawn)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner(); //������ źȯ�� ������, ������ ������
			SpawnParams.Instigator = InstigatorPawn;
			UWorld* World = GetWorld();
			if (World)
			{
				World->SpawnActor<AProjectile>( //����Ŭ����, ������ġ, ��������, �з����͸� ���� ����
					ProjectileClass,
					SocketTransform.GetLocation(),
					TargetRotation,
					SpawnParams
					);
			}
		}
	}
}