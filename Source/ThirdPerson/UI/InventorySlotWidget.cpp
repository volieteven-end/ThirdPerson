// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySlotWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

namespace
{
void RefreshForestSlotOutline(UUserWidget* Widget, bool bSelected, bool bHovered)
{
    if (!Widget->GetWidgetFromName(TEXT("ForestSlotOverlay"))) return;
    if (UBorder* Border = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("SlotBorder"))))
    {
        // Outline-only UI material leaves the icon visible, including while selected.
        Border->SetBrushColor(bSelected ? FLinearColor(.95f, .63f, .18f, 1.f)
            : bHovered ? FLinearColor(.60f, .48f, .27f, 1.f) : FLinearColor(.18f, .16f, .10f, .65f));
    }
}
}

void UInventorySlotWidget::SetItemData(const FText& InItemName,int32 InCount,UTexture2D* InIcon )
{
	RefreshSlot(InItemName, InCount,InIcon);
    if (GetWidgetFromName(TEXT("ForestSlotOverlay")))
    {
        if (UImage* Icon = Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon"))))
        {
            Icon->SetBrushFromTexture(InIcon);
            Icon->SetVisibility(InIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
        if (UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemNameText"))))
        {
            Name->SetText(InItemName);
            Name->SetVisibility(InIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        }
        if (UTextBlock* Count = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemCountText"))))
        {
            Count->SetText(FText::AsNumber(InCount));
            Count->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}
void UInventorySlotWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	RefreshSlotStyle(bIsSelected,bIsHovered);
    RefreshForestSlotOutline(this, bIsSelected, bIsHovered);
}
void UInventorySlotWidget::SetEmpty()
{
	RefreshEmptySlot();
    if (GetWidgetFromName(TEXT("ForestSlotOverlay")))
    {
        if (UImage* Icon = Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon"))))
        {
            Icon->SetBrushFromTexture(nullptr);
            Icon->SetVisibility(ESlateVisibility::Collapsed);
        }
        for (const TCHAR* TextName : {TEXT("ItemNameText"), TEXT("ItemCountText")})
        {
            if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TextName)))
            {
                Label->SetText(FText::GetEmpty());
                Label->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }
}
void UInventorySlotWidget::SetSlotIndex(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
}

void UInventorySlotWidget::NotifyClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}
void UInventorySlotWidget::SetHovered(bool bHovered)
{
	bIsHovered = bHovered;
	RefreshSlotStyle(bIsSelected,bIsHovered);
    RefreshForestSlotOutline(this, bIsSelected, bIsHovered);
}
