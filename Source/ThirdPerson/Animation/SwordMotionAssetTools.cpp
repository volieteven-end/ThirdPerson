#include "SwordMotionAssetTools.h"
#if WITH_EDITOR
#include "AnimPose.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "SkeletonModifier.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetCompilingManager.h"

namespace SwordMotion
{
const FName ControlBone(TEXT("sword_motion"));
const FString MeshRoot(TEXT("/Game/Characters/Mannequins/Meshes/"));
const FString SourceRoot(TEXT("/Game/SwordAnimsetPro/Animations/Root-Motion/"));

template<class T> T* Load(const FString& Path)
{
    return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path)));
}
bool Save(UObject* Object)
{
    Object->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
        *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
FTransform SocketLocal(const USkeletalMeshSocket* S)
{
    return FTransform(S->RelativeRotation, S->RelativeLocation, S->RelativeScale);
}
void SetSocket(USkeleton* Skeleton, FName Name, const FTransform& Transform)
{
    USkeletalMeshSocket* Socket = Skeleton->FindSocket(Name);
    if (!Socket)
    {
        Socket = NewObject<USkeletalMeshSocket>(Skeleton, NAME_None, RF_Transactional);
        Socket->SocketName = Name;
        Skeleton->Sockets.Add(Socket);
    }
    Socket->Modify(); Socket->BoneName = ControlBone;
    Socket->RelativeLocation = Transform.GetTranslation();
    Socket->RelativeRotation = Transform.Rotator();
    Socket->RelativeScale = Transform.GetScale3D();
    Socket->bForceAlwaysAnimated = true;
}
FTransform Bone(const FAnimPose& Pose, FName Name)
{
    return UAnimPoseExtensions::GetBonePose(Pose, Name, EAnimPoseSpaces::World);
}

bool Bake(UAnimSequence* Source, UAnimSequence* Target, USkeletalMesh* TargetMesh,
    const FTransform& SourceGripR, const FTransform& SourceGripL,
    const FTransform& TargetGripR, const FTransform& TargetGripL, bool bRotationOnly)
{
    const auto* Model = Target->GetDataModel();
    const int32 Count = Model->GetNumberOfKeys();
    if (Count < 2 || Source->GetPlayLength() <= 0.f) return false;
    TArray<FName> ExistingTracks; Model->GetBoneTrackNames(ExistingTracks);
    if (bRotationOnly && !ExistingTracks.Contains(ControlBone)) return false;
    TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
    Positions.Reserve(Count); Rotations.Reserve(Count); Scales.Init(FVector3f::OneVector, Count);
    FAnimPoseEvaluationOptions SourceOptions, TargetOptions;
    SourceOptions.bShouldRetarget = false;
    TargetOptions.OptionalSkeletalMesh = TargetMesh;
    FAnimPose SourceStart, TargetStart;
    UAnimPoseExtensions::GetAnimPoseAtTime(Source, 0., SourceOptions, SourceStart);
    UAnimPoseExtensions::GetAnimPoseAtTime(Target, 0., TargetOptions, TargetStart);
    if (!SourceStart.IsValid() || !TargetStart.IsValid()) return false;
    // The hand bones use different local axes after retargeting. Calibrate from corresponding
    // poses, not artist-authored grip socket rotations (HandGrip_L made the blade point up).
    const FQuat RightBasis = Bone(TargetStart, TEXT("hand_r")).GetRotation().Inverse() *
        Bone(SourceStart, TEXT("hand_r")).GetRotation();
    const FQuat LeftBasis = Bone(TargetStart, TEXT("hand_l")).GetRotation().Inverse() *
        Bone(SourceStart, TEXT("hand_l")).GetRotation();
    // Evaluate all input poses before adding keys; subsequent runs produce the same weapon track.
    for (int32 Frame = 0; Frame < Count; ++Frame)
    {
        const double Alpha = static_cast<double>(Frame) / (Count - 1);
        FAnimPose S, T;
        UAnimPoseExtensions::GetAnimPoseAtTime(Source, Alpha * Source->GetPlayLength(), SourceOptions, S);
        UAnimPoseExtensions::GetAnimPoseAtTime(Target, Alpha * Target->GetPlayLength(), TargetOptions, T);
        if (!UAnimPoseExtensions::IsValid(S) || !UAnimPoseExtensions::IsValid(T)) return false;
        const FTransform Weapon = Bone(S, TEXT("weapon_r"));
        const FTransform SourceR = SourceGripR * Bone(S, TEXT("hand_r"));
        const FTransform SourceL = SourceGripL * Bone(S, TEXT("hand_l"));
        const FTransform TargetR = TargetGripR * Bone(T, TEXT("hand_r"));
        const FTransform TargetL = TargetGripL * Bone(T, TEXT("hand_l"));
        const float RightDistance = FVector::Distance(Weapon.GetTranslation(), SourceR.GetTranslation());
        const float LeftDistance = FVector::Distance(Weapon.GetTranslation(), SourceL.GetTranslation());
        // Grip-space retargeting adapts proportions while retaining the source's flips and release motion.
        // Smoothly change anchor near a handoff; never teleport between two sockets at a timer boundary.
        const float LeftWeight = FMath::SmoothStep(-12.f, 12.f, RightDistance - LeftDistance);
        FTransform RightMapped = Weapon.GetRelativeTransform(SourceR) * TargetR;
        FTransform LeftMapped = Weapon.GetRelativeTransform(SourceL) * TargetL;
        // Keep the established grip positions. Correct orientation separately so a socket-axis
        // correction cannot orbit the sword hilt away from the hand, including during release.
        RightMapped.SetRotation(Bone(T, TEXT("hand_r")).GetRotation() * RightBasis *
            Bone(S, TEXT("hand_r")).GetRotation().Inverse() * Weapon.GetRotation());
        LeftMapped.SetRotation(Bone(T, TEXT("hand_l")).GetRotation() * LeftBasis *
            Bone(S, TEXT("hand_l")).GetRotation().Inverse() * Weapon.GetRotation());
        FTransform World;
        World.Blend(RightMapped, LeftMapped, LeftWeight);
        FTransform Local = World.GetRelativeTransform(Bone(T, TEXT("hand_r")));
        Local.NormalizeRotation();
        if (Local.ContainsNaN()) return false;
        if (bRotationOnly)
        {
            const FTransform Existing = Model->GetBoneTrackTransform(ControlBone, FFrameNumber(Frame));
            Positions.Add(FVector3f(Existing.GetTranslation())); Scales[Frame] = FVector3f(Existing.GetScale3D());
        }
        else Positions.Add(FVector3f(Local.GetTranslation()));
        Rotations.Add(FQuat4f(Local.GetRotation()));
    }
    Target->Modify();
    auto& Controller = Target->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Restore source sword motion only")), false);
    TArray<FName> Tracks; Model->GetBoneTrackNames(Tracks);
    const bool Added = Tracks.Contains(ControlBone) || Controller.AddBoneCurve(ControlBone, false);
    const bool Written = Added && Controller.SetBoneTrackKeys(ControlBone, Positions, Rotations, Scales, false);
    Controller.CloseBracket(false);
    if (!Written) return false;
    // Only the new track was changed: damage notifies, root motion, timing and body tracks remain authored.
    Target->PostEditChange();
    UE_LOG(LogTemp, Display, TEXT("SWORD_MOTION baked %s keys=%d notifies=%d"), *Target->GetPathName(), Count, Target->Notifies.Num());
    return Save(Target);
}
}
#endif

