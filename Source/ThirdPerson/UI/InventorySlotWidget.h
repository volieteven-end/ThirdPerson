// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventorySlotWidget.generated.h"
class UTexture2D;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotClicked,int32,SlotIndex);
UCLASS()
class THIRDPERSON_API UInventorySlotWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemData(const FText& InItemName,int32 InCount,UTexture2D* InIcon);
	UFUNCTION(BlueprintCallable)
	void SetSelected(bool bSelected);
	void SetEmpty();
	void SetSlotIndex(int32 InSlotIndex);
	UFUNCTION(BlueprintCallable)
	void NotifyClicked();
	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotClicked OnSlotClicked;
	UFUNCTION(BlueprintCallable)
	void SetHovered(bool bHovered);
protected:
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshSlot(const FText& ItemName,int32 ItemCount,UTexture2D* InIcon);
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshSlotStyle(bool bSelected,bool bHovered);
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshEmptySlot();
	
	
private:
	bool bIsSelected = false;
	bool bIsHovered = false;
	int32 SlotIndex = INDEX_NONE;
};