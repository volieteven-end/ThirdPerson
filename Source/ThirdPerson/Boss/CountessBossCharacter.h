#pragma once
#include "CoreMinimal.h"
#include "../AI/EnemyCharacter.h"
#include "CountessBossCharacter.generated.h"
class UBossActionComponent;
/** Data-only BP child is optional. Never inherits the old Paragon player/VR blueprint. */
UCLASS()
class THIRDPERSON_API ACountessBossCharacter : public AEnemyCharacter
{
 GENERATED_BODY()
public:
 ACountessBossCharacter();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss") TObjectPtr<UBossActionComponent> BossActions;
 virtual void HandleHealthChanged(float Current,float Max) override;
 virtual void ApplyParryStagger(AActor* Player) override;
 virtual void ApplyUppercutHit(AActor* Player) override;
 UFUNCTION(BlueprintCallable,Category="Boss") bool StartEncounter(AActor* Player);
 UFUNCTION(BlueprintCallable,Category="Boss") void RequestEncounterReset();
 UFUNCTION(BlueprintPure,Category="Boss") int32 GetBossPhase() const;
 UFUNCTION(BlueprintPure,Category="Boss") float GetPoisePercent() const;
protected:
 virtual void BeginPlay() override;
 virtual void Landed(const FHitResult& Hit) override;
 virtual void HandleDeath() override;
};
