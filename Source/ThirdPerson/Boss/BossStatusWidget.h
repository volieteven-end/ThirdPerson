#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossStatusWidget.generated.h"
class ACountessBossCharacter;
UCLASS()
class THIRDPERSON_API UBossStatusWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void BindBoss(ACountessBossCharacter* InBoss);
 virtual void NativeDestruct() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
     FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 TWeakObjectPtr<ACountessBossCharacter> Boss;
 float HealthRatio=1,PoiseRatio=1;
 int32 Phase=1;
 FString BossName;
 double DeathTime=-1;
 FTimerHandle DeathFadeTimer;
 void UpdateDeathFade();
 void Refresh();
};
