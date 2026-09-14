#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../UI/PauseMenuWidget.h"
#include "TutorialWidgets.generated.h"

class ATutorialDirector;

/** 教程目标与课程进度显示，只观察课程导演，不通过模拟输入推进课程。 */
UCLASS()
class THIRDPERSON_API UTutorialHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<ATutorialDirector> Director;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText Heading() const;
    FText Instruction() const;
    FText Keys() const;
    FText Progress() const;
    FText Status() const;
    FText Direction() const;
};

/** 教程暂停界面，在通用暂停菜单基础上提供课程重试与练习入口。 */
UCLASS()
class THIRDPERSON_API UTutorialPauseWidget : public UPauseMenuWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText Title() const;
    FText Summary() const;
    FReply Continue();
    FReply Retry();
    FReply Skip();
    FReply Restart();
    FReply Leave();
};
