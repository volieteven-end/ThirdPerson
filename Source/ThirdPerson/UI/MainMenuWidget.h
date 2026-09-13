#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuWidget.generated.h"
class UButton;
class UTextBlock;
class UBorder;

UCLASS()
class THIRDPERSON_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void StartNewGame();
    UFUNCTION(BlueprintCallable) void ContinueGame();
    UFUNCTION(BlueprintCallable) void QuitGame();
    UFUNCTION(BlueprintCallable) void ConfirmNewGame();
    UFUNCTION(BlueprintCallable) void CancelNewGame();
    static bool ResolveContinueMap(const UObject* Context,FString& Map,FText& Error);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Key) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> StartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> QuitButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ConfirmButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CancelButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> ConfirmPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
private:
    void Select(int32 Index);
    void ShowError(const FText& Error);
    UFUNCTION() void HoverStart();
    UFUNCTION() void HoverContinue();
    UFUNCTION() void HoverQuit();
    int32 Selected=0;
    bool bPending=false,bConfirm=false;
    FDelegateHandle TravelFailureHandle;
};

UCLASS()
class THIRDPERSON_API AMainMenuController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    UPROPERTY(Transient) TObjectPtr<UMainMenuWidget> Menu;
};

UCLASS()
class THIRDPERSON_API AMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMainMenuGameMode();
};
