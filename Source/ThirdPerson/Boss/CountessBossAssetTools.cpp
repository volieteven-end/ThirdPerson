#include "CountessBossAssetTools.h"
#if WITH_EDITOR
#include "BossDefinition.h"
#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "BossDamageWindowNotifyState.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Builders/CubeBuilder.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"

bool BuildCountessGraphAssets(USkeletalMesh* Mesh,UBossDefinition* Definition,UClass*& AnimClass);

namespace
{
const FString Root=TEXT("/Game/Third/Bosses/Countess/");
bool SaveAsset(UObject* Asset)
{
 if (!Asset) return false;
 Asset->MarkPackageDirty();
 const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Args);
}
bool FinishTestNavigation(UWorld* World)
{
 FlushAsyncLoading();
 World->UpdateWorldComponents(true,false);
 auto* Nav=UNavigationSystemV1::GetCurrent(World); if (!Nav) return false;
 for (TActorIterator<ANavMeshBoundsVolume> It(World);It;++It)
 {
  // SpawnActor does not run the editor volume factory: allocate its brush model before building.
  if (It->GetComponentsBoundingBox(true).GetExtent().IsNearlyZero())
  {
   It->Brush=NewObject<UModel>(*It,NAME_None,RF_Transactional); It->Brush->Initialize(*It,true);
   It->Brush->Polys=NewObject<UPolys>(It->Brush,NAME_None,RF_Transactional);
   It->GetBrushComponent()->Brush=It->Brush;
   auto* Builder=NewObject<UCubeBuilder>(*It); Builder->X=4800; Builder->Y=4800; Builder->Z=800;
   It->BrushBuilder=Builder; if (!Builder->Build(World,*It)) return false;
   It->GetBrushComponent()->BuildSimpleBrushCollision(); It->GetBrushComponent()->UpdateBounds(); It->PostEditChange();
  }
  Nav->OnNavigationBoundsUpdated(*It);
 }
 // Python commandlets can still have the new-map async-load lock after all loads have completed.
 Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
 Nav->Build();
 for (TActorIterator<ANavigationData> It(World);It;++It) It->EnsureBuildCompletion();
 FNavLocation Projected;
 const bool bReady=Nav->ProjectPointToNavigation(FVector(-650,0,20),Projected,FVector(100,100,200)) &&
     Nav->ProjectPointToNavigation(FVector(0,0,20),Projected,FVector(100,100,200));
 UE_LOG(LogTemp,Display,TEXT("Countess navigation: player and boss project to navmesh = %d"),bReady);
 return bReady;
}
template<class T> T* Existing(const FString& Relative)
{ const FString Path=Root+Relative; return LoadObject<T>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path)),nullptr,LOAD_NoWarn); }
template<class T> T* NewAsset(const FString& Relative)
{
 const FString Path=Root+Relative;
 T* A=NewObject<T>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
 FAssetRegistryModule::AssetCreated(A); return A;
}
bool BuildMaterial()
{
 if (Existing<UMaterial>(TEXT("Materials/M_BossTelegraph"))) return true;
 auto* M=NewAsset<UMaterial>(TEXT("Materials/M_BossTelegraph"));
 M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true;
 auto* UV=NewObject<UMaterialExpressionTextureCoordinate>(M);
 auto* Circle=NewObject<UMaterialExpressionScalarParameter>(M); Circle->ParameterName=TEXT("Circle"); Circle->DefaultValue=1;
 auto* Color=NewObject<UMaterialExpressionVectorParameter>(M); Color->ParameterName=TEXT("WarningColor"); Color->DefaultValue=FLinearColor(1,.015f,.035f);
 auto* Mask=NewObject<UMaterialExpressionCustom>(M); Mask->OutputType=CMOT_Float1;
 Mask->Code=TEXT("float2 p=UV-0.5; float r=length(p)*2; float ring=(1-smoothstep(0.96,1.0,r))*(0.1+0.8*smoothstep(0.86,0.9,r)); float edge=max(step(0.46,abs(p.x)),step(0.43,abs(p.y))); return lerp(0.12+edge*0.75,ring,Circle);");
 FCustomInput I; I.InputName=TEXT("UV"); I.Input.Connect(0,UV); Mask->Inputs.Add(I);
 I=FCustomInput(); I.InputName=TEXT("Circle"); I.Input.Connect(0,Circle); Mask->Inputs.Add(I);
 const TArray<UMaterialExpression*> Expressions={UV,Circle,Color,Mask};
 for (UMaterialExpression* E:Expressions) M->GetExpressionCollection().AddExpression(E);
 M->GetEditorOnlyData()->EmissiveColor.Connect(0,Color); M->GetEditorOnlyData()->Opacity.Connect(0,Mask);
 M->PostEditChange(); return SaveAsset(M);
}
USkeletalMesh* BuildMesh()
{
 if (auto* ExistingMesh=Existing<USkeletalMesh>(TEXT("Meshes/SK_CountessBoss"))) return ExistingMesh;
 auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Meshes/SM_Countess.SM_Countess"));
 if (!Source || !Source->GetImportedModel() || Source->GetImportedModel()->LODModels.IsEmpty()) return nullptr;
 auto* Mesh=DuplicateObject<USkeletalMesh>(Source,CreatePackage(*(Root+TEXT("Meshes/SK_CountessBoss"))),TEXT("SK_CountessBoss"));
 Mesh->SetFlags(RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(Mesh);
 const auto& Ref=Mesh->GetRefSkeleton(); TArray<FTransform> RefCS=Ref.GetRefBonePose();
 for (int32 I=1;I<RefCS.Num();++I) RefCS[I]=RefCS[I]*RefCS[Ref.GetParentIndex(I)];
 for (const TCHAR* Side:{TEXT("l"),TEXT("r")})
 {
  const FName Bone(*FString::Printf(TEXT("weapon_%s"),Side)); int32 Index=Ref.FindBoneIndex(Bone); if (Index==INDEX_NONE) return nullptr;
  FBox Bounds(ForceInit);
  for (const auto& Section:Mesh->GetImportedModel()->LODModels[0].Sections)
   for (const auto& V:Section.SoftVertices)
   {
    uint32 Total=0,Weight=0;
    for (int32 I=0;I<MAX_TOTAL_INFLUENCES;++I)
    { Total+=V.InfluenceWeights[I]; if (Section.BoneMap.IsValidIndex(V.InfluenceBones[I]) && Section.BoneMap[V.InfluenceBones[I]]==Index) Weight+=V.InfluenceWeights[I]; }
    if (Weight>0 && Weight*2>=Total) Bounds+=RefCS[Index].InverseTransformPosition(FVector(V.Position));
   }
  if (!Bounds.IsValid) return nullptr;
  const FVector Extent=Bounds.GetExtent(); int32 Axis=Extent.X>=Extent.Y && Extent.X>=Extent.Z?0:(Extent.Y>=Extent.Z?1:2);
  FVector Tip=Bounds.GetCenter(); Tip[Axis]=FMath::Abs(Bounds.Min[Axis])>FMath::Abs(Bounds.Max[Axis])?Bounds.Min[Axis]:Bounds.Max[Axis];
  // Bone origin is the weapon grip; weighted blade bounds choose its farthest longitudinal endpoint.
  for (bool bTip:{false,true})
  {
   auto* Socket=NewObject<USkeletalMeshSocket>(Mesh); Socket->SocketName=FName(*FString::Printf(TEXT("Blade%s_%s"),bTip?TEXT("Tip"):TEXT("Base"),*FString(Side).ToUpper()));
   Socket->BoneName=Bone; Socket->RelativeLocation=bTip?Tip:FVector::ZeroVector; Mesh->GetMeshOnlySocketList().Add(Socket);
  }
  UE_LOG(LogTemp,Display,TEXT("Countess %s blade bounds %s; tip %s"),Side,*Bounds.ToString(),*Tip.ToString());
 }
 Mesh->PostEditChange(); return SaveAsset(Mesh)?Mesh:nullptr;
}
}
#endif
bool UCountessBossAssetTools::BuildCountessAssets()
{
#if WITH_EDITOR
 if (!BuildMaterial()) return false;
 auto* Mesh=BuildMesh(); if (!Mesh) return false;
 auto* D=Existing<UBossDefinition>(TEXT("DA_CountessBoss"));
 if (!D)
 {
  D=NewAsset<UBossDefinition>(TEXT("DA_CountessBoss"));
  for (auto& A:D->Actions) for (int32 Index=0;Index<A.Stages.Num();++Index)
  {
   auto& S=A.Stages[Index];
   const FString Name=FString::Printf(TEXT("AM_Countess_%s_%d"),*UEnum::GetValueAsString(A.Id).RightChop(13),Index+1);
   const FString Relative=TEXT("Animations/")+Name;
   auto* M=Existing<UAnimMontage>(Relative);
   if (!M)
   {
    auto* Temp=UAnimMontage::CreateSlotAnimationAsDynamicMontage(S.Sequence,TEXT("DefaultSlot"),.08f,.12f,1,1);
    M=DuplicateObject<UAnimMontage>(Temp,CreatePackage(*(Root+Relative)),*Name); M->ClearFlags(RF_Transient); M->SetFlags(RF_Public|RF_Standalone);
    M->PostEditChange();
    UAnimationBlueprintLibrary::AddAnimationNotifyTrack(M,TEXT("Boss"));
    UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(M,TEXT("Boss"),S.HitStart,S.HitEnd-S.HitStart,UBossDamageWindowNotifyState::StaticClass());
    FAssetRegistryModule::AssetCreated(M); if (!SaveAsset(M)) return false;
   }
   S.Montage=M;
  }
  if (!SaveAsset(D)) return false;
 }
 if (!D->WarningMaterial)
 { D->WarningMaterial=Existing<UMaterial>(TEXT("Materials/M_BossTelegraph")); if (!SaveAsset(D)) return false; }
 for (const auto& A:D->Actions) for (const auto& S:A.Stages)
 {
  float Start=0,End=0;
  if (S.Montage && (!S.ReadHitWindow(Start,End) || End>S.Length()))
  {
   S.Montage->Notifies.Reset();
   UAnimationBlueprintLibrary::AddAnimationNotifyTrack(S.Montage,TEXT("Boss"));
   UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(S.Montage,TEXT("Boss"),S.HitStart,S.HitEnd-S.HitStart,UBossDamageWindowNotifyState::StaticClass());
   if (!SaveAsset(S.Montage)) return false;
  }
  if (S.Montage) for (const auto& Event:S.Montage->Notifies)
   UE_LOG(LogTemp,Display,TEXT("Countess window: %s, notify %s, start %.4f, end %.4f, duration %.4f"),*S.Montage->GetName(),*GetNameSafe(Event.NotifyStateClass),Event.GetTriggerTime(),Event.GetEndTriggerTime(),Event.GetDuration());
 }
 UClass* AnimClass=nullptr; if (!BuildCountessGraphAssets(Mesh,D,AnimClass)) return false;
 auto* BP=Existing<UBlueprint>(TEXT("BP_CountessBoss"));
 if (!BP)
 {
  BP=FKismetEditorUtilities::CreateBlueprint(ACountessBossCharacter::StaticClass(),CreatePackage(*(Root+TEXT("BP_CountessBoss"))),TEXT("BP_CountessBoss"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
  FKismetEditorUtilities::CompileBlueprint(BP);
  auto* CDO=Cast<ACountessBossCharacter>(BP->GeneratedClass->GetDefaultObject());
  CDO->GetMesh()->SetSkeletalMesh(Mesh); CDO->BossActions->Definition=D;
  FAssetRegistryModule::AssetCreated(BP); if (!SaveAsset(BP)) return false;
 }
 auto* CDO=Cast<ACountessBossCharacter>(BP->GeneratedClass->GetDefaultObject());
 CDO->GetMesh()->SetAnimInstanceClass(AnimClass); CDO->BossActions->Definition=D;
 if (!SaveAsset(BP)) return false;
 UE_LOG(LogTemp,Display,TEXT("Countess assets ready: BP_CountessBoss, DA_CountessBoss, mesh sockets, six actions and warning material."));
 return true;
#else
 return false;
#endif
}
bool UCountessBossAssetTools::BuildCountessTestMap()
{
#if WITH_EDITOR
 const FString Path=Root+TEXT("Maps/L_CountessBossTest");
 if (FPackageName::DoesPackageExist(Path))
 {
  // Rebuild only navigation; preserve existing map actors and all of their edited properties.
  const FString ExistingFile=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension());
  if (!GEditor || !FEditorFileUtils::LoadMap(ExistingFile)) return false;
  UWorld* ExistingWorld=GEditor->GetEditorWorldContext().World();
  return FinishTestNavigation(ExistingWorld) && FEditorFileUtils::SaveLevel(ExistingWorld->PersistentLevel,ExistingFile);
 }
 if (!GEditor || !BuildCountessAssets()) return false;
 UWorld* W=GEditor->NewMap(); if (!W) return false;
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Box=[&](FVector Location,FVector Scale,const TCHAR* Label)
 {
  auto* A=W->SpawnActor<AStaticMeshActor>(Location,FRotator::ZeroRotator);
  A->GetStaticMeshComponent()->SetStaticMesh(Cube); A->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
  A->SetActorScale3D(Scale); A->SetActorLabel(Label); return A;
 };
 Box(FVector(0,0,-20),FVector(50,50,.4f),TEXT("Boss arena floor"));
 Box(FVector(650,300,150),FVector(.8f,5,3),TEXT("Rush and LOS wall"));
 Box(FVector(-450,700,15),FVector(3,3,.3f),TEXT("Low step"));
 W->SpawnActor<APlayerStart>(FVector(-650,0,110),FRotator::ZeroRotator);
 auto* BossClass=LoadClass<ACountessBossCharacter>(nullptr,TEXT("/Game/Third/Bosses/Countess/BP_CountessBoss.BP_CountessBoss_C"));
 W->SpawnActor<ACountessBossCharacter>(BossClass,FVector(0,0,100),FRotator(0,180,0));
 auto* Sun=W->SpawnActor<ADirectionalLight>(FVector(0,0,700),FRotator(-50,-35,0)); Sun->GetLightComponent()->SetIntensity(4.f);
 Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
 auto* Sky=W->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetIntensity(1.f); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
 auto* Volume=W->SpawnActor<ANavMeshBoundsVolume>(FVector(0,0,200),FRotator::ZeroRotator);
 auto* Builder=NewObject<UCubeBuilder>(Volume); Builder->X=4800; Builder->Y=4800; Builder->Z=800; Builder->Build(W,Volume);
 Volume->SetActorLabel(TEXT("Boss arena navigation")); Volume->PostEditChange();
 if (auto* Nav=UNavigationSystemV1::GetCurrent(W))
 { Nav->OnNavigationBoundsUpdated(Volume); }
 if (!FinishTestNavigation(W)) return false;
 const FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension());
 const bool bSaved=FEditorFileUtils::SaveLevel(W->PersistentLevel,File);
 UE_LOG(LogTemp,Display,TEXT("Countess test map saved: %s (%s)"),*Path,bSaved?TEXT("yes"):TEXT("no"));
 return bSaved;
#else
 return false;
#endif
}
