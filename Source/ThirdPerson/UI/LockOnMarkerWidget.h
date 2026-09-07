#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LockOnMarkerWidget.generated.h"

/** Small white core with a soft halo, drawn at the enemy's projected chest position. */
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
