#include "SwordActionAssetTools.h"
#if WITH_EDITOR
#include "ActionDefinition.h"
#include "../Character/TPCCharacter.h"
#include "../AI/EnemyCharacter.h"
#include "../Boss/BossDefinition.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Animation/AttackWindowNotifyState.h"
#include "../Animation/ComboChainPointNotify.h"
#include "../Animation/ActionCommitNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNotifies/AnimNotify_PlayParticleEffect.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimNotifyState_MotionWarping.h"
#include "RootMotionModifier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Particles/ParticleSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

bool UpdateSwordAnimationGraphs();

namespace SwordAssetAuthoring
{
const FString Root = TEXT("/Game/Third/Actions/Sword/");
const FString SourceRoot = TEXT("/Game/Third/SwordAnimation/");
template<class T> T* Load(const FString& Path)
{
    return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path)), nullptr, LOAD_NoWarn);
}
template<class T> T* Asset(const FString& Relative)
{
    const FString Path = Root + Relative;
    if (T* Found = Load<T>(Path)) return Found;
    auto* Result = NewObject<T>(CreatePackage(*Path), *FPackageName::GetLongPackageAssetName(Path), RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);
    return Result;
}
bool Save(UObject* Object)
{
    if (!Object) return false;
    Object->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
        *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
UAnimSequence* Sequence(const FString& Name, bool bRootMotion = true, bool bStripAllNotifies = false)
{
    auto* Source = Load<UAnimSequence>(SourceRoot + Name);
    if (!Source) return nullptr;
    const FString Relative = TEXT("Sequences/AS_") + Name;
    auto* Result = Load<UAnimSequence>(Root + Relative);
    if (!Result)
    {
        Result = DuplicateObject<UAnimSequence>(Source, CreatePackage(*(Root + Relative)), *FPackageName::GetLongPackageAssetName(Relative));
        Result->SetFlags(RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(Result);
    }
    Result->RateScale = 1.f;
    Result->bEnableRootMotion = bRootMotion;
    Result->bForceRootLock = true;
    Result->RootMotionRootLock = ERootMotionRootLock::RefPose;
    Result->Notifies.RemoveAll([bStripAllNotifies](const FAnimNotifyEvent& Event)
    {
        const auto* Particle = Cast<UAnimNotify_PlayParticleEffect>(Event.Notify);
        return bStripAllNotifies || Event.NotifyName == TEXT("CommitTurn") ||
            (Particle && Particle->PSTemplate && Particle->PSTemplate->GetName() == TEXT("P_HitPoint"));
    });
    Result->PostEditChange();
    return Save(Result) ? Result : nullptr;
}
UAnimMontage* Montage(const FString& Name, UAnimSequence* Seq, bool bLoop = false)
{
    if (!Seq) return nullptr;
    const FString Relative = TEXT("Montages/") + Name;
    auto* M = Load<UAnimMontage>(Root + Relative);
    if (!M)
    {
        auto* Temp = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Seq, TEXT("DefaultSlot"), .08f, .12f, 1.f, 1);
        M = DuplicateObject<UAnimMontage>(Temp, CreatePackage(*(Root + Relative)), *Name);
        M->ClearFlags(RF_Transient); M->SetFlags(RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(M);
    }
    M->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(Seq);
    M->SetCompositeLength(M->CalculateSequenceLength());
    if (M->GetPlayLength() <= 0.f) { UE_LOG(LogTemp, Error, TEXT("Empty authored montage %s"), *Name); return nullptr; }
    if (bLoop && M->CompositeSections.Num() > 0) M->CompositeSections[0].NextSectionName = M->CompositeSections[0].SectionName;
    M->PostEditChange();
    return Save(M) ? M : nullptr;
}
void Damage(UAnimMontage* M, float Start, float End, FName Group = TEXT("Primary"))
{
    if (!M->AnimNotifyTracks.ContainsByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TEXT("SwordAction"); }))
        UAnimationBlueprintLibrary::AddAnimationNotifyTrack(M, TEXT("SwordAction"));
    auto* Notify = Cast<UAttackWindowNotifyState>(UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(
        M, TEXT("SwordAction"), Start, End - Start, UAttackWindowNotifyState::StaticClass()));
    if (Notify) { Notify->WindowType = EAttackNotifyWindowType::Damage; Notify->AttackBoneName = TEXT("hand_r"); Notify->TraceRadius = 12.f; Notify->HitGroup = Group; }
}
void Chain(UAnimMontage* M, float Time)
{
    if (!M->AnimNotifyTracks.ContainsByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TEXT("SwordAction"); }))
        UAnimationBlueprintLibrary::AddAnimationNotifyTrack(M, TEXT("SwordAction"));
    UAnimationBlueprintLibrary::AddAnimationNotifyEvent(M, TEXT("SwordAction"), Time, UComboChainPointNotify::StaticClass());
}
void Commit(UAnimMontage* M, float Time)
{
    if (!M->AnimNotifyTracks.ContainsByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TEXT("SwordAction"); }))
        UAnimationBlueprintLibrary::AddAnimationNotifyTrack(M, TEXT("SwordAction"));
    UAnimationBlueprintLibrary::AddAnimationNotifyEvent(M, TEXT("SwordAction"), Time, UActionCommitNotify::StaticClass());
}
void ClearProjectWindows(UAnimMontage* M)
{
    M->Notifies.RemoveAll([](const FAnimNotifyEvent& E)
    { return E.NotifyStateClass.IsA<UAttackWindowNotifyState>() || E.Notify.IsA<UComboChainPointNotify>() || E.Notify.IsA<UActionCommitNotify>(); });
}
UActionDefinition* Definition(const FString& Name, const FString& Id, UAnimMontage* M, float Recovery, float Scale = 1.f)
{
    if (!M) return nullptr;
    auto* D = Asset<UActionDefinition>(TEXT("Data/") + Name);
    D->ActionId = FName(*Id); D->Montage = M; D->State = ETPCActionState::Attack;
    D->PlayRate = 1.f; D->DamageMultiplier = Scale;
    D->CancelStart = Recovery; D->CancelEnd = M->GetPlayLength();
    D->CancelIntents = { ETPCActionIntent::Dodge, ETPCActionIntent::Guard };
    D->FacingEnd = .18f;
    return D;
}
bool CalibrateBlendSpace(UBlendSpace* BS, bool bCrouch)
{
    if (!BS) return false;
    auto* Param = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    auto* Interp = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("InterpolationParam"));
    auto* Samples = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
    if (!Param || !Interp || !Samples) return false;
    auto* Direction = Param->ContainerPtrToValuePtr<FBlendParameter>(BS, 0);
    Direction->Min = -180; Direction->Max = 180; Direction->bWrapInput = true; Direction->bSnapToGrid = false;
    auto* Speed = Param->ContainerPtrToValuePtr<FBlendParameter>(BS, 1);
    Speed->Min = 0; Speed->Max = bCrouch ? 100 : 650; Speed->bSnapToGrid = false;
    Interp->ContainerPtrToValuePtr<FInterpolationParameter>(BS, 0)->InterpolationTime = .1f;
    Interp->ContainerPtrToValuePtr<FInterpolationParameter>(BS, 1)->InterpolationTime = .2f;
    FScriptArrayHelper Array(Samples, Samples->ContainerPtrToValuePtr<void>(BS));
    // Normalize EVERY coordinate before editing a sequence can trigger dependent BlendSpace validation.
    for (int32 I = 0; I < Array.Num(); ++I)
    {
        auto& Sample = *reinterpret_cast<FBlendSample*>(Array.GetRawPtr(I));
        if (!Sample.Animation || Sample.SampleValue.Y < 1.f) continue;
        FString Name = Sample.Animation->GetName(); Name.RemoveFromStart(TEXT("AS_"));
        Sample.SampleValue.Y = bCrouch ? 100.f : (Name.StartsWith(TEXT("Walk")) ? 150.f : (Name.StartsWith(TEXT("Run")) ? 450.f : 650.f));
    }
    for (int32 I = 0; I < Array.Num(); ++I)
    {
        auto& Sample = *reinterpret_cast<FBlendSample*>(Array.GetRawPtr(I));
        if (!Sample.Animation || Sample.SampleValue.Y < 1.f) continue;
        FString Name = Sample.Animation->GetName();
        Name.RemoveFromStart(TEXT("AS_"));
        const float TargetSpeed = bCrouch ? 100.f : (Name.StartsWith(TEXT("Walk")) ? 150.f : (Name.StartsWith(TEXT("Run")) ? 450.f : 650.f));
        auto* Seq = Sequence(Name, false, true);
        if (!Seq) return false;
        const FVector Delta = Seq->ExtractRootTrackTransform(Seq->GetPlayLength(), nullptr).GetTranslation() - Seq->ExtractRootTrackTransform(0.f, nullptr).GetTranslation();
        const float NativeSpeed = Delta.Size2D() / Seq->GetPlayLength();
        if (NativeSpeed < 10.f) return false;
        Sample.Animation = Seq; Sample.SampleValue.Y = TargetSpeed; Sample.RateScale = TargetSpeed / NativeSpeed;
        // Foot contact markers are authored at each foot's minimum ball height, independently per direction.
        if (!Seq->AnimNotifyTracks.ContainsByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TEXT("FootSync"); }))
            UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Seq, TEXT("FootSync"));
        UAnimationBlueprintLibrary::RemoveAllAnimationSyncMarkers(Seq);
        const auto& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
        for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
        {
            const FName BoneName(*FString::Printf(TEXT("ball_%s"), Side));
            const int32 BoneIndex = Ref.FindBoneIndex(BoneName);
            if (BoneIndex == INDEX_NONE) return false;
            float BestTime = 0.f; float BestZ = MAX_flt;
            const int32 Count = FMath::Max(60, Seq->GetNumberOfSampledKeys());
            for (int32 N = 0; N < Count; ++N)
            {
                const float Time = Seq->GetPlayLength() * N / Count;
                FTransform Pose = FTransform::Identity;
                for (int32 Bone = BoneIndex; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
                {
                    FTransform Local; Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(static_cast<double>(Time)), true);
                    Pose = Pose * Local;
                }
                if (Pose.GetTranslation().Z < BestZ) { BestZ = Pose.GetTranslation().Z; BestTime = Time; }
            }
            UAnimationBlueprintLibrary::AddAnimationSyncMarker(Seq, FName(*FString::Printf(TEXT("Foot_%s"), Side)), BestTime, TEXT("FootSync"));
        }
        UE_LOG(LogTemp, Display, TEXT("Sword gait %s: %.2f / %.2f = %.4f"), *Name, TargetSpeed, NativeSpeed, Sample.RateScale);
        if (!Save(Seq)) return false;
    }
    BS->ValidateSampleData(); BS->ResampleData(); BS->PostEditChange();
    return Save(BS);
}
}
#endif

