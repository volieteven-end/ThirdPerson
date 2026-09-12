#include "WeaponVFXComponent.h"
#include "WeaponVFXProfile.h"
#include "CombatVFXSettings.h"
#include "HAL/IConsoleManager.h"
#include "WeaponActor.h"
#include "WeaponDefinition.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
TAutoConsoleVariable<int32> CVarCombatVFX(TEXT("tpc.CombatVFX"), 1,
    TEXT("Enable the new elemental sword and Countess accent effects (cosmetic only)."));
}
bool TPCCombatVFX::IsEnabled() { return CVarCombatVFX.GetValueOnGameThread() != 0; }

UWeaponVFXComponent::UWeaponVFXComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UWeaponVFXComponent::BeginPlay()
{
    Super::BeginPlay();
    Weapon = Cast<AWeaponActor>(GetOwner());
    AActor* Character = Weapon.IsValid() ? Weapon->GetOwner() : nullptr;
    if (!Profile || !Character || GetNetMode() == NM_DedicatedServer) return;
    Combat = Character->FindComponentByClass<UCombatComponent>();
    Equipment = Character->FindComponentByClass<UEquipmentComponent>();
    Actions = Character->FindComponentByClass<UActionComponent>();
    if (auto* C = Cast<ACharacter>(Character)) AddTickPrerequisiteComponent(C->GetMesh());
    auto MakeFX = [this](const TCHAR* Name)
    {
        auto* FX = NewObject<UNiagaraComponent>(GetOwner(), Name);
        FX->SetAutoActivate(false);
        FX->SetAutoDestroy(false);
        FX->SetUsingAbsoluteLocation(true);
        FX->SetUsingAbsoluteRotation(true);
        FX->SetUsingAbsoluteScale(true);
        FX->SetupAttachment(Weapon->GetRootComponent());
        FX->SetTickBehavior(ENiagaraTickBehavior::UsePrereqs);
        FX->AddTickPrerequisiteComponent(this);
        FX->RegisterComponent();
        return FX;
    };
    Trail = MakeFX(TEXT("ElementalAttackTrail"));
    Sword = MakeFX(TEXT("ElementalBuffSword"));
    SuppressLegacyTrail();
    GetWorld()->GetTimerManager().SetTimer(StateTimer, this, &ThisClass::RefreshState, .05f, true);
}

bool UWeaponVFXComponent::CanShow() const
{
    if (!TPCCombatVFX::IsEnabled() || !Profile || !Weapon.IsValid() || !Equipment.IsValid() ||
        Equipment->GetEquippedWeaponActor() != Weapon.Get() || !Equipment->IsWeaponDrawn() ||
        !Combat.IsValid() || !Combat->IsCombatEnabled()) return false;
    const auto* H = Weapon->GetOwner()->FindComponentByClass<UHealthComponent>();
    return (!H || H->CurrentHealth > 0) && (!Actions.IsValid() || Actions->GetActionState() != ETPCActionState::Dead);
}

EWeaponVFXStyle UWeaponVFXComponent::GetResolvedStyle() const
{
    if (Combat.IsValid() && Combat->GetSwordBuffMultiplier() > 1.f) return EWeaponVFXStyle::Electric;
    if (RequestedStyle != EWeaponVFXStyle::Automatic) return RequestedStyle;
    const UActionDefinition* Action = Actions.IsValid() ? Actions->GetActiveDefinition() : nullptr;
    const UActionSet* Set = Combat.IsValid() ? Combat->GetActionSet() : nullptr;
    if (Action && Set)
    {
        if (Action == Set->Rising || Action == Set->Dive) return EWeaponVFXStyle::Ice;
        if (Action == Set->SprintAttack || Action == Set->ParryCounter ||
            (!Set->GroundCombo.IsEmpty() && Action == Set->GroundCombo.Last()) ||
            (!Set->AirCombo.IsEmpty() && Action == Set->AirCombo.Last())) return EWeaponVFXStyle::Electric;
    }
    return EWeaponVFXStyle::Basic;
}

bool UWeaponVFXComponent::ReadBlade(FVector& Base, FVector& Tip) const
{
    const UWeaponDefinition* D = Weapon.IsValid() ? Weapon->GetWeaponDefinition() : nullptr;
    if (!D) return false;
    USceneComponent* Mesh = Weapon->GetWeaponMesh();
    if (auto* Skel = Weapon->GetSkeletalWeaponMesh(); Skel && Skel->GetSkeletalMeshAsset()) Mesh = Skel;
    if (!Mesh || !Mesh->DoesSocketExist(D->BladeBaseSocketName) || !Mesh->DoesSocketExist(D->BladeTipSocketName)) return false;
    Base = Mesh->GetSocketLocation(D->BladeBaseSocketName);
    Tip = Mesh->GetSocketLocation(D->BladeTipSocketName);
    return !Base.ContainsNaN() && !Tip.ContainsNaN() && FVector::DistSquared(Base, Tip) > 1.f;
}

