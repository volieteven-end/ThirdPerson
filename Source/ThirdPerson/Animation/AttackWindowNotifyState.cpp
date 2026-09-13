#include "AttackWindowNotifyState.h"
#include "CombatNotifyContext.h"
#include "Engine/World.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponVFXComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	void SetEquippedWeaponEffect(USkeletalMeshComponent* MeshComp, bool bActive, EWeaponVFXStyle Style)
	{
		AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
		UEquipmentComponent* Equipment = OwnerActor
			? OwnerActor->FindComponentByClass<UEquipmentComponent>()
			: nullptr;
		if (AWeaponActor* WeaponActor =
			Equipment ? Equipment->GetEquippedWeaponActor() : nullptr)
		{
			if (!WeaponActor->WeaponVFX->bEnableAutomaticEffects) return;
			if (bActive) WeaponActor->WeaponVFX->SetStyle(Style);
			WeaponActor->SetAttackEffectActive(bActive && Equipment->IsWeaponDrawn());
		}
	}
}


void UAttackWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) { return; }
	AActor* Owner = MeshComp->GetOwner();
	UCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat || !Combat->IsCurrentAttackNotify(Animation, GetCombatNotifyMontageInstanceId(EventReference))) { return; }
	if (WindowType == EAttackNotifyWindowType::ComboInput) { Combat->OpenComboInputWindow(); }
	else if (WindowType == EAttackNotifyWindowType::Damage) { Combat->StartAttackWindow(AttackBoneName, TraceRadius, HitGroup); }
	else if (WindowType == EAttackNotifyWindowType::WeaponEffect) { SetEquippedWeaponEffect(MeshComp, true, EffectStyle); }
	// Damage windows no longer synthesize FX; animation-authored trail/Niagara notifies own them.
}

void UAttackWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) { return; }
	AActor* Owner = MeshComp->GetOwner();
	UCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr;
	// Outgoing NotifyEnd cannot close the next attack's damage / FX window.
	if (!Combat || !Combat->IsCurrentAttackNotify(Animation, GetCombatNotifyMontageInstanceId(EventReference))) { return; }
	if (WindowType == EAttackNotifyWindowType::ComboInput) { Combat->CloseComboInputWindow(); }
	else if (WindowType == EAttackNotifyWindowType::Damage) { Combat->FinishAuthoredDamageWindow(Animation, GetCombatNotifyMontageInstanceId(EventReference)); }
	else if (WindowType == EAttackNotifyWindowType::WeaponEffect) { SetEquippedWeaponEffect(MeshComp, false, EffectStyle); }
}
