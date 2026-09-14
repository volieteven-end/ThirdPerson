#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialCourse.h"
#include "../Components/CombatHitTypes.h"
#include "../Actions/ActionDefinition.h"
#include "TutorialDirector.generated.h"

class ATPCCharacter;
class ATutorialZone;
class ATutorialTrainingEnemy;
class UTutorialHUDWidget;
class UItemDefinition;
class UWeaponDefinition;
struct FTutorialTravelSnapshot;

/** 教程运行协调器：连接训练区域、陪练和真实战斗事件，持有课程进度并处理重试、练习及跨地图恢复。 */
UCLASS()
class THIRDPERSON_API ATutorialDirector : public AActor
{
    GENERATED_BODY()
public:
    ATutorialDirector();
    static ATutorialDirector* Find(const UObject* Context);
    UPROPERTY(EditAnywhere, Category="Tutorial") TObjectPtr<UTutorialCourse> Course;
    UPROPERTY(EditAnywhere, Category="Tutorial") TObjectPtr<UWeaponDefinition> PlayerWeapon;
    UPROPERTY(EditAnywhere, Category="Tutorial") TObjectPtr<UWeaponDefinition> EnemyWeapon;
    UFUNCTION(BlueprintCallable, Category="Tutorial") void RetryLesson();
    UFUNCTION(BlueprintCallable, Category="Tutorial") void SkipLesson();
    UFUNCTION(BlueprintCallable, Category="Tutorial") void RestartCourse();
    UFUNCTION(BlueprintCallable, Category="Tutorial") void ReturnToCampaign();
    UFUNCTION(BlueprintCallable, Category="Tutorial") void ContinuePractice();
    void HandleZone(ATutorialZone* Zone, ATPCCharacter* Pawn);
    void TrainingEnemyDefeated(ATutorialTrainingEnemy* Enemy);
    void PlayerRestarted(ATPCCharacter* Pawn);
    int32 GetCheckpointIndex() const;
    FTransform GetCheckpoint() const;
    FText GetHeading() const;
    FText GetInstruction() const;
    FText GetKeys() const;
    FText GetProgressText() const;
    FText GetStatusText() const;
    FText GetCourseSummary() const;
    bool IsComplete() const { return Progress.BasicsFinished(); }
    bool IsSummaryOpen() const { return bSummaryPending && Progress.BasicsFinished(); }
    const FTutorialProgress& GetProgress() const { return Progress; }
    FTutorialTravelSnapshot ExportTravelSnapshot() const;
    bool ImportTravelSnapshot(const FTutorialTravelSnapshot& Snapshot);
    ATutorialTrainingEnemy* GetTrainingTarget() const { return Target.Get(); }
    FVector GetGuidanceLocation() const;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FTutorialTestAccess;
    FTutorialProgress Progress;
    UPROPERTY(Transient) TObjectPtr<UTutorialHUDWidget> HUD;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATutorialTrainingEnemy> Target;
    TArray<TWeakObjectPtr<ATutorialZone>> Zones;
    TWeakObjectPtr<ATutorialZone> PendingLandingZone;
    bool bInitialized = false, bEntered = false, bRefreshPending = false, bSummaryPending = false;
    bool bRespawnInitializing = false;
    bool bWasFalling = false, bSawJump = false, bForwardDodge = false, bSideDodge = false;
    int32 LastLesson = 0, GroundNext = 0, AirNext = 0, ChainPhase = 0;
    uint64 EventSerial = 0, LastTargetAction = 0, LastDodgeAction = 0;
    double LastDodgeTime = -100., LastLandingTime = -100., LastTargetHitTime = -100., SpawnAfter = -1., SummaryAfter = -1.;
    FVector DodgeStart = FVector::ZeroVector;
    FText Toast;
    double ToastUntil = -1.;

    ATutorialZone* EntryFor(int32 Index) const;
    void BindPlayer(ATPCCharacter* Pawn);
    void UnbindPlayer();
    void InitializePlayer();
    void EnterCurrentLesson();
    void SpawnTarget();
    void ClearTarget();
    void RefreshWorld();
    void ResetActionTracking();
    void RespawnAtCheckpoint();
    void CloseMenu();
    void ShowToast(const FText& Text);
    void SendSignal(ETutorialSignal Signal, uint64 Serial, FName TargetId = NAME_None);
    void OnPlayback(uint64 Instance, ETPCActionState State, const UActionDefinition* Definition);
    void OnTargetHit(const FCombatHitSpec&, const FCombatHitResult&, AActor*, TWeakObjectPtr<ATutorialTrainingEnemy>);
    void OnPlayerHit(const FCombatHitSpec&, const FCombatHitResult&, AActor*);
    void OnPotionUsed(UItemDefinition*, float Healing, int32 Remaining);
};
