#include "ArenaGameMode.h"
#include "ArenaActors.h"
#include "ArenaTravelSubsystem.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Components/HealthComponent.h"
#include "../Boss/CountessBossCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UArenaHUDWidget::RebuildWidget()
{
    TWeakObjectPtr<AArenaWaveDirector> Director;
    TWeakObjectPtr<UArenaTravelSubsystem> Travel=UArenaTravelSubsystem::Get(this);
    for (TActorIterator<AArenaWaveDirector> It(GetWorld()); It; ++It) { Director=*It; break; }
    return SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(20,90)
        [SNew(SBorder).Padding(16).BorderBackgroundColor(FLinearColor(0.015f,0.025f,0.04f,.8f))
            [SNew(STextBlock).Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(.9f,.94f,1.f))
                .Text_Lambda([Director,Travel]() { if (Travel.IsValid() && !Travel->GetErrorText().IsEmpty()) return Travel->GetErrorText(); return Director.IsValid() ? Director->GetStatusText() :
                    FText::FromString(TEXT("Countess 挑战 · 走入场地开始\n入口处按 E 返回教程；再次进入可重新挑战")); })]];
}
AArenaGameMode::AArenaGameMode()
{
    bUsePersistentPlayerSave = true; bEnableVictoryProgress = false;
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 1.f;
    static ConstructorHelpers::FClassFinder<ATPCCharacter> Pawn(TEXT("/Game/Third/Character/BP_TPCCharacter"));
    static ConstructorHelpers::FClassFinder<ATPCPlayerController> Controller(TEXT("/Game/Third/Character/BP_TPCPlayerController"));
    DefaultPawnClass = Pawn.Class; PlayerControllerClass = Controller.Class;
}
AActor* AArenaGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    FName Tag = TEXT("ArenaArrival");
    if (auto* Travel = UArenaTravelSubsystem::Get(this))
        if (const FName Arrival = Travel->GetArrivalTag(this); !Arrival.IsNone()) Tag = Arrival;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) if (It->PlayerStartTag == Tag) return *It;
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}
void AArenaGameMode::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ACountessBossCharacter> It(GetWorld()); It; ++It) { Boss=*It; break; }
    GetWorldTimerManager().SetTimerForNextTick([this]()
    {
        if (auto* PC = UGameplayStatics::GetPlayerController(this,0))
        {
            ArenaHUD = CreateWidget<UArenaHUDWidget>(PC,UArenaHUDWidget::StaticClass());
            if (ArenaHUD) { ArenaHUD->SetVisibility(ESlateVisibility::HitTestInvisible); ArenaHUD->AddToViewport(12); }
        }
    });
}
void AArenaGameMode::Tick(float Delta)
{
    Super::Tick(Delta);
    // The authored courts are at Z=0. Falling off an edge uses the same death/retry flow.
    if (auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); P && P->GetActorLocation().Z < -800.f && P->HealthComponent->GetCurrentHealth()>0.f)
        P->HealthComponent->ApplyDamage(1000000.f);
    if (bBossRewardSaved || !Boss.IsValid()) return;
    if (auto* H = Boss->FindComponentByClass<UHealthComponent>(); H && H->GetCurrentHealth() <= 0)
        if (auto* Travel = UArenaTravelSubsystem::Get(this))
            bBossRewardSaved = Travel->SaveFormal(Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0)));
}
