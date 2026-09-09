#include "EquipmentComponent.h"

#include "../Weapons/WeaponActor.h"
#include "CombatComponent.h"
#include "ActionComponent.h"
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

    if (const auto* Actions = OwnerCharacter->FindComponentByClass<UActionComponent>())
        if (Actions->GetActionState() == ETPCActionState::Dead) return false;
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
    SetWeaponDrawn(true);
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponDefinition, EquippedWeaponActor);
	return true;
}

void UEquipmentComponent::SetWeaponDrawn(bool bDrawn)
{
    bWeaponDrawn = bDrawn;
    auto* Character = Cast<ACharacter>(GetOwner());
    if (IsValid(EquippedWeaponActor) && EquippedWeaponDefinition && Character && Character->GetMesh())
    {
        if (!bDrawn)
        {
            EquippedWeaponActor->SetAttackEffectActive(false);
            if (auto* Combat = Character->FindComponentByClass<UCombatComponent>()) Combat->ClearSwordBuff();
        }
        const FName Socket = bDrawn ? EquippedWeaponDefinition->EquipSocketName : EquippedWeaponDefinition->SheathSocketName;
        EquippedWeaponActor->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
        EquippedWeaponActor->SetActorRelativeTransform(bDrawn ? FTransform::Identity : EquippedWeaponDefinition->SheathRelativeTransform);
        EquippedWeaponActor->SetActorHiddenInGame(false);
    }
}

void UEquipmentComponent::UnequipWeapon()
{
    if (auto* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
    {
        Combat->CancelActiveAttack(); Combat->CancelGuard(); Combat->ClearSwordBuff();
    }
    if (auto* Actions = GetOwner() ? GetOwner()->FindComponentByClass<UActionComponent>() : nullptr) Actions->ClearInputBuffers();
    bWeaponDrawn = true;
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
