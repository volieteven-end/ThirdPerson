#include "HitFeedbackComponent.h"
#include "CombatComponent.h"
#include "EquipmentComponent.h"
#include "HealthComponent.h"
#include "../AI/EnemyCharacter.h"
#include "../Weapons/WeaponDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"

void UHitFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* C=GetOwner()->FindComponentByClass<UCombatComponent>()) C->OnMeleeHitResolved.AddUObject(this,&ThisClass::OnMeleeHit);
    if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->OnDeath.AddDynamic(this,&ThisClass::ClearFeedback);
}
void UHitFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearFeedback();
    if (auto* C=GetOwner()->FindComponentByClass<UCombatComponent>()) C->OnMeleeHitResolved.RemoveAll(this);
    if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->OnDeath.RemoveAll(this);
    Super::EndPlay(Reason);
}
// —— 只接受主角持近战武器造成的实际伤害；格挡、弹反及重复命中组不触发。
void UHitFeedbackComponent::OnMeleeHit(AActor* Victim,const FCombatHitSpec& Spec,const FCombatHitResult& Result)
{
    const auto* P=Cast<APawn>(GetOwner());
    const auto* E=GetOwner()->FindComponentByClass<UEquipmentComponent>();
    const auto* H=GetOwner()->FindComponentByClass<UHealthComponent>();
    if (!bEnabled || !P || !P->IsLocallyControlled() || !P->IsPlayerControlled() || !E || !E->IsWeaponDrawn() ||
        !E->GetEquippedWeaponDefinition() || E->GetEquippedWeaponDefinition()->WeaponType!=EWeaponType::Melee ||
        !IsValid(Victim) || !Victim->IsA<AEnemyCharacter>() || !H || H->GetCurrentHealth()<=0 ||
        Result.ActualDamage<=0 || Result.bBlocked || Result.bParried || UGameplayStatics::IsGamePaused(this)) return;
    if (LastAction!=Spec.ActionSerial) { LastAction=Spec.ActionSerial; PlayedGroups.Reset(); }
    if (PlayedGroups.Contains(Spec.WindowId)) return;
    PlayedGroups.Add(Spec.WindowId); // 同刀命中多人和重叠请求不会延长本次慢动作。
    if (bActive) return;
    PreviousDilation=GetWorld()->GetWorldSettings()->TimeDilation;
    GetWorld()->GetWorldSettings()->SetTimeDilation(PreviousDilation*FMath::Clamp(SlowScale,.01f,1.f));
    AppliedDilation=GetWorld()->GetWorldSettings()->TimeDilation;
    ExpiresAt=FPlatformTime::Seconds()+FMath::Clamp(Duration,.001f,.1f); bActive=true;
    Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::UpdateRealTime));
}
bool UHitFeedbackComponent::UpdateRealTime(float)
{
    if (!GetWorld() || UGameplayStatics::IsGamePaused(this) || FPlatformTime::Seconds()>=ExpiresAt) { ClearFeedback(); return false; }
    return true;
}
// —— 仅当当前倍率仍等于本组件设置的值才恢复，避免覆盖其他系统后来设置的时间速度。
void UHitFeedbackComponent::ClearFeedback()
{
    if (Ticker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Ticker); Ticker.Reset(); }
    if (bActive && GetWorld())
    {
        auto* Settings=GetWorld()->GetWorldSettings();
        if (FMath::IsNearlyEqual(Settings->TimeDilation,AppliedDilation)) Settings->SetTimeDilation(PreviousDilation);
    }
    bActive=false;
}
