#include "PlayerDeathWidget.h"
#include "../Character/TPCPlayerController.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UPlayerDeathWidget::RebuildWidget()
{
    SetIsFocusable(true);
    return SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.015f,.012f,.02f,.62f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(420)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18).HAlign(HAlign_Center)
                [SNew(STextBlock).Text(FText::FromString(TEXT("你倒下了")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",36)).ColorAndOpacity(FLinearColor(.92f,.27f,.3f))]
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,28).HAlign(HAlign_Center)
                [SNew(STextBlock).Text(FText::FromString(TEXT("调整节奏，再战一次。")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",18))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(SButton).ContentPadding(FMargin(38,14)).OnClicked_UObject(this,&ThisClass::ClickRestart)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("重新开始  ·  Enter")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold",20))]]]];
}

void UPlayerDeathWidget::Restart()
{
    if (auto* PC = Cast<ATPCPlayerController>(GetOwningPlayer())) PC->RestartAfterDeath();
}
FReply UPlayerDeathWidget::ClickRestart() { Restart(); return FReply::Handled(); }
FReply UPlayerDeathWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Enter || Event.GetKey() == EKeys::SpaceBar) return ClickRestart();
    return Super::NativeOnKeyDown(Geometry, Event);
}
