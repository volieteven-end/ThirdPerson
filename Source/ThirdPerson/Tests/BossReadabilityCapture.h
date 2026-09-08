#pragma once
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"

/** Scope the capture to PIE. Global FScreenshotRequest can be consumed by an editor preview viewport. */
inline bool CaptureBossReadabilityFrame(UWorld* World,const FString& Filename)
{
 auto* Client=World?World->GetGameViewport():nullptr;
 FViewport* Viewport=Client?Client->Viewport:nullptr;
 if (!Viewport) return false;
 const FIntPoint Size=Viewport->GetSizeXY();
 TArray<FColor> Pixels;
 if (Size.X<=0 || Size.Y<=0 || !Viewport->ReadPixels(Pixels) || Pixels.Num()!=Size.X*Size.Y) return false;
 TArray64<uint8> PNG;
 FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
 return FFileHelper::SaveArrayToFile(PNG,*Filename);
}
#endif
