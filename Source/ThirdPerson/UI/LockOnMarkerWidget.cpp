#include "LockOnMarkerWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElementTypes.h"

int32 ULockOnMarkerWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Diameter = FMath::Min(Size.X, Size.Y);
	if (Diameter <= 0.f)
	{
		return Layer;
	}
	const FVector2D Centre = Size * 0.5;
	const FLinearColor ParentTint = InWidgetStyle.GetColorAndOpacityTint();
	auto DrawDisc = [&](float Radius, const FLinearColor& Color)
	{
		const FVector2f DiscSize(Radius * 2.f, Radius * 2.f);
		const FSlateRoundedBoxBrush Disc(FLinearColor::White, Radius, DiscSize);
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer,
			AllottedGeometry.ToPaintGeometry(DiscSize, FSlateLayoutTransform(
				FVector2f(Centre.X - Radius, Centre.Y - Radius))),
			&Disc, ESlateDrawEffect::None, Color * ParentTint);
	};

	// Slate circles stay crisp at HUD DPI scales and need no particle or texture asset.
	const float CoreRadius = Diameter * 0.16f;
	constexpr int32 GlowLayers = 10;
	for (int32 Index = 0; Index < GlowLayers; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / (GlowLayers - 1);
		DrawDisc(FMath::Lerp(Diameter * 0.46f, CoreRadius * 1.3f, Alpha),
			FLinearColor(1.f, 1.f, 1.f, FMath::Lerp(0.018f, 0.09f, Alpha)));
	}
	// A subtle neutral rim keeps the white core readable against pale terrain.
	DrawDisc(CoreRadius + 0.75f, FLinearColor(0.025f, 0.025f, 0.025f, 0.4f));
	DrawDisc(CoreRadius, FLinearColor::White);
	return Layer;
}
