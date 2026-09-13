#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "ArenaActors.generated.h"
class UBoxComponent;
class AEnemyCharacter;

UCLASS()
class THIRDPERSON_API AArenaBounds : public AActor
{
    GENERATED_BODY()
public:
    AArenaBounds();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena") TObjectPtr<UBoxComponent> Bounds;
    bool ContainsLocation(const FVector& Position, float Margin = 0.f) const;
    bool ContainsActor(const AActor* Actor) const;
    FVector ClampToInterior(const FVector& Position, float Margin = 100.f) const;
};

UCLASS()
class THIRDPERSON_API AArenaPortal : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AArenaPortal();
    UPROPERTY(VisibleAnywhere, Category="Travel") TObjectPtr<UBoxComponent> InteractionBox;
    UPROPERTY(EditAnywhere, Category="Travel") TSoftObjectPtr<UWorld> Destination;
    UPROPERTY(EditAnywhere, Category="Travel") FName ArrivalTag = TEXT("ArenaArrival");
    UPROPERTY(EditAnywhere, Category="Travel") FText Label;
    virtual void Interact(APawn* InstigatorPawn) override;
    virtual FText GetInteractionText() const override { return Label; }
    FString GetDestinationMap() const;
};

UCLASS()
class THIRDPERSON_API AArenaWaveDirector : public AActor
{
    GENERATED_BODY()
public:
    AArenaWaveDirector();
    UPROPERTY(EditInstanceOnly, Category="Arena") TObjectPtr<AArenaBounds> Arena;
    UPROPERTY(EditAnywhere, Category="Arena") TSubclassOf<AEnemyCharacter> MeleeClass;
    UPROPERTY(EditAnywhere, Category="Arena") TSubclassOf<AEnemyCharacter> RangedClass;
    UPROPERTY(EditAnywhere, Category="Arena", meta=(ClampMin="0.1")) float WaveDelay = 3.f;
    UPROPERTY(EditAnywhere, Category="Arena", meta=(ClampMin="200")) float PlayerSpawnClearance = 700.f;
    UPROPERTY(EditAnywhere, Category="Arena") int32 RandomSeed = 0; // Zero selects a new session seed.
    int32 GetWave() const { return Wave; }
    int32 GetRemaining() const;
    FText GetStatusText() const;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FArenaTestAccess;
    UPROPERTY(Transient) TArray<TObjectPtr<AEnemyCharacter>> Enemies;
    TArray<bool> PendingRanged;
    FRandomStream Random;
    int32 Wave = 0;
    double NextWaveAt = -1.;
    double NextSaveAttempt = 0.;
    bool bStarted = false, bDeathPending = false, bCompletedWaveSaved = true;
    void StartWave();
    bool SpawnOne(bool bRanged);
    UFUNCTION() void OnEnemyDeath();
};
