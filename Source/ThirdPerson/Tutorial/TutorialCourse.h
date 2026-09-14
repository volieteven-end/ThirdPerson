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

/** 单个教程目标的事件类型、目标标识和计数要求，供课程进度规则匹配。 */
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

/** 一节课程的静态说明与目标列表，运行中完成情况存放在进度结构而非资产内。 */
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

/** 课程配置资产，定义课程顺序、目标和返回地图；默认课程同时供编辑器工具和测试使用。 */
UCLASS(BlueprintType)
class THIRDPERSON_API UTutorialCourse : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTutorialLesson> Lessons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UWorld> ReturnMap;
    /** 默认课程由资源工具和确定性测试共用。 */
    static void PopulateDefaults(UTutorialCourse& Course);
    bool IsValidCourse(FString& Error) const;
};

enum class ETutorialProgressResult : uint8 { Ignored, Counted, ObjectiveCompleted, LessonCompleted };

/** 不依赖世界的课程进度规则：计数、去重、重试和解锁；不模拟输入、不修改 Actor、不执行磁盘读写。 */
struct THIRDPERSON_API FTutorialProgress
{
    const UTutorialCourse* Course = nullptr; // 课程对象由当前世界的导演持有。
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
