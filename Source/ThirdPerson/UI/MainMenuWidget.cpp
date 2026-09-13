#include "MainMenuWidget.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../Arena/ArenaTravelSubsystem.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Misc/PackageName.h"
#include "InputCoreTypes.h"

bool UMainMenuWidget::ResolveContinueMap(const UObject*,FString& Map,FText& Error)
{
    const FString Slot=TPCSaveSlots::Resolve();
    if (!UGameplayStatics::DoesSaveGameExist(Slot,0)) { Error=FText::FromString(TEXT("尚无正式存档，请开始游戏。")); return false; }
    const auto* Save=Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    if (!Save) { Error=FText::FromString(TEXT("存档无法读取。原文件未修改，可选择重新开始。")); return false; }
    Map=UArenaTravelSubsystem::TutorialMap();
    if (Save->bHasCheckpoint)
    {
        const FString Owner=Save->CheckpointMap.IsEmpty()?TEXT("Lvl_ThirdPerson"):Save->CheckpointMap;
        if (FPackageName::IsValidLongPackageName(Owner)) Map=Owner;
        else if (!FPackageName::SearchForPackageOnDisk(Owner,&Map))
        { Error=FText::FromString(TEXT("存档所属地图不存在，未开始加载。")); return false; }
    }
    FString Filename;
    if (!FPackageName::IsValidLongPackageName(Map) || !FPackageName::DoesPackageExist(Map,&Filename) || !Filename.EndsWith(FPackageName::GetMapPackageExtension()))
    { Error=FText::FromString(TEXT("存档所属地图不存在或不是地图资源，未开始加载。")); return false; }
    Error=FText::GetEmpty(); return true;
}
void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct(); SetIsFocusable(true);
    if (GEngine && !TravelFailureHandle.IsValid())
        TravelFailureHandle=GEngine->OnTravelFailure().AddWeakLambda(this,[this](UWorld* World,ETravelFailure::Type,const FString& Reason)
        {
            if (World!=GetWorld()) return;
            bPending=false; CancelNewGame(); ShowError(FText::FromString(TEXT("地图加载失败：")+Reason));
        });
    StartButton->OnClicked.AddUniqueDynamic(this,&ThisClass::StartNewGame);
    ContinueButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ContinueGame);
    QuitButton->OnClicked.AddUniqueDynamic(this,&ThisClass::QuitGame);
    ConfirmButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ConfirmNewGame);
    CancelButton->OnClicked.AddUniqueDynamic(this,&ThisClass::CancelNewGame);
    StartButton->OnHovered.AddUniqueDynamic(this,&ThisClass::HoverStart);
    ContinueButton->OnHovered.AddUniqueDynamic(this,&ThisClass::HoverContinue);
    QuitButton->OnHovered.AddUniqueDynamic(this,&ThisClass::HoverQuit);
    FString Map; FText Error; ContinueButton->SetIsEnabled(ResolveContinueMap(this,Map,Error));
    if (UGameplayStatics::DoesSaveGameExist(TPCSaveSlots::Resolve(),0)) ShowError(Error);
    ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed); Select(0);
}
void UMainMenuWidget::NativeDestruct()
{
    if (GEngine) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    TravelFailureHandle.Reset(); Super::NativeDestruct();
}
void UMainMenuWidget::Select(int32 Index)
{
    Selected=Index;
    UButton* Buttons[]={StartButton,ContinueButton,QuitButton};
    for (int32 I=0;I<3;++I) Buttons[I]->SetBackgroundColor(I==Selected?FLinearColor(.65f,.43f,.16f,1):FLinearColor(.13f,.17f,.17f,.92f));
    SetKeyboardFocus();
}
void UMainMenuWidget::HoverStart() { Select(0); }
void UMainMenuWidget::HoverContinue() { Select(1); }
void UMainMenuWidget::HoverQuit() { Select(2); }
void UMainMenuWidget::ShowError(const FText& Error) { if (ErrorText) ErrorText->SetText(Error); }
void UMainMenuWidget::StartNewGame()
{
    if (bPending) return;
    if (!FPackageName::DoesPackageExist(UArenaTravelSubsystem::TutorialMap())) { ShowError(FText::FromString(TEXT("教程地图不存在，未修改存档。"))); return; }
    if (UGameplayStatics::DoesSaveGameExist(TPCSaveSlots::Resolve(),0))
    { bConfirm=true; ConfirmPanel->SetVisibility(ESlateVisibility::Visible); SetKeyboardFocus(); }
    else ConfirmNewGame();
}
void UMainMenuWidget::ConfirmNewGame()
{
    if (bPending || !FPackageName::DoesPackageExist(UArenaTravelSubsystem::TutorialMap())) return;
    if (UGameplayStatics::DoesSaveGameExist(TPCSaveSlots::Resolve(),0) && !bConfirm) return;
    if (UGameplayStatics::DoesSaveGameExist(TPCSaveSlots::Resolve(),0) && !UGameplayStatics::DeleteGameInSlot(TPCSaveSlots::Resolve(),0))
    { ShowError(FText::FromString(TEXT("无法清除旧存档，未重开游戏。请检查文件权限。"))); return; }
    if (auto* Travel=UArenaTravelSubsystem::Get(this)) Travel->ResetSession();
    bPending=true; UGameplayStatics::OpenLevel(this,FName(UArenaTravelSubsystem::TutorialMap()));
}
void UMainMenuWidget::CancelNewGame() { bConfirm=false; ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed); SetKeyboardFocus(); }
void UMainMenuWidget::ContinueGame()
{
    if (bPending || bConfirm) return;
    FString Map; FText Error;
    if (!ResolveContinueMap(this,Map,Error)) { ShowError(Error); return; }
    if (auto* Travel=UArenaTravelSubsystem::Get(this)) Travel->ResetSession();
    bPending=true; UGameplayStatics::OpenLevel(this,FName(*Map));
}
void UMainMenuWidget::QuitGame() { if (!bConfirm && !bPending) UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false); }
FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if (bConfirm)
    {
        if (Key==EKeys::Escape) CancelNewGame();
        else if (Key==EKeys::Enter) ConfirmNewGame();
        return FReply::Handled();
    }
    if (Key==EKeys::Up || Key==EKeys::Down)
    {
        const int32 Step=Key==EKeys::Down?1:2;
        int32 Next=(Selected+Step)%3; if (Next==1 && !ContinueButton->GetIsEnabled()) Next=(Next+Step)%3;
        Select(Next); return FReply::Handled();
    }
    if (Key==EKeys::Enter) { if (Selected==0) StartNewGame(); else if (Selected==1) ContinueGame(); else QuitGame(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry,Event);
}
AMainMenuGameMode::AMainMenuGameMode() { DefaultPawnClass=nullptr; PlayerControllerClass=AMainMenuController::StaticClass(); }
void AMainMenuController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ACameraActor> It(GetWorld());It;++It) { SetViewTarget(*It); break; }
    auto Class=LoadClass<UMainMenuWidget>(nullptr,TEXT("/Game/Third/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    Menu=Class?CreateWidget<UMainMenuWidget>(this,Class):nullptr;
    if (Menu) { Menu->AddToViewport(100); FInputModeUIOnly Input; Input.SetWidgetToFocus(Menu->TakeWidget()); SetInputMode(Input); }
    bShowMouseCursor=true;
}
