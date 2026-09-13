#include "WeaponActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
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
    // A weapon's helper boxes/spheres must never be rendered; this does not disable their collision.
    TInlineComponentArray<UShapeComponent*> Shapes(this);
    for (auto* Shape : Shapes) { Shape->SetHiddenInGame(true); Shape->SetVisibility(false); }
}

void AWeaponActor::SetAttackEffectActive_Implementation(bool bActive)
{
	WeaponVFX->SetAttackActive(bActive);
}
