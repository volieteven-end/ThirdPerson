#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../UI/PauseMenuWidget.h"
#include "TutorialWidgets.generated.h"

class ATutorialDirector;

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
