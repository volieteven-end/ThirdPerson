#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Particles/ParticleSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimNotifies/AnimNotify_PlayParticleEffect.h"
#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "AnimNotifyState_MotionWarping.h"
#include "RootMotionModifier.h"
#include "MotionWarpingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Actions/ActionDefinition.h"
#include "../Character/TPCCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Animation/AttackWindowNotifyState.h"
#include "../Animation/ComboChainPointNotify.h"
#include "../Animation/ActionCommitNotify.h"
#include "../Weapons/WeaponActor.h"
#include "UObject/UnrealType.h"
#include "InputAction.h"
#include "InputMappingContext.h"

struct FTPCSwordActionTestAccess
{
    static void Tick(UActionComponent& A) { A.TickComponent(1.f / 60, LEVELTICK_All, nullptr); }
    static bool Pending(const UActionComponent& A) { return A.BufferedIntent != ETPCActionIntent::None; }
    static void Expire(UActionComponent& A) { A.BufferedUntil = -1.; }
};

namespace SwordTests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
template<class T> T* Load(const FString& Path)
{ return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path))); }
struct FWorld
{
    UWorld* W = nullptr; ATPCCharacter* P = nullptr;
    FWorld()
    {
        W = UWorld::CreateWorld(EWorldType::Game, false); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        UClass* BP = LoadClass<ATPCCharacter>(nullptr, TEXT("/Game/Third/Character/BP_TPCCharacter.BP_TPCCharacter_C"));
        P = W->SpawnActor<ATPCCharacter>(BP, FVector(0,0,100), FRotator::ZeroRotator, Params);
        P->StaminaComponent->SetCurrentStamina(100.f);
        P->HealthComponent->SetCurrentHealth(100.f);
        P->EquipmentComponent->EquipWeapon(Load<UWeaponDefinition>(TEXT("/Game/Third/DataAsset/DA_TestSword")));
        P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    ~FWorld()
    {
        if (P) P->CombatComponent->SetCombatEnabled(false);
        W->DestroyWorld(false); GEngine->DestroyWorldContext(W); if (W->IsRooted()) W->RemoveFromRoot();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordAssetTest, "ThirdPerson.Sword.AssetContract", SwordTests::Flags)
bool FTPCSwordAssetTest::RunTest(const FString&)
{
    using namespace SwordTests;
    const auto* Weapon = Load<UWeaponDefinition>(TEXT("/Game/Third/DataAsset/DA_TestSword"));
    if (!TestNotNull(TEXT("Saved weapon"), Weapon) || !TestNotNull(TEXT("Weapon owns action set"), Weapon->ActionSet.Get())) return false;
    const auto* Set = Weapon->ActionSet.Get();
    TestEqual(TEXT("Four committed ground stages, not twelve chained clips"), Set->GroundCombo.Num(), 4);
    TestEqual(TEXT("Two air stages per flight"), Set->AirCombo.Num(), 2);
    TestEqual(TEXT("Four directional dodges"), Set->Dodges.Num(), 4);
    TestEqual(TEXT("Four root turns"), Set->Turns.Num(), 4);
    TSet<FName> Ids;
    TArray<const UActionDefinition*> Definitions;
    for (const auto* Moves : { &Set->GroundCombo, &Set->AirCombo, &Set->Dodges, &Set->Turns })
        for (const UActionDefinition* D : *Moves) Definitions.Add(D);
    Definitions.Append({ Set->Rising.Get(), Set->Dive.Get(), Set->SprintAttack.Get(), Set->ParryCounter.Get(), Set->Buff.Get(), Set->DrawWeapon.Get(), Set->SheatheWeapon.Get() });
    for (const UActionDefinition* D : Definitions)
    {
        if (!TestNotNull(TEXT("Non-null move"), D) || !TestNotNull(TEXT("Authored montage"), D->Montage.Get())) return false;
        TestFalse(TEXT("Unique action id"), Ids.Contains(D->ActionId)); Ids.Add(D->ActionId);
        TestTrue(*D->ActionId.ToString(), D->Montage->GetPlayLength() > .1f);
        TestEqual(TEXT("One full-body slot"), D->Montage->SlotAnimTracks.Num(), 1);
        TestEqual(TEXT("DefaultSlot contract"), D->Montage->SlotAnimTracks[0].SlotName, FName(TEXT("DefaultSlot")));
        for (const auto& Segment : D->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
            if (const auto* Seq = Cast<UAnimSequence>(Segment.GetAnimReference()))
                for (const auto& Event : Seq->Notifies)
                    if (const auto* N = Cast<UAnimNotify_PlayParticleEffect>(Event.Notify))
                        TestFalse(TEXT("No fixed-frame hit spark on playable attack derivatives"), N->PSTemplate && N->PSTemplate->GetName() == TEXT("P_HitPoint"));
    }
    for (int32 I = 0; I < Set->GroundCombo.Num(); ++I)
    {
        const auto* D = Set->GroundCombo[I].Get(); int32 ChainCount = 0; int32 WarpCount = 0;
        for (const auto& Event : D->Montage->Notifies)
        {
            if (Event.Notify.IsA<UComboChainPointNotify>()) { ++ChainCount; TestTrue(TEXT("Chain before long recovery tail"), Event.GetTriggerTime() < .76f); }
            if (const auto* N = Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass))
                if (const auto* Warp = Cast<URootMotionModifier_Warp>(N->RootMotionModifier))
                {
                    ++WarpCount; TestEqual(TEXT("All four warp target names"), Warp->WarpTargetName, FName(TEXT("AttackTarget")));
                    TestFalse(TEXT("Capsule centre matches gameplay target convention"), Warp->bWarpToFeetLocation);
                    AddInfo(FString::Printf(TEXT("%s warp rotation=%d enabled=%d subtractRemaining=%d"), *D->ActionId.ToString(), static_cast<int32>(Warp->RotationType), Warp->bWarpRotation, Warp->bSubtractRemainingRootMotion));
                    TestEqual(TEXT("Warp respects the attack-facing rotation, not the offset approach point"), Warp->RotationType, EMotionWarpRotationType::Default);
                    TestTrue(TEXT("Warp ends with startup"), Event.GetEndTriggerTime() <= .181f);
                }
        }
        TestEqual(TEXT("Exactly one early chain point except committed finisher"), ChainCount, I < 3 ? 1 : 0);
        TestEqual(TEXT("Exactly one warp window"), WarpCount, 1);
        TestEqual(TEXT("Explicit legal next actions"), D->AllowedNextActions.Num(), I < 3 ? 1 : 0);
    }
    if (TestNotNull(TEXT("Three-phase dive"), Set->Dive.Get()))
    {
        const auto* M = Set->Dive->Montage.Get();
        TestTrue(TEXT("Physical-contact section exists"), M->GetSectionIndex(TEXT("Land")) != INDEX_NONE);
        const int32 Wait = M->GetSectionIndex(TEXT("ContactWait"));
        TestTrue(TEXT("Pre-contact section is a hard physical landing barrier"),Wait!=INDEX_NONE && M->CompositeSections[Wait].NextSectionName==TEXT("ContactWait"));
        const float Descent = M->CompositeSections[M->GetSectionIndex(TEXT("Descent"))].GetTime();
        const float Contact = M->CompositeSections[M->GetSectionIndex(TEXT("Land"))].GetTime();
        TestTrue(TEXT("Landed enters Out at measured frame 17"), FMath::IsNearlyEqual(Contact - Descent, 17.f / 60.f, .001f));
        TestTrue(TEXT("Dive has a complete recovery, not a zero-length dynamic asset"), M->GetPlayLength() > 2.f);
    }
    for (const UActionDefinition* D : Set->Turns)
    {
        TestTrue(TEXT("Turn uses real extracted root rotation"), D->Montage->HasRootMotion());
        for (const auto& Segment : D->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
            for (const auto& Event : Segment.GetAnimReference()->Notifies) TestNotEqual(TEXT("No legacy turn angle commit"), Event.NotifyName, FName(TEXT("CommitTurn")));
    }
    for (bool bCrouch : { false, true })
    {
        auto* BS = Load<UBlendSpace>(bCrouch ? TEXT("/Game/Third/Input/BS_Crouch_Locomotion") : TEXT("/Game/Third/Input/BS_SwordLocomotion"));
        if (!TestNotNull(TEXT("Saved locomotion blendspace"), BS)) return false;
        TestTrue(TEXT("Back directions wrap"), BS->GetBlendParameter(0).bWrapInput);
        for (const auto& Sample : BS->GetBlendSamples())
        {
            if (!Sample.Animation || Sample.SampleValue.Y < 1.f) continue;
            const auto* Seq = Sample.Animation.Get();
            const FVector Delta = Seq->ExtractRootTrackTransform(Seq->GetPlayLength(), nullptr).GetTranslation() - Seq->ExtractRootTrackTransform(0.f, nullptr).GetTranslation();
            const float Speed = Delta.Size2D() / Seq->GetPlayLength() * Sample.RateScale * Seq->RateScale;
            TestTrue(TEXT("One calibrated playback rate matches sampled speed"), FMath::IsNearlyEqual(Speed, static_cast<float>(Sample.SampleValue.Y), .1f));
            TestEqual(TEXT("Two planted-foot markers"), Seq->AuthoredSyncMarkers.Num(), 2);
        }
    }
    TestEqual(TEXT("Buff has explicit start / loop / end sections"), Set->Buff->Montage->CompositeSections.Num(), 3);
    TestTrue(TEXT("Double-jump pose is authored but base ability stays locked"), Set->DoubleJumpMontage && !Set->bAllowDoubleJump);
    for (const UActionDefinition* D : { Set->Rising.Get(),Set->Dive.Get(),Set->Buff.Get(),Set->DrawWeapon.Get(),Set->SheatheWeapon.Get() })
    {
        int32 Count=0;
        for (const auto& E : D->Montage->Notifies) if (E.Notify.IsA<UActionCommitNotify>())
        { ++Count; TestTrue(TEXT("Notify and fallback crossing share the same commit frame"),FMath::IsNearlyEqual(E.GetTriggerTime(),D->CommitTime,.001f)); }
        TestEqual(TEXT("Exactly one idempotent commit per authored action"),Count,1);
    }
    for (int32 Style : {2,3})
    {
        const auto* V=Load<UWeaponDefinition>(FString::Printf(TEXT("/Game/Third/Actions/Sword/DA_Sword_Style%02d"),Style));
        if (!TestNotNull(TEXT("Alternative style weapon"),V) || !TestNotNull(TEXT("Weapon-owned style"),V->ActionSet.Get())) return false;
        TestEqual(TEXT("Alternative style is four stages, never a twelve-clip queue"),V->ActionSet->GroundCombo.Num(),4);
        TestEqual(TEXT("Third style demonstrates unlock via data, not separate input code"),V->ActionSet->bAllowDoubleJump,Style==3);
        for (const UActionDefinition* D : V->ActionSet->GroundCombo)
        {
            TestTrue(TEXT("Alternative root distance has a bounded scale"),D->RootMotionTranslationScale>0.f && D->RootMotionTranslationScale<=1.f);
            TestTrue(TEXT("Alternative action has an actual montage"),D->Montage && D->Montage->GetPlayLength()>1.f);
        }
    }
    UClass* BP=LoadClass<ATPCCharacter>(nullptr,TEXT("/Game/Third/Character/BP_TPCCharacter.BP_TPCCharacter_C"));
    const auto* P=Cast<ATPCCharacter>(BP->GetDefaultObject());
    TestEqual(TEXT("Saved normal movement speed matches the 450 blendspace row"),P->GetCharacterMovement()->MaxWalkSpeed,450.f);
    TestEqual(TEXT("Saved crouch movement speed matches the 100 blendspace row"),P->GetCharacterMovement()->MaxWalkSpeedCrouched,100.f);
    const auto* Property=FindFProperty<FObjectProperty>(UEquipmentComponent::StaticClass(),TEXT("StartingWeapon"));
    TestTrue(TEXT("Saved protagonist starts with the configured sword"),Property && Property->GetObjectPropertyValue_InContainer(P->EquipmentComponent)==Weapon);
    for (const auto* Input : {P->AirDiveAction.Get(),P->BuffAction.Get(),P->ToggleWeaponAction.Get()})
    {
        int32 Bindings=0; for (const auto& Map : P->DefaultMappingContext->GetMappings()) if (Map.Action==Input) ++Bindings;
        TestEqual(TEXT("Every new action has exactly one saved mapping"),Bindings,1);
    }
    TestTrue(TEXT("Existing Q remains bound to UseItem, not replaced by heavy"),P->DefaultMappingContext->GetMappings().ContainsByPredicate(
        [P](const FEnhancedActionKeyMapping& M){ return M.Key==EKeys::Q && M.Action==P->UseItemAction; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordAuthorityTest, "ThirdPerson.Sword.AuthorityAndBuffers", SwordTests::Flags)
bool FTPCSwordAuthorityTest::RunTest(const FString&)
{
    SwordTests::FWorld T; auto* P = T.P; auto* A = P->ActionComponent.Get(); auto* C = P->CombatComponent.Get();
    if (!TestNotNull(TEXT("Saved BP initializes an AnimInstance"), P->GetMesh()->GetAnimInstance())) return false;
    P->HandlePrimaryAttack();
    if (!TestEqual(TEXT("Primary input owns the unified channel"), A->GetActionState(), ETPCActionState::Attack)) return false;
    const uint64 First = A->GetActionInstanceId(); auto* Montage = A->GetActiveDefinition()->Montage.Get();
    for (int32 I = 0; I < 5; ++I) P->HandlePrimaryAttack();
    TestTrue(TEXT("Five clicks store one intent"), C->HasBufferedComboInput());
    TestEqual(TEXT("Early intent does not launch another stage"), A->GetActionInstanceId(), First);
    P->GetMesh()->GetAnimInstance()->Montage_SetPosition(Montage, .25f);
    TestFalse(TEXT("Active frames deny dodge"), A->CanRequest(ETPCActionIntent::Dodge));
    TestFalse(TEXT("Active frames deny guard"), A->CanRequest(ETPCActionIntent::Guard));
    P->GetMesh()->GetAnimInstance()->Montage_SetPosition(Montage, .45f);
    TestTrue(TEXT("Recovery enables dodge"), A->CanRequest(ETPCActionIntent::Dodge));
    TestTrue(TEXT("Recovery enables guard"), A->CanRequest(ETPCActionIntent::Guard));
    C->ReachComboChainPoint(Montage, A->GetMontageInstanceId());
    TestTrue(TEXT("Chain point consumes into a new instance"), A->GetActionInstanceId() > First);
    TestEqual(TEXT("Next action comes from weapon data"), A->GetActiveDefinition()->ActionId, FName(TEXT("Sword.Light.2")));
    TestFalse(TEXT("Spam did not become an input queue"), C->HasBufferedComboInput());
    A->EndAction(First); TestEqual(TEXT("Stale end cannot free the new action"), A->GetActionState(), ETPCActionState::Attack);
    C->CancelActiveAttack();
    TestEqual(TEXT("Interrupt releases channel"), A->GetActionState(), ETPCActionState::Free);
    TestNull(TEXT("Interrupt removes warp target"), P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName));
    P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    A->BufferIntent(ETPCActionIntent::PrimaryAttack, true); FTPCSwordActionTestAccess::Tick(*A);
    TestTrue(TEXT("Landing input waits for ground; it is not continually requeued in air"), FTPCSwordActionTestAccess::Pending(*A));
    FTPCSwordActionTestAccess::Expire(*A); FTPCSwordActionTestAccess::Tick(*A);
    TestFalse(TEXT("Expired landing input is dropped"), FTPCSwordActionTestAccess::Pending(*A));
    A->BufferIntent(ETPCActionIntent::Jump); A->SetInputSuppressed(true);
    TestFalse(TEXT("Menu clears input"), FTPCSwordActionTestAccess::Pending(*A));
    TestFalse(TEXT("Menu denies actions"), A->CanRequest(ETPCActionIntent::PrimaryAttack));
    A->SetInputSuppressed(false); A->EnterDead();
    TestEqual(TEXT("Dead cannot begin a new attack"), A->BeginAction(ETPCActionState::Attack), uint64(0));
    A->EndAction(A->GetActionInstanceId());
    TestEqual(TEXT("Even a current callback cannot exit Dead"), A->GetActionState(), ETPCActionState::Dead);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordUtilityTest,"ThirdPerson.Sword.UtilityAndFailure",SwordTests::Flags)
bool FTPCSwordUtilityTest::RunTest(const FString&)
{
    SwordTests::FWorld T; auto* P=T.P; auto* A=P->ActionComponent.Get(); auto* C=P->CombatComponent.Get();
    auto* E=P->EquipmentComponent.Get(); auto* Anim=P->GetMesh()->GetAnimInstance();
    const auto* Set=A->GetActionSet();
    A->SetInputSuppressed(true); C->TryAttack();
    TestEqual(TEXT("Even a direct combat call cannot bypass suppressed input"),A->GetActionState(),ETPCActionState::Free);
    TestFalse(TEXT("Suppressed component does not play a montage"),C->IsMeleeAttackInProgress());
    A->SetInputSuppressed(false);
    P->HandleBuff();
    TestEqual(TEXT("Buff owns Skill"),A->GetActionState(),ETPCActionState::Skill);
    TestEqual(TEXT("Buff spends SP once"),P->StaminaComponent->GetCurrentStamina(),80.f);
    C->CancelActiveAttack();
    TestEqual(TEXT("Precommit interruption grants no buff"),C->GetSwordBuffMultiplier(),1.f);
    P->StaminaComponent->SetCurrentStamina(100.f); P->HandleBuff();
    Anim->Montage_SetPosition(Set->Buff->Montage,.4f); FTPCSwordActionTestAccess::Tick(*A);
    TestTrue(TEXT("Crossing applies authored damage bonus"),FMath::IsNearlyEqual(C->GetSwordBuffMultiplier(),1.2f));
    C->NotifyActionCommit(Set->Buff->Montage,A->GetMontageInstanceId());
    C->NotifyActionCommit(Set->Buff->Montage,A->GetMontageInstanceId());
    TestTrue(TEXT("Notify plus tick never stacks the bonus"),FMath::IsNearlyEqual(C->GetSwordBuffMultiplier(),1.2f));
    C->CancelActiveAttack(); C->SetCombatEnabled(false); C->SetCombatEnabled(true);
    TestEqual(TEXT("Death/disable clears the buff"),C->GetSwordBuffMultiplier(),1.f);
    auto* WeaponActor=E->GetEquippedWeaponActor();
    P->HandleToggleWeapon(); Anim->Montage_SetPosition(Set->SheatheWeapon->Montage,.56f); FTPCSwordActionTestAccess::Tick(*A);
    TestTrue(TEXT("Sheathe commit hides, rather than destroys, the sword"),E->GetEquippedWeaponActor()==WeaponActor && !E->IsWeaponDrawn() && WeaponActor->IsHidden());
    C->CancelActiveAttack(); P->HandlePrimaryAttack();
    TestTrue(TEXT("Primary while sheathed selects Draw"),A->GetActiveDefinition()==Set->DrawWeapon);
    Anim->Montage_SetPosition(Set->DrawWeapon->Montage,.56f); FTPCSwordActionTestAccess::Tick(*A);
    TestTrue(TEXT("Draw commit unhides the same actor"),E->IsWeaponDrawn() && E->GetEquippedWeaponActor()==WeaponActor && !WeaponActor->IsHidden());
    C->CancelActiveAttack();
    auto* InvalidWeapon=DuplicateObject<UWeaponDefinition>(E->GetEquippedWeaponDefinition(),GetTransientPackage());
    auto* InvalidSet=DuplicateObject<UActionSet>(const_cast<UActionSet*>(Set),GetTransientPackage());
    auto* InvalidAction=DuplicateObject<UActionDefinition>(Set->GroundCombo[0],GetTransientPackage());
    InvalidAction->Montage=nullptr; InvalidAction->RootMotionTranslationScale=.3f;
    InvalidSet->GroundCombo={InvalidAction}; InvalidWeapon->ActionSet=InvalidSet;
    E->EquipWeapon(InvalidWeapon); P->HandlePrimaryAttack();
    TestEqual(TEXT("Missing configured montage remains Free"),A->GetActionState(),ETPCActionState::Free);
    TestFalse(TEXT("Missing asset does not start a substitute attack"),C->IsMeleeAttackInProgress());
    TestNull(TEXT("Failed entry leaves no warp target"),P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName));
    TestEqual(TEXT("Failed entry leaves root scale restored"),P->GetAnimRootMotionTranslationScale(),1.f);
    return true;
}
#endif
