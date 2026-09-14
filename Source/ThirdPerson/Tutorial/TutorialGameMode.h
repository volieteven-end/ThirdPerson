#pragma once
#include "CoreMinimal.h"
#include "../GameMode/TPCGameMode.h"
#include "../Character/TPCPlayerController.h"
#include "TutorialGameMode.generated.h"

/** 教程玩家控制器，在通用输入和界面管理基础上接入教程提示及暂停菜单。 */
UCLASS()
class THIRDPERSON_API ATutorialPlayerController : public ATPCPlayerController
{
    GENERATED_BODY()
public:
    ATutorialPlayerController();
};

/** 教程场景规则，使用训练角色配置与课程导演，避免训练进度覆盖正式角色进度。 */
UCLASS()
class THIRDPERSON_API ATutorialGameMode : public ATPCGameMode
{
    GENERATED_BODY()
public:
    ATutorialGameMode();
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
};
