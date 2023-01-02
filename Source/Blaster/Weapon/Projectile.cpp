// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Blaster.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; //서버에서만 생성되게 작성했기에, Replicates 처리를 해줘야 클라에서도 생성할수있따.

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block); //캐릭터를 의미하는 collisionChannel생성(Blaster.h) 및 사용

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true; //탄의 rotation이 궤적을 따라 변화 true
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (Tracer)
	{
		TracerComponent = UGameplayStatics::SpawnEmitterAttached(
			Tracer,
			CollisionBox,
			FName(), //붙이고싶은 위치가 없다면 비워둔다.
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition
			);
	}

	if (HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit); //collision에서 OnComponentHit가 일어나면, 우리가 만든 OnHit에 연결
	}
}

void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		BlasterCharacter->MulticastHit();

	}

	Destroyed();
}

void AProjectile::Destroyed()
{
	Super::Destroyed(); //builtin된 Destroyed를 사용하면 서버뿐 아니라 클라에서도 쉽게 삭제할수있다. 서버뿐 아니라 클라에도 간섭한다는 특성을 이용해, 
	//클라에 간섭할때 삭제만이 아닌 hit이벤트를 발생시킨다면 RPC를 사용하지 않고도 효과적이게 복사가 가능할것이다!
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
}

