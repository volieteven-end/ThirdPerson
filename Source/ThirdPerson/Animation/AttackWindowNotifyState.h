// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "../Weapons/WeaponVFXTypes.h"
#include "AttackWindowNotifyState.generated.h"

UENUM(BlueprintType)
enum class EAttackNotifyWindowType : uint8
{
	Damage,
	ComboInput,
	/** Activates the effect owned by the equipped weapon, not the character capsule. */
	WeaponEffect
};

UCLASS()
class THIRDPERSON_API UAttackWindowNotifyState
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Reopening the same group (including looping sections) never damages the same target twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	FName HitGroup = TEXT("Primary");
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,float TotalDuration,const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference) override;
	UPROPERTY(EditAnywhere, Category = "Attack")
	EAttackNotifyWindowType WindowType = EAttackNotifyWindowType::Damage;
	UPROPERTY(EditAnywhere, Category="Attack|Effects")
	EWeaponVFXStyle EffectStyle = EWeaponVFXStyle::Automatic;
	UPROPERTY(EditAnywhere, Category = "Attack")
	FName AttackBoneName = TEXT("hand_r");
	UPROPERTY(EditAnywhere, Category = "Attack",meta = (ClampMin = "1.0"))
	float TraceRadius = 18.f;
};
