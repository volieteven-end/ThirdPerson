#include "ActionComponent.h"
#include "CombatComponent.h"
#include "EquipmentComponent.h"
#include "../Character/TPCCharacter.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Particles/ParticleSystemComponent.h"

UActionComponent::UActionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

const UActionSet* UActionComponent::GetActionSet() const
{
    const auto* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
    const auto* Weapon = Equipment ? Equipment->GetEquippedWeaponDefinition() : nullptr;
    return Weapon ? Weapon->ActionSet.Get() : nullptr;
}

ETPCMovementContext UActionComponent::GetMovementContext() const
{
    const auto* Character = Cast<ACharacter>(GetOwner());
    return Character && Character->GetCharacterMovement()->IsFalling()
        ? ETPCMovementContext::Air : ETPCMovementContext::Ground;
}

bool UActionComponent::IsMovementLocked() const
{
    return bInputSuppressed || (State != ETPCActionState::Free && State != ETPCActionState::Turn &&
        (!Definition || Definition->MovementPolicy != ETPCMovementPolicy::Input));
}

bool UActionComponent::OwnsRotation() const
{
    return State != ETPCActionState::Free && (!Definition || Definition->RotationPolicy != ETPCRotationPolicy::Movement);
}

float UActionComponent::GetMontagePosition() const
{
    const auto* Character = Cast<ACharacter>(GetOwner());
    auto* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    const auto* Instance = Anim ? Anim->GetMontageInstanceForID(MontageInstanceId) : nullptr;
    return Instance ? Instance->GetPosition() : 0.f;
}

bool UActionComponent::CanRequest(ETPCActionIntent Intent) const
{
    if (State == ETPCActionState::Dead || bInputSuppressed || Intent == ETPCActionIntent::None) return false;
    if (State == ETPCActionState::Free || State == ETPCActionState::Turn) return true;
    if (State == ETPCActionState::Guard) return Intent == ETPCActionIntent::PrimaryAttack || Intent == ETPCActionIntent::Guard;
    if (State == ETPCActionState::Attack && Intent == ETPCActionIntent::PrimaryAttack) return true; // Single combo slot.
    if (State == ETPCActionState::Dodge && Intent == ETPCActionIntent::PrimaryAttack) return true; // First-dodge continuation.
    if (!Definition || bDamageActive || !Definition->CancelIntents.Contains(Intent)) return false;
    const float Position = GetMontagePosition();
    return Definition->CancelStart >= 0.f && Position >= Definition->CancelStart && Position <= Definition->CancelEnd;
}

bool UActionComponent::AuthorizeOrBuffer(ETPCActionIntent Intent)
{
    if (CanRequest(Intent)) return true;
    BufferIntent(Intent);
    return false;
}

void UActionComponent::BufferIntent(ETPCActionIntent Intent, bool bRequireGround)
{
    if (State == ETPCActionState::Dead || State == ETPCActionState::HitReact ||
        State == ETPCActionState::Knockdown || State == ETPCActionState::GetUp || bInputSuppressed || !GetWorld()) return;
    if (Intent != ETPCActionIntent::Jump && Intent != ETPCActionIntent::Dodge && Intent != ETPCActionIntent::PrimaryAttack) return;
    BufferedIntent = Intent;
    bBufferedRequiresGround = bRequireGround;
    BufferedUntil = GetWorld()->GetTimeSeconds() + InputBufferDuration;
}

void UActionComponent::ClearInputBuffers()
{
    BufferedIntent = ETPCActionIntent::None;
    BufferedUntil = 0.;
    bBufferedRequiresGround = false;
    ComboBuffer.Reset(InstanceId);
}

uint64 UActionComponent::BeginAction(ETPCActionState InState, const UActionDefinition* InDefinition)
{
    if (State == ETPCActionState::Dead && InState != ETPCActionState::Dead) return 0;
    CleanupActionTransient();
    ++InstanceId;
    State = InState;
    Definition = InDefinition;
    if (auto* Character = Cast<ACharacter>(GetOwner()))
        Character->SetAnimRootMotionTranslationScale(Definition ? Definition->RootMotionTranslationScale : 1.f);
    ActiveMontage.Reset();
    MontageInstanceId = INDEX_NONE;
    ComboBuffer.Reset(InstanceId);
    bCommitted = false;
    StartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (InState == ETPCActionState::HitReact || InState == ETPCActionState::Dead ||
        InState == ETPCActionState::Knockdown || InState == ETPCActionState::GetUp) ClearInputBuffers();
    return InstanceId;
}

void UActionComponent::BindMontage(uint64 ExpectedId, UAnimMontage* Montage, int32 InMontageInstanceId)
{
    if (ExpectedId != InstanceId) return;
    ActiveMontage = Montage;
    MontageInstanceId = InMontageInstanceId;
    if (Montage && InMontageInstanceId != INDEX_NONE && PublishedPlaybackId != InstanceId)
    {
        PublishedPlaybackId = InstanceId;
        OnActionPlaybackStarted.Broadcast(InstanceId, State, Definition);
    }
}

