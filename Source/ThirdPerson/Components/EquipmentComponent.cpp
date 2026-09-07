#include "EquipmentComponent.h"

#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UEquipmentComponent::UEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (StartingWeapon)
	{
		EquipWeapon(StartingWeapon);
	}
}

bool UEquipmentComponent::EquipWeapon(UWeaponDefinition* NewWeaponDefinition)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh() ||
		!NewWeaponDefinition || !GetWorld())
	{
		return false;
	}

	if (EquippedWeaponDefinition == NewWeaponDefinition &&
		(!NewWeaponDefinition->WeaponActorClass || IsValid(EquippedWeaponActor)))
	{
		return true;
	}

	UnequipWeapon();

	AWeaponActor* NewWeaponActor = nullptr;
	if (NewWeaponDefinition->WeaponActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter;
		SpawnParams.Instigator = OwnerCharacter;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		NewWeaponActor = GetWorld()->SpawnActor<AWeaponActor>(
			NewWeaponDefinition->WeaponActorClass,
			OwnerCharacter->GetActorTransform(),
			SpawnParams);

		if (!NewWeaponActor)
		{
			return false;
		}

		NewWeaponActor->InitializeWeapon(NewWeaponDefinition);
		NewWeaponActor->AttachToComponent(
			OwnerCharacter->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			NewWeaponDefinition->EquipSocketName);
	}

	EquippedWeaponDefinition = NewWeaponDefinition;
	EquippedWeaponActor = NewWeaponActor;
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponDefinition, EquippedWeaponActor);
	return true;
}

void UEquipmentComponent::UnequipWeapon()
{
	if (IsValid(EquippedWeaponActor))
	{
		EquippedWeaponActor->Destroy();
	}

	EquippedWeaponActor = nullptr;
	EquippedWeaponDefinition = nullptr;
	OnEquippedWeaponChanged.Broadcast(nullptr, nullptr);
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(EquippedWeaponActor))
	{
		EquippedWeaponActor->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}
