// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TPCGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnEnemyDefeated,
	int32,
	NewDefeatedCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameWon);
UCLASS()
class THIRDPERSON_API ATPCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Per-world policy. Training worlds never read or write the campaign save. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Persistence")
	bool bUsePersistentPlayerSave = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	bool bEnableVictoryProgress = true;
	void RegisterEnemyDefeated();
	void RestoreEnemiesDefeated(int32 SavedDefeatedCount);
	int32 GetEnemiesDefeated() const{return EnemiesDefeated;}
	UPROPERTY(BlueprintAssignable, Category = "Game")
	FOnEnemyDefeated OnEnemyDefeated;
	bool IsGameWon() const{return bGameWon;}
	UPROPERTY(BlueprintAssignable, Category = "Game")
	FOnGameWon OnGameWon;
	void WinGame();
	bool HasMetVictoryKillTarget() const{return EnemiesDefeated >= VictoryKillCount;}
	int32 GetVictoryKillTarget() const{return VictoryKillCount;}
private:
	int32 EnemiesDefeated = 0;
	UPROPERTY(EditDefaultsOnly, Category = "Game",meta = (ClampMin = "1"))
	int32 VictoryKillCount = 5;

	bool bGameWon = false;
};
