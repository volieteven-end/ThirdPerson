#include "HealthPotionWidget.h"
#include "../Components/InventoryComponent.h"
#include "../Items/ItemDefinition.h"
#include "Engine/Texture2D.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UHealthPotionWidget::SetInventory(UInventoryComponent* Inventory)
{
	if (BoundInventory) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Refresh);
	BoundInventory = Inventory;
	if (BoundInventory) BoundInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	Refresh();
}

void UHealthPotionWidget::Refresh()
{
	DisplayedCount = BoundInventory ? BoundInventory->GetHealthPotionCount() : 0;
	const UItemDefinition* Potion = BoundInventory ? BoundInventory->HealthPotionDefinition.Get() : nullptr;
	IconBrush.SetResourceObject(Potion ? Potion->Icon.Get() : nullptr);
	IconBrush.ImageSize = FVector2D(64,64);
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.TintColor = FSlateColor(FLinearColor(1,1,1, DisplayedCount > 0 ? 1.f : .32f));
	InvalidateLayoutAndVolatility();
}

FText UHealthPotionWidget::GetCountText() const
{
	return FText::AsNumber(DisplayedCount);
}

TSharedRef<SWidget> UHealthPotionWidget::RebuildWidget()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	const FLinearColor Gold(.70f,.58f,.34f,1.f);
	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,32,28)
		[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(SBox).WidthOverride(88).HeightOverride(88)
				[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Gold).Padding(1)
					[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(.017f,.031f,.025f,.95f)).Padding(4)
						[SNew(SOverlay)
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[SNew(SBox).WidthOverride(64).HeightOverride(64)[SNew(SImage).Image(&IconBrush)]]
							+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
							[SNew(STextBlock).Text_UObject(this, &ThisClass::GetCountText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ColorAndOpacity(FLinearColor(.96f,.89f,.69f))
								.ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black)]]]]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,6,0,0)
			[SNew(STextBlock).Text(FText::FromString(TEXT("Q  ·  血瓶")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(.86f,.84f,.74f))]];
}

void UHealthPotionWidget::NativeDestruct()
{
	SetInventory(nullptr);
	Super::NativeDestruct();
}
