#include "ArenaTravelSubsystem.h"
#include "ArenaGameMode.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../Save/TPCPlayerProgress.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Tutorial/TutorialDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Misc/PackageName.h"

const TCHAR* UArenaTravelSubsystem::TutorialMap() { return TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial"); }
const TCHAR* UArenaTravelSubsystem::BossMap() { return TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"); }
const TCHAR* UArenaTravelSubsystem::WaveMap() { return TEXT("/Game/Third/Arenas/Maps/L_RandomArena"); }
void UArenaTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine) TravelFailureHandle = GEngine->OnTravelFailure().AddWeakLambda(this,
        [this](UWorld* World, ETravelFailure::Type, const FString& Reason)
        {
            if (World != GetWorld() || !bTravelPending) return;
            bTravelPending = false; PendingMap.Reset(); PendingStart = NAME_None; bResumeTutorial = false;
            ReportFailure(FText::FromString(TEXT("地图传送失败，可重新交互：") + Reason));
        });
}
void UArenaTravelSubsystem::Deinitialize()
{
    if (GEngine) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    Super::Deinitialize();
}
UArenaTravelSubsystem* UArenaTravelSubsystem::Get(const UObject* Context)
{
    auto* GI = Context ? UGameplayStatics::GetGameInstance(Context) : nullptr;
    return GI ? GI->GetSubsystem<UArenaTravelSubsystem>() : nullptr;
}
bool UArenaTravelSubsystem::IsArena(const UObject* Context)
{
    return Context && Context->GetWorld() && Context->GetWorld()->GetAuthGameMode<AArenaGameMode>() != nullptr;
}
bool UArenaTravelSubsystem::LoadFormal()
{
    if (Formal) return true;
    const FString Slot = TPCSaveSlots::Resolve();
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        Formal = Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        bFormalInitialized = Formal != nullptr;
        if (!Formal) ReportFailure(FText::FromString(TEXT("正式存档读取失败，未开始传送。")));
        return Formal != nullptr;
    }
    Formal = NewObject<UTPCSaveGame>(this);
    bFormalInitialized = false;
    Formal->bHasCheckpoint = false;
    return true;
}
void UArenaTravelSubsystem::ReportFailure(const FText& Message) const
{
    LastError=Message; ErrorUntil=FPlatformTime::Seconds()+8.;
    UE_LOG(LogTemp, Error, TEXT("Arena: %s"), *Message.ToString());
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, Message.ToString());
}
FText UArenaTravelSubsystem::GetErrorText() const
{ return FPlatformTime::Seconds() < ErrorUntil ? LastError : FText::GetEmpty(); }
bool UArenaTravelSubsystem::SaveFormal(ATPCCharacter* Player, bool bRestoreVitals)
{
    if (!Player || ATutorialDirector::Find(Player)) return false;
    if (!LoadFormal()) return false;
    // Checkpoints may have been updated since the last arena visit; merge portable progress only.
    if (UGameplayStatics::DoesSaveGameExist(TPCSaveSlots::Resolve(), 0))
    {
        auto* Disk = Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(TPCSaveSlots::Resolve(), 0));
        if (!Disk) { ReportFailure(FText::FromString(TEXT("存档读取失败，未覆盖原存档。"))); return false; }
        Formal = Disk;
    }
    auto* Candidate = DuplicateObject<UTPCSaveGame>(Formal, this);
    TPCPlayerProgress::Capture(*Player, *Candidate, bRestoreVitals);
    if (!UGameplayStatics::SaveGameToSlot(Candidate, TPCSaveSlots::Resolve(), 0))
    { ReportFailure(FText::FromString(TEXT("正式进度保存失败，请检查磁盘后重试。"))); return false; }
    Formal = Candidate; bFormalInitialized = true;
    return true;
}
bool UArenaTravelSubsystem::InitializeArenaPlayer(ATPCCharacter& Player)
{
    if (!IsArena(&Player)) return false;
    if (!LoadFormal()) return true; // Never fall through to legacy checkpoint teleport.
    if (bFormalInitialized) TPCPlayerProgress::Apply(Player, *Formal);
    else { TPCPlayerProgress::Capture(Player, *Formal); bFormalInitialized = true; }
    return true;
}
bool UArenaTravelSubsystem::Travel(ATPCCharacter* Player, const FString& InMap, FName Start, bool bRetry)
{
    const FString Map=UWorld::RemovePIEPrefix(InMap);
    if (!Player || bTravelPending || (!bRetry && Player->HealthComponent->GetCurrentHealth() <= 0)) return false;
    if (!FPackageName::IsValidLongPackageName(Map) || !FPackageName::DoesPackageExist(Map))
    { ReportFailure(FText::FromString(TEXT("目标地图不存在，未开始传送。"))); return false; }
    auto* Director = ATutorialDirector::Find(Player);
    if (Director)
    {
        // Reload the disk profile, never serialize the tutorial's temporary bag/weapon.
        Formal = nullptr;
        if (!LoadFormal()) return false;
        Tutorial = Director->ExportTravelSnapshot();
    }
    else if (!SaveFormal(Player, bRetry)) return false;
    PendingMap = Map; PendingStart = Start; bTravelPending = true;
    bResumeTutorial = Map == TutorialMap() && !Tutorial.CoursePath.IsEmpty();
    UGameplayStatics::OpenLevel(Player, FName(*Map));
    return true;
}
FName UArenaTravelSubsystem::GetArrivalTag(const UObject* Context) const
{
    return bTravelPending && UGameplayStatics::GetCurrentLevelName(Context, true) == FPaths::GetBaseFilename(PendingMap)
        ? PendingStart : NAME_None;
}
void UArenaTravelSubsystem::ConsumeArrival(const UObject* Context)
{
    if (!GetArrivalTag(Context).IsNone()) { bTravelPending = false; PendingStart = NAME_None; PendingMap.Reset(); }
}
bool UArenaTravelSubsystem::RestoreTutorial(ATutorialDirector& Director)
{
    if (!bResumeTutorial) return false;
    bResumeTutorial = false;
    return Director.ImportTravelSnapshot(Tutorial);
}
