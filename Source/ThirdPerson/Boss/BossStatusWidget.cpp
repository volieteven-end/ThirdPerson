#include "BossStatusWidget.h"
#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "../Components/HealthComponent.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"
TSharedRef<SWidget> UBossStatusWidget::RebuildWidget() { return SNew(SSpacer).Size(FVector2D(720,64)); }
void UBossStatusWidget::BindBoss(ACountessBossCharacter* InBoss)
{
 if (Boss.IsValid()) Boss->BossActions->OnStatusChanged.RemoveAll(this);
 Boss=InBoss; DeathTime=-1;
 // UE's position/size setters reset viewport anchors. Apply the top-center anchor last.
 SetDesiredSizeInViewport(FVector2D(720,64)); SetPositionInViewport(FVector2D(0,28),false);
 SetAnchorsInViewport(FAnchors(.5f,0)); SetAlignmentInViewport(FVector2D(.5f,0));
 SetVisibility(ESlateVisibility::HitTestInvisible);
 if (Boss.IsValid()) Boss->BossActions->OnStatusChanged.AddUObject(this,&ThisClass::Refresh);
 Refresh();
}
void UBossStatusWidget::Refresh()
{
 if (!Boss.IsValid()) return;
 const auto* H=Boss->FindComponentByClass<UHealthComponent>(); const auto* A=Boss->BossActions.Get();
 HealthRatio=H?FMath::Clamp(H->CurrentHealth/FMath::Max(1.f,H->MaxHealth),0.f,1.f):0;
 PoiseRatio=FMath::Clamp(A->Poise/FMath::Max(1.f,A->GetMaxPoise()),0.f,1.f);
 Phase=A->Phase; BossName=A->GetDefinition()->DisplayName.ToString();
 if (A->State==EBossState::Dead && DeathTime<0)
 {
  DeathTime=GetWorld()->GetTimeSeconds();
  GetWorld()->GetTimerManager().SetTimer(DeathFadeTimer,this,&ThisClass::UpdateDeathFade,.05f,true);
 }
 InvalidateLayoutAndVolatility();
}
void UBossStatusWidget::UpdateDeathFade()
{
 if (DeathTime>=0)
 {
  double Elapsed=GetWorld()->GetTimeSeconds()-DeathTime;
  SetRenderOpacity(1.f-FMath::Clamp(static_cast<float>((Elapsed-2.)/.4),0.f,1.f));
  if (Elapsed>=2.4) RemoveFromParent();
 }
}
void UBossStatusWidget::NativeDestruct()
{ if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DeathFadeTimer); if (Boss.IsValid()) Boss->BossActions->OnStatusChanged.RemoveAll(this); Super::NativeDestruct(); }
int32 UBossStatusWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& E,int32 L,const FWidgetStyle& Style,bool Enabled) const
{
 L=Super::NativePaint(Args,G,Culling,E,L,Style,Enabled);
 const FVector2D Size=G.GetLocalSize(); const float W=Size.X;
 auto Box=[&](FVector2D P,FVector2D S,FLinearColor C)
 { FSlateDrawElement::MakeBox(E,++L,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,C*Style.GetColorAndOpacityTint()); };
 Box({0,0},{W,64},{.015f,.008f,.015f,.45f});
 FSlateDrawElement::MakeText(E,++L,G.ToPaintGeometry(FVector2D(W,24),FSlateLayoutTransform(FVector2D(12,2))),FString::Printf(TEXT("%s    %s"),*BossName,Phase==2?TEXT("II"):TEXT("I")),FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),18),ESlateDrawEffect::None,FLinearColor(.94f,.85f,.8f)*Style.GetColorAndOpacityTint());
 Box({12,30},{W-24,17},{.08f,.02f,.025f,1}); Box({12,30},{(W-24)*HealthRatio,17},{.65f,.025f,.065f,1});
 Box({12,52},{W-24,5},{.12f,.11f,.085f,1}); Box({12,52},{(W-24)*PoiseRatio,5},{.8f,.61f,.25f,1});
 return L;
}
