#include "EnemySpawner.h"
#include "EnemyCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../GameMode/TPCGameMode.h"
AEnemySpawner::AEnemySpawner()
{
	SceneRoot =CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	SpawnEnemy();
}

void AEnemySpawner::SpawnEnemy()
{
	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GameMode->IsGameWon())
		{
			return;
		}
	}
	if (CurrentEnemy || !EnemyClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;

	SpawnParams.SpawnCollisionHandlingOverride =ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	CurrentEnemy = GetWorld()->SpawnActor<AEnemyCharacter>(EnemyClass,GetActorLocation(),GetActorRotation(),SpawnParams);

	if (CurrentEnemy)
	{
		CurrentEnemy->OnDestroyed.AddDynamic(this,&ThisClass::HandleEnemyDestroyed);
	}
}

void AEnemySpawner::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	CurrentEnemy = nullptr;

	GetWorldTimerManager().SetTimer(RespawnTimerHandle,this,&ThisClass::SpawnEnemy,RespawnDelay,false);
}