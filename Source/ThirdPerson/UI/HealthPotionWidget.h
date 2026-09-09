#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HealthPotionWidget.generated.h"

class UInventoryComponent;

/** Display only: potion ownership/count always comes from the actual inventory. */
UCLASS()
class THIRDPERSON_API UHealthPotionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetInventory(UInventoryComponent* Inventory);
	int32 GetDisplayedCount() const { return DisplayedCount; }
	UInventoryComponent* GetInventory() const { return BoundInventory; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void Refresh();
	FText GetCountText() const;
	UPROPERTY(Transient) TObjectPtr<UInventoryComponent> BoundInventory;
	UPROPERTY(Transient) FSlateBrush IconBrush;
	int32 DisplayedCount = 0;
};
