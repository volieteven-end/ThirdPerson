#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArenaTravelSubsystem.generated.h"
class ATPCCharacter;
class UTPCSaveGame;
class ATutorialDirector;

/** 跨地图暂存的教程进度，仅保存可恢复的值数据；不携带旧世界 Actor 或正在播放的动作。 */
USTRUCT()
struct FTutorialTravelSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FString CoursePath;
    UPROPERTY() TArray<uint8> Status;
    UPROPERTY() int32 Lesson = INDEX_NONE;
    UPROPERTY() int32 Objective = 0;
    UPROPERTY() int32 Count = 0;
    UPROPERTY() int32 LastLesson = 0;
    UPROPERTY() bool bPractice = false;
};

/** 游戏会话级地图交接：分离训练状态和正式进度，先校验并保存再传送，防止重复交互和坐标串图。 */
UCLASS()
class THIRDPERSON_API UArenaTravelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static UArenaTravelSubsystem* Get(const UObject* Context);
    static const TCHAR* TutorialMap();
    static const TCHAR* BossMap();
    static const TCHAR* WaveMap();
    static bool IsArena(const UObject* Context);
    void ResetSession();
    bool Travel(ATPCCharacter* Player, const FString& Map, FName Start, bool bRetry = false);
    bool SaveFormal(ATPCCharacter* Player, bool bRestoreVitals = false);
    bool InitializeArenaPlayer(ATPCCharacter& Player);
    FName GetArrivalTag(const UObject* Context) const;
    void ConsumeArrival(const UObject* Context);
    bool RestoreTutorial(ATutorialDirector& Director);
    void ReportFailure(const FText& Message) const;
    FText GetErrorText() const;
private:
    UPROPERTY(Transient) TObjectPtr<UTPCSaveGame> Formal;
    UPROPERTY(Transient) FTutorialTravelSnapshot Tutorial;
    FString PendingMap;
    FName PendingStart;
    bool bTravelPending = false;
    bool bResumeTutorial = false;
    bool bFormalInitialized = false;
    mutable FText LastError;
    mutable double ErrorUntil = 0.;
    FDelegateHandle TravelFailureHandle;
    bool LoadFormal();
};
