#include "CheckpointActor.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TPCSaveGame.h"
#include "TPCSaveSlots.h"
#include "../Items/ItemDefinition.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/LevelComponent.h"
#include "../GameMode/TPCGameMode.h"
#include "../World/DoorActor.h"
#include "EngineUtils.h"
ACheckpointActor::ACheckpointActor()
{
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(
		TEXT("TriggerBox"));

	SetRootComponent(TriggerBox);
	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(TriggerBox);
	RespawnPoint->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 120.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this,&ThisClass::HandlePlayerEnter);
}

void ACheckpointActor::HandlePlayerEnter(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ATPCCharacter* Player =Cast<ATPCCharacter>(OtherActor);

	if (!Player)
	{
		return;
	}
	UTPCSaveGame* SaveGame =Cast<UTPCSaveGame>(UGameplayStatics::CreateSaveGameObject(UTPCSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return;
	}
	SaveGame->PlayerTransform =RespawnPoint->GetComponentTransform();
	if (UHealthComponent* Health =Player->FindComponentByClass<UHealthComponent>())
	{
		SaveGame->PlayerHealth =Health->GetCurrentHealth();
	}
	if (UStaminaComponent* Stamina =Player->FindComponentByClass<UStaminaComponent>())
	{
		SaveGame->PlayerStamina =Stamina->GetCurrentStamina();
	}
	if (ULevelComponent* Level = Player->FindComponentByClass<ULevelComponent>())
	{
		SaveGame->PlayerLevel = Level->GetLevel();
		SaveGame->PlayerExperience = Level->GetCurrentExperience();
		SaveGame->PlayerUpgrades.Reset();
		for (const ELevelUpgradeType Upgrade : Level->GetSelectedUpgrades())
		{
			SaveGame->PlayerUpgrades.Add(static_cast<uint8>(Upgrade));
		}
	}
	SaveGame->InventorySlots.Reset();
	if (UInventoryComponent* Inventory =Player->FindComponentByClass<UInventoryComponent>())
	{
		SaveGame->InventoryCapacity = Inventory->MaxSlots;
		for (const FInventorySlot& Slot : Inventory->Slots)
		{
			if (!Slot.ItemDefinition || Slot.Count <= 0)
			{
				continue;
			}
			FSaveInventorySlot& SavedSlot =SaveGame->InventorySlots.AddDefaulted_GetRef();
			SavedSlot.ItemDefinition = Slot.ItemDefinition;
			SavedSlot.Count = Slot.Count;
		}
	}
	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		SaveGame->EnemiesDefeated =GameMode->GetEnemiesDefeated();
	}
	SaveGame->DoorStates.Reset();
	for (TActorIterator<ADoorActor> It(GetWorld()); It; ++It)
	{
		ADoorActor* Door = *It;

		if (!Door || Door->GetSaveId().IsNone())
		{
			continue;
		}

		FSaveDoorState& SavedDoor =SaveGame->DoorStates.AddDefaulted_GetRef();

		SavedDoor.SaveId = Door->GetSaveId();
		SavedDoor.bIsOpen = Door->IsOpen();
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame,TPCSaveSlots::Resolve(SaveSlotName),0);
	if (bSaved)
	{
		bIsActivated = true;
		OnCheckpointActivated();
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Checkpoint saved: %s"),
		bSaved ? TEXT("Success") : TEXT("Failed"));
}
