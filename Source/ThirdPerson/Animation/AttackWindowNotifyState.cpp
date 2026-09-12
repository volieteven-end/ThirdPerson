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
	bool HasExplicitEffectWindow(const UAnimSequenceBase* Animation)
	{
		return Animation && Animation->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
		{
			const auto* Window = Cast<UAttackWindowNotifyState>(Event.NotifyStateClass);
			return Window && Window->WindowType == EAttackNotifyWindowType::WeaponEffect;
		});
	}
	void SetEquippedWeaponEffect(USkeletalMeshComponent* MeshComp, bool bActive, EWeaponVFXStyle Style, bool bProfileOnly = false)
	{
		AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
		UEquipmentComponent* Equipment = OwnerActor
			? OwnerActor->FindComponentByClass<UEquipmentComponent>()
			: nullptr;
		if (AWeaponActor* WeaponActor =
			Equipment ? Equipment->GetEquippedWeaponActor() : nullptr)
		{
			if (bProfileOnly && !WeaponActor->WeaponVFX->Profile) return;
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
	// Existing authored moves without a separate cosmetic window follow their damage window.
	// This preserves dirty montage assets and never rewrites gameplay notify timing.
	if (WindowType == EAttackNotifyWindowType::Damage && !HasExplicitEffectWindow(Animation) &&
		Combat->IsCurrentAttackNotify(Animation, GetCombatNotifyMontageInstanceId(EventReference)))
		SetEquippedWeaponEffect(MeshComp, true, EffectStyle, true);
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
	if (WindowType == EAttackNotifyWindowType::Damage && !HasExplicitEffectWindow(Animation) &&
		Combat->IsCurrentAttackNotify(Animation, GetCombatNotifyMontageInstanceId(EventReference)))
		SetEquippedWeaponEffect(MeshComp, false, EffectStyle, true);
}