void UActionComponent::EndAction(uint64 ExpectedId)
{
    if (ExpectedId == 0 || ExpectedId != InstanceId || State == ETPCActionState::Dead) return;
    CleanupActionTransient();
    ++InstanceId;
    State = ETPCActionState::Free;
    Definition = nullptr;
    ActiveMontage.Reset();
    MontageInstanceId = INDEX_NONE;
    ComboBuffer.Reset(InstanceId);
}

void UActionComponent::CleanupActionTransient()
{
    bDamageActive = false;
    if (auto* Character = Cast<ACharacter>(GetOwner())) Character->SetAnimRootMotionTranslationScale(1.f);
    FallbackInvulnerabilityEnd = 0.f;
    if (auto* Character = Cast<ATPCCharacter>(GetOwner()))
    {
        if (Character->CombatComponent) Character->CombatComponent->EndAttackWindow();
        if (Character->EquipmentComponent)
            if (auto* Weapon = Character->EquipmentComponent->GetEquippedWeaponActor()) Weapon->SetAttackEffectActive(false);
        TArray<USceneComponent*> Children;
        Character->GetMesh()->GetChildrenComponents(true, Children);
        for (auto* Child : Children) if (auto* Trail = Cast<UParticleSystemComponent>(Child)) Trail->EndTrails();
        if (Character->MotionWarpingComponent)
        {
            Character->MotionWarpingComponent->DisableAllRootMotionModifiers();
            Character->MotionWarpingComponent->RemoveWarpTarget(Character->AttackWarpTargetName);
        }
    }
}

bool UActionComponent::IsInvulnerable() const
{
    if (State != ETPCActionState::Dodge) return false;
    if (Definition)
    {
        const float Position = GetMontagePosition();
        return MontageInstanceId != INDEX_NONE && Definition->InvulnerabilityStart >= 0.f &&
            Position >= Definition->InvulnerabilityStart && Position < Definition->InvulnerabilityEnd;
    }
    return GetWorld() && GetWorld()->GetTimeSeconds() - StartedAt < FallbackInvulnerabilityEnd;
}

bool UActionComponent::IsFacingWindowOpen() const
{
    return State == ETPCActionState::Attack && (!Definition ||
        (Definition->RotationPolicy == ETPCRotationPolicy::CameraAnticipation && GetMontagePosition() < Definition->FacingEnd));
}

void UActionComponent::EnterDead() { BeginAction(ETPCActionState::Dead); ClearInputBuffers(); }
void UActionComponent::SetInputSuppressed(bool bSuppressed)
{
    bInputSuppressed = bSuppressed;
    ClearInputBuffers();
    if (auto* Character = Cast<ATPCCharacter>(GetOwner()))
    {
        Character->CancelSprintOrDodgeInput();
        Character->ClearMoveInput();
    }
    if (auto* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
    {
        Combat->ClearDashCombo();
        if (bSuppressed) { Combat->CancelGuard(); Combat->CancelActiveAttack(); }
    }
}

void UActionComponent::DispatchIntent(ETPCActionIntent Intent)
{
    if (auto* Character = Cast<ATPCCharacter>(GetOwner()))
    {
        switch (Intent)
        {
        case ETPCActionIntent::Jump: Character->StartJump(); break;
        case ETPCActionIntent::Dodge: Character->Dash(); break;
        case ETPCActionIntent::PrimaryAttack: Character->HandlePrimaryAttack(); break;
        default: break;
        }
    }
}

void UActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime, TickType, Function);
    if (State == ETPCActionState::Attack)
        if (auto* Combat = GetOwner()->FindComponentByClass<UCombatComponent>()) Combat->UpdateDiveApproach();
    if (Definition && !bCommitted && Definition->CommitTime >= 0.f && MontageInstanceId != INDEX_NONE &&
        GetMontagePosition() >= Definition->CommitTime)
    {
        bCommitted = true; // Frame crossing, including low FPS, commits once per action generation.
        if (auto* Combat = GetOwner()->FindComponentByClass<UCombatComponent>()) Combat->CommitSpecialMovement();
    }
    if (BufferedIntent == ETPCActionIntent::None || !GetWorld()) return;
    if (GetWorld()->GetTimeSeconds() > BufferedUntil) { BufferedIntent = ETPCActionIntent::None; return; }
    if (bBufferedRequiresGround && GetMovementContext() != ETPCMovementContext::Ground) return;
    if (const auto* Character = Cast<ATPCCharacter>(GetOwner()))
    {
        if (BufferedIntent == ETPCActionIntent::Jump && !Character->CanJump()) return;
        if (BufferedIntent == ETPCActionIntent::Dodge && GetWorld()->GetTimeSeconds() - Character->LastDashTime < Character->DashCooldown) return;
    }
    if (!CanRequest(BufferedIntent) || (BufferedIntent == ETPCActionIntent::PrimaryAttack && State != ETPCActionState::Free)) return;
    const auto Intent = BufferedIntent;
    BufferedIntent = ETPCActionIntent::None; // Consume before entering any callback.
    DispatchIntent(Intent);
}

void UActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearInputBuffers();
    CleanupActionTransient();
    Super::EndPlay(Reason);
}