bool USwordActionAssetTools::BuildSwordActionAssets()
{
#if WITH_EDITOR
    using namespace SwordAssetAuthoring;
    auto* Set = Asset<UActionSet>(TEXT("DA_SwordActionSet"));
    Set->GroundCombo.Reset(); Set->AirCombo.Reset(); Set->Dodges.Reset(); Set->Turns.Reset();
    const float Starts[] = { .214f, .223f, .332f, .338f };
    const float Ends[] = { .356f, .408f, .502f, .536f };
    const float Chains[] = { .58f, .58f, .68f, -1.f };
    for (int32 I = 0; I < 4; ++I)
    {
        auto* Seq = Sequence(FString::Printf(TEXT("Attack_Combo_01_%02d_Anim"), I + 1));
        auto* M = Load<UAnimMontage>(SourceRoot + FString::Printf(TEXT("AM_Sword_Attack_%02d"), I + 1));
        if (!M || !Seq) return false;
        for (auto& Track : M->SlotAnimTracks) for (auto& Segment : Track.AnimTrack.AnimSegments) Segment.SetAnimReference(Seq);
        ClearProjectWindows(M); Damage(M, Starts[I], Ends[I]); if (I < 3) Chain(M, Chains[I]);
        for (auto& Event : M->Notifies)
            if (auto* Notify = Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass))
                if (auto* Warp = Cast<URootMotionModifier_Warp>(Notify->RootMotionModifier))
                {
                    Warp->WarpTargetName = TEXT("AttackTarget"); Warp->bWarpToFeetLocation = false; Warp->bIgnoreZAxis = true;
                    // The approach point may be behind an already-close attacker. Face the
                    // opponent's authored target rotation, never that offset position.
                    Warp->RotationType = EMotionWarpRotationType::Default;
                    // Direction/target adjustments finish before the blade's active frames.
                    Event.SetTime(.04f); Event.SetDuration(.14f); Event.EndLink.Link(M, .18f);
                }
        M->PostEditChange(); if (!Save(M)) return false;
        auto* D = Definition(FString::Printf(TEXT("DA_Action_Sword_Light_%02d"), I + 1),
            FString::Printf(TEXT("Sword.Light.%d"), I + 1), M, I == 3 ? .92f : Ends[I] + .06f, I == 3 ? 1.35f : 1.f);
        D->AllowedNextActions.Reset();
        if (I < 3) D->AllowedNextActions.Add(FName(*FString::Printf(TEXT("Sword.Light.%d"), I + 2)));
        D->PoiseDamage = I == 3 ? 18.f : 10.f;
        if (!Save(D)) return false; Set->GroundCombo.Add(D);
    }
    for (int32 I = 0; I < 2; ++I)
    {
        auto* M = Montage(FString::Printf(TEXT("AM_Sword_AirLight_%02d"), I + 1), Sequence(FString::Printf(TEXT("Air_Attack_%02d_Anim"), I + 1)));
        if (!M) return false;
        ClearProjectWindows(M); Damage(M, .16f, .34f); if (I == 0) Chain(M, .48f);
        if (!Save(M)) return false;
        auto* D = Definition(FString::Printf(TEXT("DA_Action_Sword_AirLight_%02d"), I + 1),
            FString::Printf(TEXT("Sword.Air.Light.%d"), I + 1), M, .46f, 1.15f);
        D->MovementPolicy = ETPCMovementPolicy::Ballistic; D->CancelIntents.Add(ETPCActionIntent::Dive); D->PoiseDamage = 12.f; D->AllowedNextActions.Reset();
        if (I == 0) D->AllowedNextActions.Add(TEXT("Sword.Air.Light.2"));
        if (!Save(D)) return false; Set->AirCombo.Add(D);
    }
    auto* Rise = Montage(TEXT("AM_Sword_Rising"), Sequence(TEXT("Attack_Rising_Anim")));
    if (!Rise) return false;
    ClearProjectWindows(Rise); Damage(Rise, .20f, .46f); Commit(Rise, .18f); Chain(Rise, .54f); if (!Save(Rise)) return false;
    Set->Rising = Definition(TEXT("DA_Action_Sword_Rising"), TEXT("Sword.Rising"), Rise, .62f, 1.5f);
    Set->Rising->StaminaCost = 15.f; Set->Rising->CommitTime = .18f; Set->Rising->MovementPolicy = ETPCMovementPolicy::Ballistic;
    Set->Rising->AllowedNextActions = { TEXT("Sword.Air.Light.1") };
    Set->Rising->HitReactionProfile = ETPCHitReactionProfile::Launch; Set->Rising->PoiseDamage = 30.f;
    if (!Save(Set->Rising)) return false;

    auto* DiveIn = Sequence(TEXT("Air_Attack_05_In_Anim"));
    auto* DiveLoop = Sequence(TEXT("Air_Attack_05_Loop_Anim"));
    auto* DiveOut = Sequence(TEXT("Air_Attack_05_Out_Anim"));
    auto* Dive = Montage(TEXT("AM_Sword_Dive"), DiveIn);
    if (!Dive || !DiveLoop || !DiveOut) return false;
    auto& Segments = Dive->SlotAnimTracks[0].AnimTrack.AnimSegments;
    Segments.SetNum(1); Segments[0].StartPos = 0.f;
    FAnimSegment Loop; Loop.SetAnimReference(DiveLoop); Loop.AnimStartTime = 0.f; Loop.AnimEndTime = DiveLoop->GetPlayLength(); Loop.StartPos = DiveIn->GetPlayLength();
    Segments.Add(Loop);
    FAnimSegment Out; Out.SetAnimReference(DiveOut); Out.AnimStartTime = 0.f; Out.AnimEndTime = DiveOut->GetPlayLength(); Out.StartPos = Loop.StartPos + Loop.GetLength();
    Segments.Add(Out);
    Dive->SetCompositeLength(Dive->CalculateSequenceLength());
    Dive->CompositeSections.Reset();
    Dive->AddAnimCompositeSection(TEXT("Start"), 0.f);
    Dive->AddAnimCompositeSection(TEXT("Loop"), Loop.StartPos);
    Dive->AddAnimCompositeSection(TEXT("Descent"), Out.StartPos);
    // 60 Hz pose extraction: frame 17 first has BOTH ball bones within 3 cm of the floor.
    const float Contact = Out.StartPos + 17.f / 60.f;
    // A bounded pre-contact section can never advance into grounded recovery by time alone.
    Dive->AddAnimCompositeSection(TEXT("ContactWait"), Contact - 1.f / 60.f);
    Dive->AddAnimCompositeSection(TEXT("Land"), Contact);
    Dive->CompositeSections[0].NextSectionName = TEXT("Loop");
    Dive->CompositeSections[1].NextSectionName = TEXT("Loop");
    Dive->CompositeSections[2].NextSectionName = TEXT("ContactWait");
    Dive->CompositeSections[3].NextSectionName = TEXT("ContactWait");
    Dive->PostEditChange();
    ClearProjectWindows(Dive); Damage(Dive, .22f, .38f); Damage(Dive, Contact + .01f, Contact + .17f, TEXT("Landing")); Commit(Dive, .3f);
    if (!Save(Dive)) return false;
    Set->Dive = Definition(TEXT("DA_Action_Sword_Dive"), TEXT("Sword.Air.Dive"), Dive, Contact + .65f, 1.75f);
    Set->Dive->CommitTime = .3f; Set->Dive->StaminaCost = 15.f; Set->Dive->MovementPolicy = ETPCMovementPolicy::Ballistic;
    Set->Dive->RotationPolicy = ETPCRotationPolicy::Frozen; Set->Dive->PoiseDamage = 25.f;
    if (!Save(Set->Dive)) return false;

    for (const TCHAR* Direction : { TEXT("F"), TEXT("B"), TEXT("L"), TEXT("R") })
    {
        auto* M = Load<UAnimMontage>(SourceRoot + FString::Printf(TEXT("AM_Dodge_%s_Anim_Montage"), Direction));
        auto* D = Definition(FString::Printf(TEXT("DA_Action_Dodge_%s"), Direction), FString::Printf(TEXT("Sword.Dodge.%s"), Direction), M, -1.f);
        if (!D) return false;
        D->State = ETPCActionState::Dodge; D->RotationPolicy = ETPCRotationPolicy::Frozen;
        D->StaminaCost = 30.f; D->InvulnerabilityStart = .05f; D->InvulnerabilityEnd = .30f;
        D->CancelIntents.Reset(); D->FacingEnd = 0.f;
        if (!Save(D)) return false; Set->Dodges.Add(D);
    }
    const TCHAR* TurnNames[] = { TEXT("L_90"), TEXT("R_90"), TEXT("L_180"), TEXT("R_180") };
    for (const TCHAR* Name : TurnNames)
    {
        auto* M = Montage(FString::Printf(TEXT("AM_Turn_%s"), Name), Sequence(FString::Printf(TEXT("Turn_%s_Anim"), Name), true, true));
        auto* D = Definition(FString::Printf(TEXT("DA_Action_Turn_%s"), Name), FString::Printf(TEXT("Sword.Turn.%s"), Name), M, -1.f);
        if (!D) return false;
        D->State = ETPCActionState::Turn; D->RotationPolicy = ETPCRotationPolicy::Root; D->CancelIntents.Reset();
        if (!Save(D)) return false; Set->Turns.Add(D);
    }
    auto* Sprint = Montage(TEXT("AM_Sword_SprintAttack"), Sequence(TEXT("Attack_Sprint_Anim")));
    auto* Counter = Montage(TEXT("AM_Sword_ParryCounter"), Sequence(TEXT("Attack_Combo_02_04_Anim")));
    if (!Sprint || !Counter) return false;
    ClearProjectWindows(Sprint); Damage(Sprint, .28f, .55f); if (!Save(Sprint)) return false;
    ClearProjectWindows(Counter); Damage(Counter, .26f, .54f); if (!Save(Counter)) return false;
    Set->SprintAttack = Definition(TEXT("DA_Action_Sword_SprintAttack"), TEXT("Sword.Sprint"), Sprint, .75f, 1.45f);
    Set->SprintAttack->StaminaCost = 15.f; Set->SprintAttack->PoiseDamage = 20.f;
    Set->ParryCounter = Definition(TEXT("DA_Action_Sword_ParryCounter"), TEXT("Sword.ParryCounter"), Counter, .72f, 2.f);
    Set->ParryCounter->PoiseDamage = 40.f;
    if (!Save(Set->SprintAttack) || !Save(Set->ParryCounter) || !Save(Set)) return false;

    // Utility actions share exactly the same full-body authority and interruption path.
    auto* BuffIn = Sequence(TEXT("Buff_In_Anim"), false, true);
    auto* BuffLoop = Sequence(TEXT("Buff_Loop_Anim"), false, true);
    auto* BuffOut = Sequence(TEXT("Buff_out_Anim"), false, true);
    auto* Buff = Montage(TEXT("AM_Sword_Buff"), BuffIn);
    if (!Buff || !BuffLoop || !BuffOut) return false;
    auto& BuffSegments = Buff->SlotAnimTracks[0].AnimTrack.AnimSegments;
    BuffSegments.SetNum(1);
    FAnimSegment BLoop; BLoop.SetAnimReference(BuffLoop, true); BLoop.StartPos = BuffIn->GetPlayLength(); BuffSegments.Add(BLoop);
    FAnimSegment BOut; BOut.SetAnimReference(BuffOut, true); BOut.StartPos = BLoop.StartPos + BLoop.GetLength(); BuffSegments.Add(BOut);
    Buff->SetCompositeLength(Buff->CalculateSequenceLength());
    Buff->CompositeSections.Reset();
    Buff->AddAnimCompositeSection(TEXT("Start"), 0.f); Buff->AddAnimCompositeSection(TEXT("Loop"), BLoop.StartPos);
    Buff->AddAnimCompositeSection(TEXT("End"), BOut.StartPos);
    Buff->CompositeSections[0].NextSectionName = TEXT("Loop"); Buff->CompositeSections[1].NextSectionName = TEXT("End");
    ClearProjectWindows(Buff); Commit(Buff, .35f); Buff->PostEditChange();
    Set->Buff = Definition(TEXT("DA_Action_Sword_Buff"), TEXT("Sword.Buff"), Buff, .5f);
    Set->Buff->State = ETPCActionState::Skill; Set->Buff->StaminaCost = 20.f; Set->Buff->CommitTime = .35f;
    Set->Buff->MovementPolicy = ETPCMovementPolicy::Locked; Set->Buff->RotationPolicy = ETPCRotationPolicy::Frozen;
    Set->BuffDuration = 8.f; Set->BuffDamageMultiplier = 1.2f;
    auto* Draw = Montage(TEXT("AM_Sword_Draw"), Sequence(TEXT("Weapon_Deployment_Anim"), false, true));
    auto* Sheathe = Montage(TEXT("AM_Sword_Sheathe"), Sequence(TEXT("Weapon_Retraction_Anim"), false, true));
    if (!Draw || !Sheathe) return false;
    Set->DrawWeapon = Definition(TEXT("DA_Action_Sword_Draw"), TEXT("Sword.Draw"), Draw, .8f);
    Set->SheatheWeapon = Definition(TEXT("DA_Action_Sword_Sheathe"), TEXT("Sword.Sheathe"), Sheathe, .8f);
    for (auto* D : { Set->DrawWeapon.Get(), Set->SheatheWeapon.Get() })
    {
        D->State = ETPCActionState::Skill; D->MovementPolicy = ETPCMovementPolicy::Locked;
        D->RotationPolicy = ETPCRotationPolicy::Frozen; D->RootMotionTranslationScale = 0.f; D->CommitTime = .55f;
        ClearProjectWindows(D->Montage); Commit(D->Montage, D->CommitTime);
        if (!Save(D->Montage) || !Save(D)) return false;
    }
    Set->DoubleJumpMontage = Montage(TEXT("AM_Sword_DoubleJump"), Sequence(TEXT("Jump_Two_Anim"), false, true));
    if (!Set->DoubleJumpMontage || !Save(Buff) || !Save(Set->Buff) || !Save(Set)) return false;

    auto* Weapon = Load<UWeaponDefinition>(TEXT("/Game/Third/DataAsset/DA_TestSword"));
    auto* BP = Load<UBlueprint>(TEXT("/Game/Third/Character/BP_TPCCharacter"));
    if (!Weapon || !BP) return false;
    Weapon->ActionSet = Set;
    if (!Save(Weapon)) return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto* CDO = Cast<ATPCCharacter>(BP->GeneratedClass->GetDefaultObject());
    if (!CDO || !CDO->CombatComponent) return false;
    // The design is a sword protagonist. Equip through the existing component, not a second actor path.
    if (auto* StartWeaponProperty = FindFProperty<FObjectProperty>(UEquipmentComponent::StaticClass(), TEXT("StartingWeapon")))
        StartWeaponProperty->SetObjectPropertyValue_InContainer(CDO->EquipmentComponent, Weapon);
    CDO->TurnLeft90Montage = Set->Turns[0]->Montage; CDO->TurnRight90Montage = Set->Turns[1]->Montage;
    CDO->TurnLeft180Montage = Set->Turns[2]->Montage; CDO->TurnRight180Montage = Set->Turns[3]->Montage;
    CDO->DeathMontage = Load<UAnimMontage>(SourceRoot + TEXT("AM_Dead"));
    if (CDO->DeathMontage) { CDO->DeathMontage->bEnableAutoBlendOut = false; if (!Save(CDO->DeathMontage)) return false; }
    CDO->BlockBreachReact = Load<UAnimSequence>(SourceRoot + TEXT("Block_Breach_Anim"));
    CDO->GetCharacterMovement()->MaxWalkSpeed = 450.f;
    CDO->GetCharacterMovement()->MaxWalkSpeedCrouched = 100.f;
    CDO->SprintMaxWalkSpeed = 650.f;
    if (auto* Launch = FindFProperty<FFloatProperty>(UCombatComponent::StaticClass(), TEXT("UppercutLaunchVelocity"))) Launch->SetPropertyValue_InContainer(CDO->CombatComponent, 720.f);
    auto* Heavy = Asset<UInputAction>(TEXT("Input/IA_HeavyAttack")); Heavy->ValueType = EInputActionValueType::Boolean;
    CDO->AirDiveAction = Heavy;
    if (!CDO->DefaultMappingContext) return false;
    bool bMapped = false;
    for (const auto& Mapping : CDO->DefaultMappingContext->GetMappings())
    {
        UE_LOG(LogTemp, Display, TEXT("Sword input: %s -> %s"), *Mapping.Key.ToString(), *GetNameSafe(Mapping.Action));
        if (Mapping.Action == Heavy) bMapped = true;
    }
    if (!bMapped)
    {
        for (const FKey Key : { EKeys::R, EKeys::F, EKeys::G, EKeys::H })
            if (!CDO->DefaultMappingContext->GetMappings().ContainsByPredicate([Key](const FEnhancedActionKeyMapping& M) { return M.Key == Key; }))
            {
                CDO->DefaultMappingContext->MapKey(Heavy, Key); bMapped = true;
                UE_LOG(LogTemp, Display, TEXT("Sword HeavyAttack mapped to %s; existing mappings retained."), *Key.ToString());
                break;
            }
        if (!bMapped) return false;
    }
    auto MapFree = [CDO](UInputAction* Action, const TArray<FKey>& Choices)
    {
        for (const auto& M : CDO->DefaultMappingContext->GetMappings()) if (M.Action == Action) return true;
        for (const FKey Key : Choices)
            if (!CDO->DefaultMappingContext->GetMappings().ContainsByPredicate([Key](const FEnhancedActionKeyMapping& M) { return M.Key == Key; }))
            { CDO->DefaultMappingContext->MapKey(Action, Key); UE_LOG(LogTemp, Display, TEXT("Sword %s mapped to %s"), *Action->GetName(), *Key.ToString()); return true; }
        return false;
    };
    CDO->BuffAction = Asset<UInputAction>(TEXT("Input/IA_SwordBuff"));
    CDO->ToggleWeaponAction = Asset<UInputAction>(TEXT("Input/IA_ToggleSword"));
    if (!MapFree(CDO->BuffAction, { EKeys::B, EKeys::G, EKeys::H }) || !MapFree(CDO->ToggleWeaponAction, { EKeys::X, EKeys::V, EKeys::N })) return false;
    if (!Save(Heavy) || !Save(CDO->BuffAction) || !Save(CDO->ToggleWeaponAction) || !Save(CDO->DefaultMappingContext) || !Save(BP)) return false;

    // 02 / 03 are separate four-move weapon styles. They never extend the base input queue.
    for (int32 Style = 2; Style <= 3; ++Style)
    {
        auto* Variant = Asset<UActionSet>(FString::Printf(TEXT("DA_SwordActionSet_Style%02d"), Style));
        Variant->GroundCombo.Reset(); Variant->AirCombo = Set->AirCombo; Variant->Dodges = Set->Dodges; Variant->Turns = Set->Turns;
        Variant->Rising = Set->Rising; Variant->Dive = Set->Dive; Variant->SprintAttack = Set->SprintAttack; Variant->ParryCounter = Set->ParryCounter;
        Variant->Buff = Set->Buff; Variant->DrawWeapon = Set->DrawWeapon; Variant->SheatheWeapon = Set->SheatheWeapon;
        Variant->DoubleJumpMontage = Set->DoubleJumpMontage; Variant->bAllowDoubleJump = Style == 3;
        for (int32 I = 1; I <= 4; ++I)
        {
            auto* Seq = Sequence(FString::Printf(TEXT("Attack_Combo_%02d_%02d_Anim"), Style, I));
            auto* M = Montage(FString::Printf(TEXT("AM_Sword_Style%02d_%02d"), Style, I), Seq);
            if (!M || !Seq) return false;
            ClearProjectWindows(M);
            float LastDamageEnd = .55f; int32 Group = 0;
            // Source-authored blade trails identify distinct swings; each receives its own bounded hit group.
            for (const auto& Event : Seq->Notifies)
                if (Event.NotifyStateClass.IsA<UAnimNotifyState_Trail>())
                {
                    const float From = FMath::Clamp(Event.GetTriggerTime(), .05f, M->GetPlayLength() - .1f);
                    const float To = FMath::Clamp(Event.GetEndTriggerTime(), From + .05f, M->GetPlayLength() - .05f);
                    Damage(M, From, To, FName(*FString::Printf(TEXT("Swing%d"), ++Group))); LastDamageEnd = FMath::Max(LastDamageEnd, To);
                }
            if (!Group) { Damage(M, .25f, .55f); Group = 1; }
            const float ChainAt = FMath::Min(LastDamageEnd + .08f, M->GetPlayLength() - .15f);
            if (I < 4) Chain(M, ChainAt);
            auto* D = Definition(FString::Printf(TEXT("DA_Action_Sword_Style%02d_%02d"), Style, I),
                FString::Printf(TEXT("Sword.Style%d.Light.%d"), Style, I), M, LastDamageEnd + (I < 4 ? .05f : .3f), (I < 4 ? 1.f : 1.4f) / Group);
            D->AllowedNextActions.Reset();
            if (I < 4) D->AllowedNextActions.Add(FName(*FString::Printf(TEXT("Sword.Style%d.Light.%d"), Style, I+1)));
            FVector Previous = Seq->ExtractRootTrackTransform(0.f, nullptr).GetTranslation(); float PathLength = 0.f;
            const int32 Samples = FMath::CeilToInt(Seq->GetPlayLength() * 60.f);
            for (int32 Sample = 1; Sample <= Samples; ++Sample)
            {
                const FVector Current = Seq->ExtractRootTrackTransform(Seq->GetPlayLength() * Sample / Samples, nullptr).GetTranslation();
                PathLength += FVector::Dist2D(Previous, Current); Previous = Current;
            }
            D->RootMotionTranslationScale = FMath::Min(1.f, 160.f / FMath::Max(1.f, PathLength));
            D->PoiseDamage = I < 4 ? 10.f / Group : 20.f / Group;
            if (!Save(M) || !Save(D)) return false;
            Variant->GroundCombo.Add(D);
        }
        auto* StyleWeapon = Asset<UWeaponDefinition>(FString::Printf(TEXT("DA_Sword_Style%02d"), Style));
        StyleWeapon->WeaponId = FName(*FString::Printf(TEXT("Sword.Style%d"), Style)); StyleWeapon->DisplayName = FText::FromString(Style == 2 ? TEXT("Flow Sword") : TEXT("Aerial Sword"));
        StyleWeapon->ActionSet = Variant; StyleWeapon->WeaponActorClass = Weapon->WeaponActorClass;
        StyleWeapon->Damage = Weapon->Damage; StyleWeapon->AttackCooldown = Weapon->AttackCooldown; StyleWeapon->EquipSocketName = Weapon->EquipSocketName;
        StyleWeapon->BladeBaseSocketName = Weapon->BladeBaseSocketName; StyleWeapon->BladeTipSocketName = Weapon->BladeTipSocketName;
        StyleWeapon->MeleeHitEffect = Weapon->MeleeHitEffect; StyleWeapon->AttackMontages.Reset();
        for (const UActionDefinition* D : Variant->GroundCombo) StyleWeapon->AttackMontages.Add(D->Montage);
        if (!Save(Variant) || !Save(StyleWeapon)) return false;
    }
    if (!CalibrateBlendSpace(Load<UBlendSpace>(TEXT("/Game/Third/Input/BS_SwordLocomotion")), false) ||
        !CalibrateBlendSpace(Load<UBlendSpace>(TEXT("/Game/Third/Input/BS_Crouch_Locomotion")), true)) return false;
    for (const TCHAR* Name : { TEXT("Jump_In_Anim"), TEXT("Jump_Loop_Anim"), TEXT("Jump_Out_Anim") })
        if (!Sequence(Name, false, true)) return false;
    auto* AirStart = Sequence(TEXT("Hit_Drop_03_Start_Anim"), true, true);
    auto* AirLoop = Sequence(TEXT("Hit_Drop_03_Loop_Anim"), true, true);
    auto* AirHit = Montage(TEXT("AM_Enemy_LaunchHit"), AirStart);
    auto* LandHit = Montage(TEXT("AM_Enemy_LaunchLand"), Sequence(TEXT("Hit_Drop_03_End_Anim"), true, true));
    auto* Down = Montage(TEXT("AM_Enemy_Down01_Idle"), Sequence(TEXT("Down_01_Idle_Anim"), true, true), true);
    auto* DownHit = Montage(TEXT("AM_Enemy_Down01_Hit"), Sequence(TEXT("Down_01_Hit_Anim"), true, true));
    auto* GetUp = Montage(TEXT("AM_Enemy_Down01_GetUp"), Sequence(TEXT("Down_01_Getup_Anim"), true, true));
    if (!AirHit || !AirLoop || !LandHit || !Down || !DownHit || !GetUp) return false;
    auto& AirSegments = AirHit->SlotAnimTracks[0].AnimTrack.AnimSegments;
    AirSegments.SetNum(1);
    FAnimSegment AirLoopSegment; AirLoopSegment.SetAnimReference(AirLoop, true); AirLoopSegment.StartPos = AirStart->GetPlayLength();
    AirSegments.Add(AirLoopSegment); AirHit->SetCompositeLength(AirHit->CalculateSequenceLength());
    AirHit->CompositeSections.Reset(); AirHit->AddAnimCompositeSection(TEXT("Start"), 0.f); AirHit->AddAnimCompositeSection(TEXT("Loop"), AirStart->GetPlayLength());
    AirHit->CompositeSections[0].NextSectionName = TEXT("Loop"); AirHit->CompositeSections[1].NextSectionName = TEXT("Loop");
    AirHit->PostEditChange(); if (!Save(AirHit)) return false;
    for (const TCHAR* Name : { TEXT("BP_EnemyCharacter"), TEXT("BP_EnemyRangedCharacter") })
    {
        auto* EnemyBP = Load<UBlueprint>(TEXT("/Game/Third/Character/") + FString(Name));
        if (!EnemyBP) return false;
        FKismetEditorUtilities::CompileBlueprint(EnemyBP);
        auto* Enemy = Cast<AEnemyCharacter>(EnemyBP->GeneratedClass->GetDefaultObject());
        if (!Enemy) return false;
        Enemy->UppercutLaunchVelocity = 740.f;
        Enemy->UppercutHorizontalVelocity = 80.f;
        Enemy->UppercutHitMontage = AirHit; Enemy->UppercutLandMontage = LandHit;
        Enemy->UppercutDownIdleMontage = Down; Enemy->UppercutDownHitMontage = DownHit; Enemy->UppercutGetUpMontage = GetUp;
        if (!Save(EnemyBP)) return false;
    }
    if (!ApplyGameplayFeedbackAssets()) return false;
    UE_LOG(LogTemp, Display, TEXT("SWORD_ACTION_ASSETS_READY: 4 ground, 2 air, rising, dive, 4 dodge, 4 turn, sprint, counter, buff, draw/sheathe, optional double jump, styles 02/03."));
    return true;