bool USwordMotionAssetTools::BuildSwordMotion(bool bRotationOnly)
{
#if WITH_EDITOR
    using namespace SwordMotion;
    auto* Quinn = Load<USkeletalMesh>(MeshRoot + TEXT("SKM_Quinn_Simple"));
    auto* Manny = Load<USkeletalMesh>(MeshRoot + TEXT("SKM_Manny_Simple"));
    auto* SourceMesh = Load<USkeletalMesh>(TEXT("/Game/SwordAnimsetPro/Demo/Character/Mesh/SK_Mannequin"));
    if (!Quinn || !Manny || !SourceMesh || Quinn->GetSkeleton() != Manny->GetSkeleton()) return false;
    USkeleton* Skeleton = Quinn->GetSkeleton();
    const auto* Equip = Skeleton->FindSocket(TEXT("weapon_r"));
    const auto* Left = Skeleton->FindSocket(TEXT("HandGrip_L"));
    if (!Equip || !Left) return false;
    const auto& SourceRef = SourceMesh->GetRefSkeleton();
    const int32 SR = SourceRef.FindBoneIndex(TEXT("weapon_r")), SL = SourceRef.FindBoneIndex(TEXT("weapon_l"));
    if (SR == INDEX_NONE || SL == INDEX_NONE) return false;
    const FTransform SourceGripR = SourceRef.GetRefBonePose()[SR];
    const FTransform SourceGripL = SourceRef.GetRefBonePose()[SL];
    const int32 Existing = Quinn->GetRefSkeleton().FindBoneIndex(ControlBone);
    if (bRotationOnly && (Existing == INDEX_NONE || Manny->GetRefSkeleton().FindBoneIndex(ControlBone) == INDEX_NONE ||
        Equip->BoneName != ControlBone)) return false;
    const FTransform TargetGripR = Existing != INDEX_NONE ? Quinn->GetRefSkeleton().GetRefBonePose()[Existing] : SocketLocal(Equip);
    const FTransform TargetGripL = SocketLocal(Left);
    struct FPair { UAnimSequence* Source; UAnimSequence* Target; };
    TArray<FPair> Pairs;
    for (int32 Index = 1; Index <= 4; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Attack_Combo_01_%02d_Anim"), Index);
        auto* Source = Load<UAnimSequence>(SourceRoot + Name);
        for (const FString& Path : {FString(TEXT("/Game/Third/SwordAnimation/")) + Name,
            FString(TEXT("/Game/Third/Actions/Sword/Sequences/AS_")) + Name})
        {
            auto* Target = Load<UAnimSequence>(Path);
            if (!Source || !Target || Target->GetSkeleton() != Skeleton) return false;
            Pairs.Add({Source, Target});
        }
    }
    if (!bRotationOnly)
    {
        Skeleton->Modify();
        for (auto* Mesh : {Quinn, Manny})
        {
            if (Mesh->GetRefSkeleton().FindBoneIndex(ControlBone) == INDEX_NONE)
            {
                auto* Modifier = NewObject<USkeletonModifier>();
                if (!Modifier->SetSkeletalMesh(Mesh) || !Modifier->AddBone(ControlBone, TEXT("hand_r"), TargetGripR) ||
                    !Modifier->CommitSkeletonToSkeletalMesh()) return false;
            }
        }
        const int32 NewBone = Skeleton->GetReferenceSkeleton().FindBoneIndex(ControlBone);
        if (NewBone == INDEX_NONE) return false;
        Skeleton->SetBoneTranslationRetargetingMode(NewBone, EBoneTranslationRetargetingMode::Animation, false);
        SetSocket(Skeleton, TEXT("weapon_r"), FTransform::Identity);
        // Existing animation-notify names now follow the same animated control bone as the actual weapon.
        for (const FName Name : {FName(TEXT("Weapon_R_Trail_A")), FName(TEXT("Weapon_R_Trail_B"))})
        {
            const auto* SourceSocket = SourceMesh->FindSocket(Name);
            if (!SourceSocket) return false;
            SetSocket(Skeleton, Name, SocketLocal(SourceSocket));
        }
        auto* SwordMesh = Load<USkeletalMesh>(TEXT("/Game/SwordAnimsetPro/Demo/Character/Mesh/Weapon_Sword"));
        if (!SwordMesh) return false;
        for (const auto& Names : {TPair<FName,FName>(TEXT("SwordTrailBase"), TEXT("BladeBase")),
            TPair<FName,FName>(TEXT("SwordTrailTip"), TEXT("BladeTip"))})
        {
            const auto* Socket = SwordMesh->FindSocket(Names.Value);
            if (!Socket) return false;
            SetSocket(Skeleton, Names.Key, SocketLocal(Socket));
        }
        // Preserve preview attachments, only redirect this sword's right-hand preview to its animated control.
        for (int32 Index = 0; Index < Skeleton->PreviewAttachedAssetContainer.Num(); ++Index)
        {
            auto& Pair = Skeleton->PreviewAttachedAssetContainer[Index];
            if (Pair.GetAttachedObject() == SwordMesh && (Pair.AttachedTo == TEXT("HandGrip_R") || Pair.AttachedTo == TEXT("weapon_r")))
                Pair.AttachedTo = TEXT("weapon_r");
        }
    }
    for (const FPair& Pair : Pairs)
        if (!Bake(Pair.Source, Pair.Target, Quinn, SourceGripR, SourceGripL, TargetGripR, TargetGripL, bRotationOnly)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    return bRotationOnly || (Save(Quinn) && Save(Manny) && Save(Skeleton));
#else
    return false;
#endif
}
