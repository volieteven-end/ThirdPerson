#include "LevelExitActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "../Character/TPCCharacter.h"
#include "../GameMode/TPCGameMode.h"
ALevelExitActor::ALevelExitActor()
{
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 120.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	ExitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMesh"));
	ExitMesh->SetupAttachment(TriggerBox);
	ExitMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExitMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}


void ALevelExitActor::BeginPlay()
{
	Super::BeginPlay();
	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->OnEnemyDefeated.AddDynamic(this,&ThisClass::HandleEnemyDefeated);
		bIsUnlocked = !bRequireKillTarget ||GameMode->HasMetVictoryKillTarget();
		SetExitUnlockedVisual(bIsUnlocked);
	}
}

void ALevelExitActor::HandleEnemyDefeated(int32 NewDefeatedCount)
{
	ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		return;
	}
	if (!bIsUnlocked &&(!bRequireKillTarget ||NewDefeatedCount >=GameMode->GetVictoryKillTarget()))
	{
		bIsUnlocked = true;
		SetExitUnlockedVisual(true);
		UE_LOG(LogTemp, Warning,
			TEXT("Exit unlocked"));
	}
}
FText ALevelExitActor::GetInteractionText() const
{
	if (!bIsUnlocked)
	{
		return FText::FromString(TEXT("Exit locked"));
	}

	return FText::FromString(TEXT("Leave level"));
}

void ALevelExitActor::Interact(APawn* InstigatorPawn)
{
	if (!bIsUnlocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("Exit locked"));
		return;
	}

	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		OnPlayerExit();
		GameMode->WinGame();
	}
}