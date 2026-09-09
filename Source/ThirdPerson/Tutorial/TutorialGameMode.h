#pragma once
#include "CoreMinimal.h"
#include "../GameMode/TPCGameMode.h"
#include "../Character/TPCPlayerController.h"
#include "TutorialGameMode.generated.h"

UCLASS()
class THIRDPERSON_API ATutorialPlayerController : public ATPCPlayerController
{
    GENERATED_BODY()
public:
    ATutorialPlayerController();
};

UCLASS()
class THIRDPERSON_API ATutorialGameMode : public ATPCGameMode
{
    GENERATED_BODY()
public:
    ATutorialGameMode();
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
};
