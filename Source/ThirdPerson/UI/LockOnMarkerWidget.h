#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LockOnMarkerWidget.generated.h"

/** 锁定目标的屏幕标记，跟随当前目标更新位置，不负责选择或切换锁定目标。 */
UCLASS()
class THIRDPERSON_API ULockOnMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
};
