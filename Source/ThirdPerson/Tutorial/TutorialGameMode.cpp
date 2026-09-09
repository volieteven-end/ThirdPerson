#include "TutorialGameMode.h"
#include "TutorialDirector.h"
#include "TutorialWidgets.h"
#include "../Character/TPCCharacter.h"
#include "../UI/InventroyWidget.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

ATutorialPlayerController::ATutorialPlayerController()
{
    static ConstructorHelpers::FClassFinder<ATPCPlayerController> Template(TEXT("/Game/Third/Character/BP_TPCPlayerController"));
    if (Template.Class)
        if (auto* Property = FindFProperty<FClassProperty>(ATPCPlayerController::StaticClass(), TEXT("InventoryWidgetClass")))
            InventoryWidgetClass = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Template.Class->GetDefaultObject()));
    PauseMenuWidgetClass = UTutorialPauseWidget::StaticClass();
}
ATutorialGameMode::ATutorialGameMode()
{
    bUsePersistentPlayerSave = false; bEnableVictoryProgress = false;
    static ConstructorHelpers::FClassFinder<ATPCCharacter> Pawn(TEXT("/Game/Third/Character/BP_TPCCharacter"));
    DefaultPawnClass = Pawn.Class;
    PlayerControllerClass = ATutorialPlayerController::StaticClass();
}
AActor* ATutorialGameMode::FindPlayerStart_Implementation(AController* PC, const FString& IncomingName)
{
    const auto* D = ATutorialDirector::Find(this);
    const FName Tag(*FString::Printf(TEXT("Tutorial_%d"), D ? D->GetCheckpointIndex() : 0));
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) if (It->PlayerStartTag == Tag) return *It;
    return Super::FindPlayerStart_Implementation(PC,IncomingName);
}
void ATutorialGameMode::RestartPlayer(AController* PC)
{
    Super::RestartPlayer(PC);
    if (PC) if (auto* P = Cast<ATPCCharacter>(PC->GetPawn()))
        if (auto* D = ATutorialDirector::Find(this)) D->PlayerRestarted(P);
}
