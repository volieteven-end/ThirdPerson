#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIController.h"
#include "../AI/EnemyCharacter.h"
#include "TutorialCourse.h"
#include "TutorialActors.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWeaponDefinition;
class ATutorialDirector;

UCLASS()
class THIRDPERSON_API ATutorialZone : public AActor
{
    GENERATED_BODY()
public:
    ATutorialZone();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Marker;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Number;
    UPROPERTY(EditAnywhere, Category="Tutorial") int32 LessonIndex = 0;
    UPROPERTY(EditAnywhere, Category="Tutorial") bool bEntry = false;
    UPROPERTY(EditAnywhere, Category="Tutorial") ETutorialSignal Signal = ETutorialSignal::WalkMarker;
    UPROPERTY(EditAnywhere, Category="Tutorial") FName TargetId;
    UPROPERTY(EditAnywhere, Category="Tutorial") FTransform Checkpoint;
    UPROPERTY(EditAnywhere, Category="Tutorial") FTransform TargetSpawn;
    bool Contains(const FVector& Location) const;
    void SetHighlighted(bool Active);
protected:
    UFUNCTION() void Enter(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
};

UCLASS()
class THIRDPERSON_API ATutorialGate : public AActor
{
    GENERATED_BODY()
public:
    ATutorialGate();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bars;
    UPROPERTY(EditAnywhere, Category="Tutorial") int32 UnlockLesson = 1;
    void SetOpen(bool Open);
};

UENUM()
enum class ETutorialEnemyMode : uint8 { Passive, Guard, Combat };

UCLASS()
class THIRDPERSON_API ATutorialTrainingEnemy : public AEnemyCharacter
{
    GENERATED_BODY()
public:
    ATutorialTrainingEnemy();
    UPROPERTY(EditAnywhere, Category="Tutorial") ETutorialEnemyMode TrainingMode = ETutorialEnemyMode::Passive;
    UPROPERTY(EditAnywhere, Category="Tutorial") FName TrainingId;
    UPROPERTY(EditAnywhere, Category="Tutorial") TObjectPtr<UWeaponDefinition> TrainingWeapon;
    TWeakObjectPtr<ATutorialDirector> Director;
    bool IsThreatening() const;
protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;
};

/** Bounded, single-target training AI. No campaign behavior tree or attack reservations. */
UCLASS()
class THIRDPERSON_API ATutorialTrainingController : public AAIController
{
    GENERATED_BODY()
public:
    ATutorialTrainingController();
    virtual void Tick(float DeltaSeconds) override;
    bool bTelegraphing = false;
private:
    double NextAttack = 0.;
    double LastMoveRequest = 0.;
};
