// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryComponent.h"
#include "../Items/ItemDefinition.h"
#include "HealthComponent.h"
#include "UObject/ConstructorHelpers.h"
// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UItemDefinition> Potion(
		TEXT("/Game/Third/DataAsset/DA_Test0Rb.DA_Test0Rb"));
	HealthPotionDefinition = Potion.Object;
}


// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

bool UInventoryComponent::AddItem(
	UItemDefinition* InItemDefinition,
	int32 Count)
{
	if (!IsValid(InItemDefinition) ||InItemDefinition->ItemId.IsNone() ||Count <= 0 ||InItemDefinition->MaxStack <= 0)
	{
		return false;
	}

	const int32 MaxStack = InItemDefinition->MaxStack;
	int32 RemainingCount = Count;

	for (const FInventorySlot& InventorySlot : Slots)
	{
		if (InventorySlot.ItemDefinition != InItemDefinition ||
			InventorySlot.Count >= MaxStack)
		{
			continue;
		}

		const int32 Space = MaxStack - InventorySlot.Count;
		RemainingCount -= FMath::Min(Space, RemainingCount);

		if (RemainingCount == 0)
		{
			break;
		}
	}

	const int32 NewSlotsNeeded =
		(RemainingCount + MaxStack - 1) / MaxStack;

	if (Slots.Num() + NewSlotsNeeded > MaxSlots)
	{
		return false;
	}

	RemainingCount = Count;

	for (FInventorySlot& InventorySlot : Slots)
	{
		if (InventorySlot.ItemDefinition != InItemDefinition ||
			InventorySlot.Count >= MaxStack)
		{
			continue;
		}

		const int32 Space = MaxStack - InventorySlot.Count;
		const int32 AmountToAdd =
			FMath::Min(Space, RemainingCount);

		InventorySlot.Count += AmountToAdd;
		RemainingCount -= AmountToAdd;

		if (RemainingCount == 0)
		{
			OnInventoryChanged.Broadcast();
			return true;
		}
	}

	while (RemainingCount > 0)
	{
		FInventorySlot& NewSlot = Slots.AddDefaulted_GetRef();

		NewSlot.ItemDefinition = InItemDefinition;
		NewSlot.ItemId = InItemDefinition->ItemId;
		NewSlot.Count = FMath::Min(RemainingCount, MaxStack);

		RemainingCount -= NewSlot.Count;
	}

	OnInventoryChanged.Broadcast();
	return true;
}
void UInventoryComponent::TryUseFirstConsumable()
{
	if (!UseFirstConsumable())
	{
		UE_LOG(LogTemp, Warning, TEXT("No usable consumable"));
	}
}
bool UInventoryComponent::UseFirstConsumable()
{
	// Prefer the potion shown in the quick slot, regardless of inventory ordering.
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].ItemDefinition == HealthPotionDefinition && UseItemAtSlot(Index)) return true;
	}
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (UseItemAtSlot(Index))
		{
			return true;
		}
	}

	return false;
}
bool UInventoryComponent::HasItem(const FName& ItemId) const
{
	if (ItemId.IsNone())
	{
		return false;
	}
	for (const FInventorySlot& InventorySlot : Slots)
	{
		if (InventorySlot.ItemId == ItemId &&InventorySlot.Count > 0)
		{
			return true;
		}
	}
	return false;
}
bool UInventoryComponent::RemoveItem(const FName& ItemId,int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return false;
	}
	int32 TotalCount = 0;
	for (const FInventorySlot& InventorySlot : Slots)
	{
		if (InventorySlot.ItemId == ItemId)
		{
			TotalCount += InventorySlot.Count;
		}
	}
	if (TotalCount < Count)
	{
		return false;
	}
	int32 RemainingToRemove = Count;
	for (int32 Index = Slots.Num() - 1;Index >= 0 && RemainingToRemove > 0;--Index)
	{
		FInventorySlot& InventorySlot = Slots[Index];
		if (InventorySlot.ItemId != ItemId)
		{
			continue;
		}
		const int32 RemoveCount = FMath::Min(InventorySlot.Count,RemainingToRemove);
		InventorySlot.Count -= RemoveCount;
		RemainingToRemove -= RemoveCount;
		if (InventorySlot.Count <= 0)
		{
			Slots.RemoveAt(Index);
		}
	}
	OnInventoryChanged.Broadcast();
	return true;
}
bool UInventoryComponent::UseItemAtSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}
	UHealthComponent* HealthComponent =OwnerActor->FindComponentByClass<UHealthComponent>();
	FInventorySlot& InventorySlot = Slots[SlotIndex];
	UItemDefinition* ItemDefinition =InventorySlot.ItemDefinition;
	if (!HealthComponent ||!ItemDefinition ||InventorySlot.Count <= 0 ||ItemDefinition->ItemType != EItemType::Consumable ||ItemDefinition->HealAmount <= 0.f ||HealthComponent->GetCurrentHealth() <= 0.f ||HealthComponent->GetCurrentHealth() >=HealthComponent->GetMaxHealth())
	{
		return false;
	}
	HealthComponent->Heal(ItemDefinition->HealAmount);
	--InventorySlot.Count;
	if (InventorySlot.Count <= 0)
	{
		Slots.RemoveAt(SlotIndex);
	}
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UInventoryComponent::GetHealthPotionCount() const
{
	int32 Count = 0;
	if (HealthPotionDefinition)
		for (const FInventorySlot& Slot : Slots)
			if (Slot.ItemDefinition == HealthPotionDefinition) Count += FMath::Max(0, Slot.Count);
	return Count;
}

bool UInventoryComponent::RefillHealthPotionsAfterDeath()
{
	const int32 Missing = FMath::Max(0, 2 - GetHealthPotionCount());
	if (!Missing) return true;
	if (!IsValid(HealthPotionDefinition) || HealthPotionDefinition->ItemId.IsNone() ||
		HealthPotionDefinition->MaxStack <= 0 || HealthPotionDefinition->ItemType != EItemType::Consumable ||
		HealthPotionDefinition->HealAmount <= 0.f) return false;

	int32 Remaining = Missing;
	for (const FInventorySlot& Slot : Slots)
		if (Slot.ItemDefinition == HealthPotionDefinition)
			Remaining -= FMath::Min(Remaining, FMath::Max(0, HealthPotionDefinition->MaxStack - Slot.Count));
	const int32 ExtraSlots = FMath::DivideAndRoundUp(Remaining, HealthPotionDefinition->MaxStack);
	// Death recovery must not evict loot from a full bag. Extend only as far as needed.
	MaxSlots = FMath::Max(MaxSlots, Slots.Num() + ExtraSlots);
	return AddItem(HealthPotionDefinition, Missing);
}

void UInventoryComponent::RestoreRespawnInventory(const UInventoryComponent& Source)
{
	Slots = Source.Slots;
	MaxSlots = Source.MaxSlots;
	HealthPotionDefinition = Source.HealthPotionDefinition;
	ensureMsgf(RefillHealthPotionsAfterDeath(), TEXT("Respawn health potion definition must be valid"));
	OnInventoryChanged.Broadcast();
}
