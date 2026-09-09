#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TutorialCourse.generated.h"

UENUM(BlueprintType)
enum class ETutorialSignal : uint8
{
    WalkMarker, SprintMarker, JumpLanding, SwordHit, GroundCombo,
    ForwardDodge, SideDodge, Blocked, Parried, PotionUsed, EnemyDefeated,
    RisingHit, AirPair, DiveHit, AirChain
};

UENUM(BlueprintType)
enum class ETutorialLessonStatus : uint8 { Locked, Available, Completed, Skipped };

USTRUCT(BlueprintType)
struct FTutorialObjective
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ETutorialSignal Signal = ETutorialSignal::WalkMarker;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Keys;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TargetId;
};

USTRUCT(BlueprintType)
struct FTutorialLesson
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Introduction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOptional = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTutorialObjective> Objectives;
};

UCLASS(BlueprintType)
class THIRDPERSON_API UTutorialCourse : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTutorialLesson> Lessons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UWorld> ReturnMap;
    /** Default curriculum is shared by the asset builder and deterministic tests. */
    static void PopulateDefaults(UTutorialCourse& Course);
    bool IsValidCourse(FString& Error) const;
};

enum class ETutorialProgressResult : uint8 { Ignored, Counted, ObjectiveCompleted, LessonCompleted };

/** Pure progress rules: no input simulation, actor mutation, disk IO or timers. */
struct THIRDPERSON_API FTutorialProgress
{
    const UTutorialCourse* Course = nullptr; // Owned by the world's director.
    TArray<ETutorialLessonStatus> Status;
    int32 Lesson = INDEX_NONE;
    int32 Objective = 0;
    int32 Count = 0;
    bool bPractice = false;
    TSet<FString> Seen;

    void Initialize(const UTutorialCourse* InCourse);
    bool StartLesson(int32 Index, bool bInPractice = false);
    void Retry();
    void Skip();
    bool BasicsFinished() const;
    bool IsUnlocked(int32 Index) const;
    const FTutorialObjective* CurrentObjective() const;
    ETutorialProgressResult Observe(ETutorialSignal Signal, uint64 Serial, FName TargetId);
    void Finish(bool bSkipped);
};
