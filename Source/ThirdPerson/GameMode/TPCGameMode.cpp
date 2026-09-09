// Fill out your copyright notice in the Description page of Project Settings.


#include "TPCGameMode.h"
void ATPCGameMode::RegisterEnemyDefeated()
{
	if (!bEnableVictoryProgress || bGameWon)
	{
		return;
	}

	++EnemiesDefeated;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Enemies defeated: %d"),
		EnemiesDefeated);

	OnEnemyDefeated.Broadcast(EnemiesDefeated);

	if (EnemiesDefeated >= VictoryKillCount)
	{
		UE_LOG(LogTemp, Warning,TEXT("Exit unlocked! Go to the exit."));
	}
}
void ATPCGameMode::RestoreEnemiesDefeated(int32 SavedDefeatedCount)
{
	if (!bEnableVictoryProgress) return;
	EnemiesDefeated =FMath::Max(0, SavedDefeatedCount);
	OnEnemyDefeated.Broadcast(EnemiesDefeated);
	if (EnemiesDefeated >= VictoryKillCount)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Exit restored as unlocked"));
	}
}
void ATPCGameMode::WinGame()
{
	if (!bEnableVictoryProgress || bGameWon)
	{
		return;
	}
	bGameWon = true;

	UE_LOG(LogTemp, Warning, TEXT("Victory!"));

	OnGameWon.Broadcast();
}