#else
    return false;
#endif
}

bool USwordActionAssetTools::ApplyGameplayFeedbackAssets()
{
#if WITH_EDITOR
    using namespace SwordAssetAuthoring;
    auto* BP = Load<UBlueprint>(TEXT("/Game/Third/Character/BP_TPCCharacter"));
    if (!BP) return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto* Player = Cast<ATPCCharacter>(BP->GeneratedClass->GetDefaultObject());
    if (!Player || !Player->DefaultMappingContext || !Player->SprintAction) return false;
    auto* Context = Player->DefaultMappingContext.Get();
    Context->UnmapAllKeysFromAction(Player->DashAction);
    Context->UnmapAllKeysFromAction(Player->SprintAction);
    Context->MapKey(Player->SprintAction, EKeys::LeftShift);
    Player->SprintHoldThreshold = .2f; Player->LandingContactTime = .12f;
    if (!Save(Context) || !Save(BP)) return false;
    for (const TCHAR* Name : {TEXT("BP_EnemyCharacter"), TEXT("BP_EnemyRangedCharacter")})
    {
        auto* EnemyBP = Load<UBlueprint>(FString(TEXT("/Game/Third/Character/"))+Name);
        if (!EnemyBP) return false;
        FKismetEditorUtilities::CompileBlueprint(EnemyBP);
        auto* Enemy = Cast<AEnemyCharacter>(EnemyBP->GeneratedClass->GetDefaultObject());
        if (!Enemy) return false;
        Enemy->ParryStaggerMinimumDuration = Enemy->ParryStaggerFallbackDuration = 1.4f;
        if (!Save(EnemyBP)) return false;
    }
    auto* Boss = Load<UBossDefinition>(TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss"));
    if (!Boss) return false;
    Boss->ParryRecoil = .7f;
    return Save(Boss) && UpdateSwordAnimationGraphs();
#else
    return false;
#endif
}

FString USwordActionAssetTools::InspectSwordGraphs()
{
    FString Result;
#if WITH_EDITOR
    for (const TCHAR* AssetPath : { TEXT("/Game/Third/Character/ABP_TPCCharacter"), TEXT("/Game/Third/Character/BP_TPCCharacter") })
    {
        auto* BP = SwordAssetAuthoring::Load<UBlueprint>(AssetPath);
        if (!BP) { Result += TEXT("Missing blueprint\n"); continue; }
        Result += FString::Printf(TEXT("BLUEPRINT %s status=%d\n"), *BP->GetName(), static_cast<int32>(BP->Status));
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            Result += FString::Printf(TEXT("GRAPH %s\n"), *Graph->GetName());
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                Result += FString::Printf(TEXT("  NODE %s %s: %s\n"), *Node->GetName(), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction == EGPD_Input && Pin->LinkedTo.IsEmpty() && (!Pin->DefaultValue.IsEmpty() || Pin->DefaultObject))
                        Result += FString::Printf(TEXT("    %s default=%s %s\n"), *Pin->PinName.ToString(), *Pin->DefaultValue, *GetNameSafe(Pin->DefaultObject));
                    for (UEdGraphPin* Other : Pin->LinkedTo)
                        if (Pin->Direction == EGPD_Output) Result += FString::Printf(TEXT("    %s -> %s.%s\n"), *Pin->PinName.ToString(), *Other->GetOwningNode()->GetName(), *Other->PinName.ToString());
                }
            }
        }
    }
    for (const TCHAR* Name : { TEXT("Air_Attack_01_Anim"), TEXT("Air_Attack_02_Anim"), TEXT("Air_Attack_05_Out_Anim") })
        if (auto* Seq = SwordAssetAuthoring::Load<UAnimSequence>(SwordAssetAuthoring::SourceRoot + Name))
            for (const auto& E : Seq->Notifies)
                Result += FString::Printf(TEXT("SOURCE_NOTIFY %s %s %.5f %.5f\n"), Name, *E.NotifyName.ToString(), E.GetTriggerTime(), E.GetEndTriggerTime());
#endif
    return Result;
}
