#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponVFXProfile.generated.h"

class UNiagaraSystem;
class UParticleSystem;

UCLASS(BlueprintType)
class THIRDPERSON_API UWeaponVFXProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> BasicTrail;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> IceTrail;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> ElectricTrail;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> BuffSword;
    /** Only this superseded trail is suppressed; other gameplay particles are untouched. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UParticleSystem> LegacyTrail;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor IceColor = FLinearColor(.12f, .65f, 1.f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor ElectricColor = FLinearColor(.2f, .45f, 1.f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=".01", ClampMax="2")) float TrailWidth = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=".01", ClampMax="2")) float IceSlashWidth = .7f;
    /** Reset ribbon history on teleport, not on an ordinary fast sword swing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100", Units="cm")) float TeleportResetDistance = 180.f;
};