void UWeaponVFXComponent::UpdateParameters(UNiagaraComponent* FX, bool bSword, const FVector& Base, const FVector& Tip)
{
    if (!FX) return;
    const bool bElectric = bSword || GetResolvedStyle() == EWeaponVFXStyle::Electric;
    // Vendor ribbons face SystemZAxis and endpoint sparks use a LOCAL position offset.
    // Place the system at blade midpoint so ribbon width/cylinder height stay on the blade.
    const float Length = FVector::Distance(Base, Tip);
    FX->SetWorldLocationAndRotation((Base + Tip) * .5f, FRotationMatrix::MakeFromZ(Tip - Base).Rotator());
    FX->SetVariableLinearColor(TEXT("User.Color"), bElectric ? Profile->ElectricColor : Profile->IceColor);
    FX->SetVariableFloat(TEXT("User.SwordLength"), Length);
    if (!bSword)
    {
        FX->SetVariableVec3(TEXT("User.EndParticle_Position"), FVector(0, 0, Length * .5f));
        const bool bElectricTrail = GetResolvedStyle() == EWeaponVFXStyle::Electric;
        FX->SetVariableFloat(TEXT("User.Trail Width"), (bElectricTrail ? 18.629158f : 100.f) * Profile->TrailWidth);
        if (GetResolvedStyle() == EWeaponVFXStyle::Ice) FX->SetVariableFloat(TEXT("User.Slash Width"), 135.f * Profile->IceSlashWidth);
    }
}

void UWeaponVFXComponent::StartTrail()
{
    FVector Base, Tip;
    if (!Trail || !CanShow() || !ReadBlade(Base, Tip)) return;
    PlayingStyle = GetResolvedStyle();
    UNiagaraSystem* System = PlayingStyle == EWeaponVFXStyle::Electric ? Profile->ElectricTrail :
        PlayingStyle == EWeaponVFXStyle::Ice ? Profile->IceTrail : Profile->BasicTrail;
    if (!System) return;
    Trail->DeactivateImmediate();
    Trail->SetAsset(System);
    UpdateParameters(Trail, false, Base, Tip);
    Trail->Activate(true);
    bTrailActive = true;
    PreviousOwnerLocation = Weapon->GetOwner()->GetActorLocation();
    bHavePreviousOwnerLocation = true;
    SetComponentTickEnabled(true);
}

void UWeaponVFXComponent::SetAttackActive(bool bActive)
{
    if (!Profile) return;
    SuppressLegacyTrail();
    RefreshState();
    if (bActive && CanShow())
    {
        if (!bTrailActive || PlayingStyle != GetResolvedStyle()) StartTrail();
    }
    else
    {
        bTrailActive = false;
        if (Trail) Trail->DeactivateImmediate();
        RequestedStyle = EWeaponVFXStyle::Automatic;
        SetComponentTickEnabled(bBuffActive);
    }
}

void UWeaponVFXComponent::RefreshState()
{
    if (!CanShow()) { StopAll(); return; }
    const bool bBuff = Combat->GetSwordBuffMultiplier() > 1.f;
    if (bBuff != bBuffActive)
    {
        bBuffActive = bBuff;
        if (Sword)
        {
            Sword->DeactivateImmediate();
            FVector Base, Tip;
            if (bBuff && Profile->BuffSword && ReadBlade(Base, Tip))
            {
                Sword->SetAsset(Profile->BuffSword);
                UpdateParameters(Sword, true, Base, Tip);
                Sword->Activate(true);
            }
        }
    }
    if (bTrailActive && PlayingStyle != GetResolvedStyle()) StartTrail();
    SetComponentTickEnabled(bTrailActive || bBuffActive);
}

void UWeaponVFXComponent::SuppressLegacyTrail()
{
    if (!Profile || !Profile->LegacyTrail || !Weapon.IsValid()) return;
    TInlineComponentArray<UParticleSystemComponent*> Parts(Weapon.Get());
    if (const auto* C = Cast<ACharacter>(Weapon->GetOwner()))
    {
        TArray<USceneComponent*> Children;
        C->GetMesh()->GetChildrenComponents(true, Children);
        for (auto* Child : Children)
            if (auto* P = Cast<UParticleSystemComponent>(Child)) Parts.AddUnique(P);
    }
    for (auto* P : Parts)
        if (IsValid(P) && P->Template == Profile->LegacyTrail)
        {
            P->EndTrails(); P->DeactivateSystem(); P->SetVisibility(false);
        }
}

void UWeaponVFXComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    RefreshState();
    FVector Base, Tip;
    if ((!bTrailActive && !bBuffActive) || !ReadBlade(Base, Tip)) { StopAll(); return; }
    const FVector Location = Weapon->GetOwner()->GetActorLocation();
    if (bHavePreviousOwnerLocation && FVector::DistSquared(Location, PreviousOwnerLocation) > FMath::Square(Profile->TeleportResetDistance))
    {
        if (bTrailActive) StartTrail();
        if (bBuffActive && Sword) { Sword->DeactivateImmediate(); UpdateParameters(Sword, true, Base, Tip); Sword->Activate(true); }
    }
    PreviousOwnerLocation = Location;
    bHavePreviousOwnerLocation = true;
    if (bTrailActive) UpdateParameters(Trail, false, Base, Tip);
    if (bBuffActive) UpdateParameters(Sword, true, Base, Tip);
    SuppressLegacyTrail();
}

void UWeaponVFXComponent::StopAll()
{
    if (Trail) Trail->DeactivateImmediate();
    if (Sword) Sword->DeactivateImmediate();
    bTrailActive = bBuffActive = bHavePreviousOwnerLocation = false;
    RequestedStyle = EWeaponVFXStyle::Automatic;
    SetComponentTickEnabled(false);
}

void UWeaponVFXComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(StateTimer);
    StopAll();
    if (Trail) Trail->DestroyComponent();
    if (Sword) Sword->DestroyComponent();
    Super::EndPlay(Reason);
}
