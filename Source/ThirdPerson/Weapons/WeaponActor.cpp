#include "WeaponActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "WeaponDefinition.h"
#include "WeaponVFXComponent.h"

AWeaponActor::AWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WeaponVFX = CreateDefaultSubobject<UWeaponVFXComponent>(TEXT("WeaponVFX"));

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	SkeletalWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("SkeletalWeaponMesh"));
	SkeletalWeaponMesh->SetupAttachment(WeaponMesh);
	SkeletalWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalWeaponMesh->SetGenerateOverlapEvents(false);
}

void AWeaponActor::InitializeWeapon(UWeaponDefinition* InWeaponDefinition)
{
	WeaponDefinition = InWeaponDefinition;
}

void AWeaponActor::SetAttackEffectActive_Implementation(bool bActive)
{
	WeaponVFX->SetAttackActive(bActive);
}
