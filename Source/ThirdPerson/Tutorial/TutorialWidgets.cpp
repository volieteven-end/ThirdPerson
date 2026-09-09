#include "TutorialWidgets.h"
#include "TutorialDirector.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace TutorialStyle
{
const FLinearColor Ink(.023f,.055f,.041f,.97f);
const FLinearColor Gold(.72f,.60f,.32f,1.f);
const FLinearColor Cream(.96f,.93f,.81f,1.f);
const FLinearColor Muted(.65f,.74f,.65f,1.f);
TSharedRef<SBorder> Panel(const TSharedRef<SWidget>& Content, FMargin Padding)
{
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Gold).Padding(1)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(Padding)[Content]];
}
}
FText UTutorialHUDWidget::Heading() const { return Director.IsValid() ? Director->GetHeading() : FText::GetEmpty(); }
FText UTutorialHUDWidget::Instruction() const { return Director.IsValid() ? Director->GetInstruction() : FText::GetEmpty(); }
FText UTutorialHUDWidget::Keys() const { return Director.IsValid() ? Director->GetKeys() : FText::GetEmpty(); }
FText UTutorialHUDWidget::Progress() const { return Director.IsValid() ? Director->GetProgressText() : FText::GetEmpty(); }
FText UTutorialHUDWidget::Status() const { return Director.IsValid() ? Director->GetStatusText() : FText::GetEmpty(); }
FText UTutorialHUDWidget::Direction() const
{
    const auto* PC = GetOwningPlayer(); const APawn* P = PC ? PC->GetPawn().Get() : nullptr;
    if (!P || !Director.IsValid()) return FText::GetEmpty();
    const FVector Delta = Director->GetGuidanceLocation() - P->GetActorLocation();
    const float Yaw = FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Yaw,Delta.Rotation().Yaw);
    const TCHAR* Dir = FMath::Abs(Yaw) < 30 ? TEXT("前方") : FMath::Abs(Yaw) > 135 ? TEXT("后方") : Yaw > 0 ? TEXT("右侧") : TEXT("左侧");
    return FText::FromString(FString::Printf(TEXT("%s  ·  目标 %.0f 米"),Dir,Delta.Size2D()/100.f));
}
TSharedRef<SWidget> UTutorialHUDWidget::RebuildWidget()
{
    using namespace TutorialStyle;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return SNew(SOverlay).Visibility_Lambda([this]()
        { const auto* PC=GetOwningPlayer(); return PC && PC->bShowMouseCursor ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0,80,28,0)
        [SNew(SBox).WidthOverride(390)
            [Panel(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                [SNew(STextBlock).Text(FText::FromString(TEXT("FOREST TRAINING  /  森林训练营")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Gold)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
                [SNew(STextBlock).Text_UObject(this,&ThisClass::Heading).Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ColorAndOpacity(Cream).AutoWrapText(true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(STextBlock).Text_UObject(this,&ThisClass::Instruction).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Cream).AutoWrapText(true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.09f,.15f,.10f)).Padding(10)
                    [SNew(STextBlock).Text_UObject(this,&ThisClass::Keys).Font(FCoreStyle::GetDefaultFontStyle("Bold",15)).ColorAndOpacity(Gold).AutoWrapText(true)]]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text_UObject(this,&ThisClass::Progress).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Muted)],FMargin(20))]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,28,0,0)
        [Panel(SNew(STextBlock).Text_UObject(this,&ThisClass::Direction).Font(FCoreStyle::GetDefaultFontStyle("Bold",15)).ColorAndOpacity(Cream),FMargin(18,9))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(200,0,200,38)
        [SNew(SBox).WidthOverride(620)
            [Panel(SNew(STextBlock).Text_UObject(this,&ThisClass::Status).Font(FCoreStyle::GetDefaultFontStyle("Regular",16))
                .ColorAndOpacity(Cream).Justification(ETextJustify::Center).AutoWrapText(true),FMargin(16,10))]];
}

FText UTutorialPauseWidget::Title() const
{
    const auto* D = ATutorialDirector::Find(this);
    return FText::FromString(D && D->IsSummaryOpen() ? TEXT("基础训练结束") : TEXT("森林训练营 · 教学菜单"));
}
FText UTutorialPauseWidget::Summary() const
{
    const auto* D = ATutorialDirector::Find(this); return D ? D->GetCourseSummary() : FText::GetEmpty();
}
FReply UTutorialPauseWidget::Continue() { if (auto* D = ATutorialDirector::Find(this)) D->ContinuePractice(); return FReply::Handled(); }
FReply UTutorialPauseWidget::Retry() { if (auto* D = ATutorialDirector::Find(this)) D->RetryLesson(); return FReply::Handled(); }
FReply UTutorialPauseWidget::Skip() { if (auto* D = ATutorialDirector::Find(this)) D->SkipLesson(); return FReply::Handled(); }
FReply UTutorialPauseWidget::Restart() { if (auto* D = ATutorialDirector::Find(this)) D->RestartCourse(); return FReply::Handled(); }
FReply UTutorialPauseWidget::Leave() { if (auto* D = ATutorialDirector::Find(this)) D->ReturnToCampaign(); return FReply::Handled(); }
TSharedRef<SWidget> UTutorialPauseWidget::RebuildWidget()
{
    using namespace TutorialStyle;
    SetIsFocusable(true);
    auto Actions = SNew(SVerticalBox);
    auto Button = [&](const TCHAR* Label, FReply (UTutorialPauseWidget::*Fn)())
    {
        Actions->AddSlot().AutoHeight().Padding(0,4)
        [SNew(SButton).OnClicked(FOnClicked::CreateUObject(this,Fn)).ContentPadding(FMargin(16,10))
            .ButtonColorAndOpacity(FLinearColor(.13f,.22f,.16f)).HAlign(HAlign_Center)
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",17)).ColorAndOpacity(Cream)]];
    };
    Button(TEXT("继续 / 自由练习"),&ThisClass::Continue);
    Button(TEXT("重试本节"),&ThisClass::Retry);
    Button(TEXT("跳过本节"),&ThisClass::Skip);
    Button(TEXT("重新开始整套教学"),&ThisClass::Restart);
    Button(TEXT("返回原地图"),&ThisClass::Leave);
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.72f))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(540)
            [Panel(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
                [SNew(STextBlock).Text_UObject(this,&ThisClass::Title).Font(FCoreStyle::GetDefaultFontStyle("Bold",26)).ColorAndOpacity(Cream)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(STextBlock).Text_UObject(this,&ThisClass::Summary).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(Muted)]
                + SVerticalBox::Slot().AutoHeight()[Actions]
                + SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)
                [SNew(STextBlock).Text(FText::FromString(TEXT("Esc  继续    ·    教学不会改变正式存档")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Gold)],FMargin(26))]];
}
