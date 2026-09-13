#include "TPCPlayerProgress.h"
#include "TPCSaveGame.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/LevelComponent.h"
#include "../Items/ItemDefinition.h"
#include "../Weapons/WeaponDefinition.h"
#include "Kismet/GameplayStatics.h"

void TPCPlayerProgress::Capture(const ATPCCharacter& P, UTPCSaveGame& S, bool bRestoreVitals)
{
    S.PlayerHealth = bRestoreVitals ? P.HealthComponent->GetMaxHealth() : P.HealthComponent->GetCurrentHealth();
    S.PlayerStamina = bRestoreVitals ? P.StaminaComponent->GetMaxStamina() : P.StaminaComponent->GetCurrentStamina();
    S.PlayerLevel = P.LevelComponent->GetLevel();
    S.PlayerExperience = P.LevelComponent->GetCurrentExperience();
    S.PlayerUpgrades.Reset();
    for (auto Upgrade : P.LevelComponent->GetSelectedUpgrades()) S.PlayerUpgrades.Add(static_cast<uint8>(Upgrade));
    S.InventoryCapacity = P.InventoryComponent->MaxSlots;
    S.InventorySlots.Reset();
    for (const auto& Slot : P.InventoryComponent->Slots)
        if (Slot.ItemDefinition && Slot.Count > 0)
        {
            auto& Saved = S.InventorySlots.AddDefaulted_GetRef();
            Saved.ItemDefinition = Slot.ItemDefinition; Saved.Count = Slot.Count;
        }
    S.bHasEquipmentState = true;
    S.EquippedWeapon = P.EquipmentComponent->GetEquippedWeaponDefinition();
    S.bWeaponDrawn = P.EquipmentComponent->IsWeaponDrawn();
    S.bDoubleJumpUnlocked = P.bDoubleJumpUnlocked;
    S.PendingUpgradeSelections = P.LevelComponent->GetPendingUpgradeSelections();
    S.PendingUpgradeChoices.Reset();
    for (const auto& Choice : P.LevelComponent->GetPendingUpgradeChoices()) S.PendingUpgradeChoices.Add(static_cast<uint8>(Choice.Type));
}

void TPCPlayerProgress::Apply(ATPCCharacter& P, const UTPCSaveGame& S)
{
    TArray<ELevelUpgradeType> Upgrades;
    for (uint8 Value : S.PlayerUpgrades)
        if (Value <= static_cast<uint8>(ELevelUpgradeType::IronSkin)) Upgrades.Add(static_cast<ELevelUpgradeType>(Value));
    P.LevelComponent->RestoreProgress(S.PlayerLevel, S.PlayerExperience, Upgrades);
    P.LevelComponent->RestorePendingUpgradeChoices(S.PendingUpgradeSelections, S.PendingUpgradeChoices);
    P.HealthComponent->SetCurrentHealth(S.PlayerHealth);
    P.StaminaComponent->SetCurrentStamina(S.PlayerStamina);
    P.InventoryComponent->Slots.Reset();
    P.InventoryComponent->MaxSlots = FMath::Max(P.InventoryComponent->MaxSlots, FMath::Max(S.InventoryCapacity, S.InventorySlots.Num()));
    for (const auto& Slot : S.InventorySlots)
        if (Slot.Count > 0) if (auto* Item = Slot.ItemDefinition.LoadSynchronous()) P.InventoryComponent->AddItem(Item, Slot.Count);
    P.InventoryComponent->OnInventoryChanged.Broadcast();
    if (S.bHasEquipmentState)
    {
        if (S.EquippedWeapon.IsNull()) P.EquipmentComponent->UnequipWeapon();
        else if (auto* Weapon = S.EquippedWeapon.LoadSynchronous()) P.EquipmentComponent->EquipWeapon(Weapon);
        P.EquipmentComponent->SetWeaponDrawn(S.bWeaponDrawn);
        P.SetDoubleJumpUnlocked(S.bDoubleJumpUnlocked);
    }
}

bool TPCPlayerProgress::OwnsCheckpoint(const UTPCSaveGame& S, const UObject* Context)
{
    if (!S.bHasCheckpoint) return false;
    const FString Current = UGameplayStatics::GetCurrentLevelName(Context, true);
    // Old saves predate travel and were written in the original campaign world.
    const FString Owner = S.CheckpointMap.IsEmpty() ? TEXT("Lvl_ThirdPerson") : FPaths::GetBaseFilename(S.CheckpointMap);
    return Current == Owner;
}
