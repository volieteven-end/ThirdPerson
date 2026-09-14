// 武器换手回归：核对剑术骨骼、挂点和动画姿态，防止 UE4 转 UE5 后握剑方向错误。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AnimPose.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwordMotionAssetTest, "ThirdPerson.SwordMotion.AssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSwordMotionAssetTest::RunTest(const FString&)
{
    auto* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    if (!TestNotNull(TEXT("Quinn reopens"), Mesh)) return false;
    const auto* Skeleton = Mesh->GetSkeleton();
    const FName Control(TEXT("sword_motion"));
    TestTrue(TEXT("Control bone survives mesh save/reload"), Mesh->GetRefSkeleton().FindBoneIndex(Control) != INDEX_NONE);
    const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Control);
    if (!TestTrue(TEXT("Control bone survives skeleton save/reload"), BoneIndex != INDEX_NONE)) return false;
    TestEqual(TEXT("Control bone translation is animated, not reset by retargeting"),
        Skeleton->GetBoneTranslationRetargetingMode(BoneIndex), EBoneTranslationRetargetingMode::Animation);
    for (FName Name : {FName(TEXT("weapon_r")), FName(TEXT("SwordTrailBase")), FName(TEXT("SwordTrailTip")),
        FName(TEXT("Weapon_R_Trail_A")), FName(TEXT("Weapon_R_Trail_B"))})
    {
        const auto* Socket = Mesh->FindSocket(Name);
        if (TestNotNull(*Name.ToString(), Socket)) TestEqual(TEXT("Weapon and trail sockets share animated motion"), Socket->BoneName, Control);
    }
    auto* Manny = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    TestTrue(TEXT("Manny has compatible weapon control"), Manny && Manny->GetRefSkeleton().FindBoneIndex(Control) != INDEX_NONE);
    for (int32 Index = 1; Index <= 4; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Attack_Combo_01_%02d_Anim"), Index);
        for (const FString& Path : {FString(TEXT("/Game/Third/SwordAnimation/")) + Name + TEXT(".") + Name,
            FString(TEXT("/Game/Third/Actions/Sword/Sequences/AS_")) + Name + TEXT(".AS_") + Name})
        {
            auto* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
            if (!TestNotNull(*Path, Sequence)) continue;
            TArray<FName> Tracks; Sequence->GetDataModel()->GetBoneTrackNames(Tracks);
            TestTrue(TEXT("Saved sequence contains actual weapon motion track"), Tracks.Contains(Control));
            bool TrailFound = false;
            for (const auto& Event : Sequence->Notifies)
                if (const auto* Trail = Cast<UAnimNotifyState_Trail>(Event.NotifyStateClass))
                {
                    TrailFound = true;
                    TestNotNull(TEXT("Authored trail template retained"), Trail->PSTemplate.Get());
                    TestTrue(TEXT("Both authored trail endpoints resolve on target mesh"),
                        Mesh->FindSocket(Trail->FirstSocketName) && Mesh->FindSocket(Trail->SecondSocketName));
                }
            TestTrue(TEXT("Animation's native trail notify retained"), TrailFound);
            FAnimPoseEvaluationOptions Raw, Compressed; Raw.OptionalSkeletalMesh = Compressed.OptionalSkeletalMesh = Mesh;
            Compressed.EvaluationType = EAnimDataEvalType::Compressed;
            FBox Motion(ForceInit);
            for (double T : {0., .3, .6, .9, 1.2, 1.5})
            {
                FAnimPose R, C;
                UAnimPoseExtensions::GetAnimPoseAtTime(Sequence, T, Raw, R);
                UAnimPoseExtensions::GetAnimPoseAtTime(Sequence, T, Compressed, C);
                const auto& RP = UAnimPoseExtensions::GetBonePose(R, Control);
                const auto& CP = UAnimPoseExtensions::GetBonePose(C, Control);
                Motion += CP.GetTranslation();
                TestTrue(TEXT("Runtime compression retains weapon translation within 1 cm"), RP.GetTranslation().Equals(CP.GetTranslation(), 1.f));
                TestTrue(TEXT("Runtime compression retains weapon rotation within 1 degree"), RP.GetRotation().AngularDistance(CP.GetRotation()) < FMath::DegreesToRadians(1.f));
            }
            if (Index == 2) TestTrue(TEXT("Second attack is not locked to a static right-hand socket"), Motion.GetSize().Size() > 50.f);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwordMotionOrientationTest, "ThirdPerson.SwordMotion.SourceOrientation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSwordMotionOrientationTest::RunTest(const FString&)
{
    // Compare against vendor animation, not against our own baked raw data: a wrong bake can
    // survive compression perfectly. Check the entire motion, including both handoffs.
    for (int32 Index = 1; Index <= 4; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Attack_Combo_01_%02d_Anim"), Index);
        auto* Source = LoadObject<UAnimSequence>(nullptr, *(TEXT("/Game/SwordAnimsetPro/Animations/Root-Motion/") + Name + TEXT(".") + Name));
        if (!TestNotNull(TEXT("Original UE4 animation"), Source)) return false;
        for (const FString& Prefix : {FString(TEXT("/Game/Third/SwordAnimation/")), FString(TEXT("/Game/Third/Actions/Sword/Sequences/AS_"))})
        {
            const FString ObjectName = Prefix.EndsWith(TEXT("AS_")) ? TEXT("AS_") + Name : Name;
            auto* Target = LoadObject<UAnimSequence>(nullptr, *(Prefix + Name + TEXT(".") + ObjectName));
            if (!TestNotNull(TEXT("Repaired animation"), Target)) return false;
            for (const TCHAR* MeshName : {TEXT("SKM_Quinn_Simple"), TEXT("SKM_Manny_Simple")})
            {
                const FString MeshPath = FString(TEXT("/Game/Characters/Mannequins/Meshes/")) + MeshName + TEXT(".") + MeshName;
                auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
                if (!TestNotNull(TEXT("Target proportions"), Mesh)) return false;
                FAnimPoseEvaluationOptions SO, TO; SO.bShouldRetarget = false;
                TO.OptionalSkeletalMesh = Mesh; TO.EvaluationType = EAnimDataEvalType::Compressed;
                float MaxRotationError = 0.f, MaxBladeError = 0.f;
                const int32 Keys = Target->GetDataModel()->GetNumberOfKeys();
                for (int32 Frame = 0; Frame < Keys; ++Frame)
                {
                    const double Alpha = static_cast<double>(Frame) / (Keys - 1);
                    FAnimPose S, T;
                    UAnimPoseExtensions::GetAnimPoseAtTime(Source, Alpha * Source->GetPlayLength(), SO, S);
                    UAnimPoseExtensions::GetAnimPoseAtTime(Target, Alpha * Target->GetPlayLength(), TO, T);
                    if (!S.IsValid() || !T.IsValid()) { AddError(TEXT("Invalid comparison pose")); return false; }
                    const FQuat SQ = UAnimPoseExtensions::GetBonePose(S, TEXT("weapon_r"), EAnimPoseSpaces::World).GetRotation();
                    const FQuat TQ = UAnimPoseExtensions::GetBonePose(T, TEXT("sword_motion"), EAnimPoseSpaces::World).GetRotation();
                    MaxRotationError = FMath::Max(MaxRotationError, FMath::RadiansToDegrees(SQ.AngularDistance(TQ)));
                    MaxBladeError = FMath::Max(MaxBladeError, FMath::RadiansToDegrees(FMath::Acos(
                        FMath::Clamp(FVector::DotProduct(SQ.GetAxisX(), TQ.GetAxisX()), -1., 1.))));
                    if (Index == 2 && Frame == 32)
                        TestTrue(TEXT("Second attack frame 32 is horizontal, not upright"), FMath::Abs(TQ.GetAxisX().Z) < .1);
                }
                AddInfo(FString::Printf(TEXT("%s %s: max orientation %.3f deg, blade direction %.3f deg"),
                    *Target->GetName(), MeshName, MaxRotationError, MaxBladeError));
                TestTrue(TEXT("Full sword rotation agrees with original UE4 clip within one degree"), MaxRotationError < 1.f);
                TestTrue(TEXT("Sword blade direction agrees with original UE4 clip within one degree"), MaxBladeError < 1.f);
            }
        }
    }
    return true;
}
#endif
