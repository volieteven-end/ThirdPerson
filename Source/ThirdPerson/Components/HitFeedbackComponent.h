#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatHitTypes.h"
#include "Containers/Ticker.h"
#include "HitFeedbackComponent.generated.h"

/** Cosmetic, real-time hit stop. Never owns damage, montages or game pause. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UHitFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Hit Feedback") bool bEnabled=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Hit Feedback", meta=(ClampMin="0.001",ClampMax="0.1")) float Duration=.0333333f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Hit Feedback", meta=(ClampMin="0.01",ClampMax="1")) float SlowScale=.1f;
    UFUNCTION(BlueprintPure) bool IsFeedbackActive() const { return bActive; }
    UFUNCTION(BlueprintCallable) void ClearFeedback();
    void OnMeleeHit(AActor* Victim,const FCombatHitSpec& Spec,const FCombatHitResult& Result);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FCombatUIAccess;
    bool UpdateRealTime(float Unused);
    FTSTicker::FDelegateHandle Ticker;
    int64 LastAction=MIN_int64;
    TSet<FName> PlayedGroups;
    double ExpiresAt=0;
    float PreviousDilation=1,AppliedDilation=1;
    bool bActive=false;
};
