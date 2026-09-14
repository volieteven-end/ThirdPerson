#include "CheckpointActor.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TPCSaveGame.h"
#include "TPCSaveSlots.h"
#include "TPCPlayerProgress.h"
#include "../Character/TPCCharacter.h"
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
	if (const ATPCGameMode* Mode = GetWorld()->GetAuthGameMode<ATPCGameMode>())
	{
		if (!Mode->bUsePersistentPlayerSave) return;
	}
	UTPCSaveGame* SaveGame =Cast<UTPCSaveGame>(UGameplayStatics::CreateSaveGameObject(UTPCSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return;
	}
	SaveGame->PlayerTransform =RespawnPoint->GetComponentTransform();
	TPCPlayerProgress::Capture(*Player, *SaveGame);
	SaveGame->bHasCheckpoint = true;
	SaveGame->CheckpointMap = UGameplayStatics::GetCurrentLevelName(this, true);
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
