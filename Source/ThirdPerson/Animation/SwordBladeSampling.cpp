#include "SwordBladeSampling.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"

bool TPCBladeSampling::SampleEquipmentPose(const UAnimMontage* Montage, float MontageTime,
    const USkeletalMeshComponent* Mesh, FName Socket, FTransform& OutPose)
{
    if (!Montage || !Mesh || Montage->SlotAnimTracks.IsEmpty()) return false;
    const auto* Segment = Montage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(MontageTime);
    const auto* Sequence = Segment ? Cast<UAnimSequence>(Segment->GetAnimReference()) : nullptr;
    if (!Sequence || !Sequence->GetSkeleton()) return false;
    const FName BoneName = Mesh->GetSocketBoneName(Socket);
    const int32 MeshBone = Mesh->GetBoneIndex(BoneName);
    const auto& Ref = Sequence->GetSkeleton()->GetReferenceSkeleton();
    const int32 SkeletonBone = Ref.FindBoneIndex(BoneName);
    if (MeshBone == INDEX_NONE || SkeletonBone == INDEX_NONE) return false;
    const float Time = Segment->ConvertTrackPosToAnimPos(MontageTime);
    FTransform BonePose = FTransform::Identity;
    for (int32 Bone = SkeletonBone; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
    {
        FTransform Local = Ref.GetRefBonePose()[Bone];
        if (Bone == 0 && (Sequence->bEnableRootMotion || Sequence->bForceRootLock))
        {
            if (Sequence->RootMotionRootLock == ERootMotionRootLock::Zero) Local = FTransform::Identity;
            else if (Sequence->RootMotionRootLock == ERootMotionRootLock::AnimFirstFrame)
                Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(0.), false);
        }
        else Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(static_cast<double>(Time)), false);
        BonePose = BonePose * Local;
    }
    const FTransform SocketOffset = Mesh->GetSocketTransform(Socket, RTS_Component).GetRelativeTransform(
        Mesh->GetBoneTransform(MeshBone, FTransform::Identity));
    OutPose = SocketOffset * BonePose;
    return true;
}
