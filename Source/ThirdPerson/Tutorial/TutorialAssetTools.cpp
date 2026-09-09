#include "TutorialAssetTools.h"
#if WITH_EDITOR
#include "TutorialActors.h"
#include "TutorialDirector.h"
#include "TutorialGameMode.h"
#include "TutorialCourse.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/CombatComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/BrushComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Builders/CubeBuilder.h"
#include "Model.h"
#include "Engine/Polys.h"

namespace ForestBuild
{
const FString Root(TEXT("/Game/Third/Tutorial/"));
const FString MapPath = Root + TEXT("Maps/L_ForestTutorial");
const FString Props(TEXT("/Game/green_Island/meshes/construction_props/"));
const FVector Centers[] = {
    {-3900,-2400,0}, {-300,-2400,0}, {2500,-2400,0}, {4400,300,0},
    {2100,2600,0}, {-700,2600,0}, {-3700,2300,0}
};
const FVector Starts[] = {
    {-5150,-2400,112}, {-950,-2400,112}, {2200,-2400,112}, {4400,-300,112},
    {2650,2600,112}, {-30,2600,112}, {-3050,2300,112}
};
const float Yaws[] = {0,0,0,90,180,180,180};

template<class T> T* Load(const FString& Path)
{ return LoadObject<T>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path)),nullptr,LOAD_NoWarn); }
template<class T> T* NewAsset(const FString& Relative)
{
    const FString Path=Root+Relative;
    auto* O=NewObject<T>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
    FAssetRegistryModule::AssetCreated(O); return O;
}
bool Save(UObject* O)
{
    if (!O) return false;
    O->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(O->GetOutermost(),O,*File,Args);
}
UMaterial* Color(const TCHAR* Name,FLinearColor Value,bool Emissive=false)
{
    const FString Path=FString(TEXT("Materials/"))+Name;
    if (auto* Existing=Load<UMaterial>(Root+Path)) return Existing;
    auto* M=NewAsset<UMaterial>(Path); M->TwoSided=true;
    auto* C=NewObject<UMaterialExpressionConstant3Vector>(M); C->Constant=Value;
    M->GetExpressionCollection().AddExpression(C);
    M->GetEditorOnlyData()->BaseColor.Connect(0,C);
    auto* R=NewObject<UMaterialExpressionConstant>(M); R->R=.82f;
    M->GetExpressionCollection().AddExpression(R); M->GetEditorOnlyData()->Roughness.Connect(0,R);
    if(Emissive) M->GetEditorOnlyData()->EmissiveColor.Connect(0,C);
    M->PostEditChange(); return Save(M)?M:nullptr;
}
UMaterial* Grass()
{
    if (auto* Existing=Load<UMaterial>(Root+TEXT("Materials/M_TrainingGrass"))) return Existing;
    auto* Texture=Load<UTexture2D>(TEXT("/Game/green_Island/textures/landscape_textures/grass/T_grass_basecolor"));
    if (!Texture) return nullptr;
    auto* M=NewAsset<UMaterial>(TEXT("Materials/M_TrainingGrass"));
    auto* Pos=NewObject<UMaterialExpressionWorldPosition>(M);
    auto* UV=NewObject<UMaterialExpressionCustom>(M); UV->OutputType=CMOT_Float2; UV->Code=TEXT("return Position.xy / 280.0;");
    FCustomInput Input; Input.InputName=TEXT("Position"); Input.Input.Connect(0,Pos); UV->Inputs.Add(Input);
    auto* T=NewObject<UMaterialExpressionTextureSample>(M); T->Texture=Texture; T->Coordinates.Connect(0,UV);
    auto* Shade=NewObject<UMaterialExpressionCustom>(M); Shade->OutputType=CMOT_Float3;
    Shade->Code=TEXT("float mottling=0.84+0.12*sin(Position.x/460.0)*sin(Position.y/380.0); return Color*float3(0.78,0.88,0.66)*mottling;");
    Input.InputName=TEXT("Position"); Input.Input.Connect(0,Pos); Shade->Inputs.Add(Input);
    Input.InputName=TEXT("Color"); Input.Input.Connect(0,T); Shade->Inputs.Add(Input);
    auto* R=NewObject<UMaterialExpressionConstant>(M); R->R=.96f;
    for (auto* E : TArray<UMaterialExpression*>{Pos,UV,T,Shade,R}) M->GetExpressionCollection().AddExpression(E);
    M->GetEditorOnlyData()->BaseColor.Connect(0,Shade); M->GetEditorOnlyData()->Roughness.Connect(0,R);
    M->PostEditChange(); return Save(M)?M:nullptr;
}
UMaterial* Ring()
{
    if(auto* Existing=Load<UMaterial>(Root+TEXT("Materials/M_TrainingRing")))return Existing;
    auto* M=NewAsset<UMaterial>(TEXT("Materials/M_TrainingRing")); M->BlendMode=BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true;
    auto* UV=NewObject<UMaterialExpressionTextureCoordinate>(M);
    auto* Mask=NewObject<UMaterialExpressionCustom>(M); Mask->OutputType=CMOT_Float1;
    Mask->Code=TEXT("float r=length(UV-0.5)*2; return (1-smoothstep(.96,1.0,r))*(.12+.8*smoothstep(.76,.83,r));");
    FCustomInput I; I.InputName=TEXT("UV"); I.Input.Connect(0,UV); Mask->Inputs.Add(I);
    auto* C=NewObject<UMaterialExpressionConstant3Vector>(M); C->Constant=FLinearColor(.9f,.62f,.14f);
    for(auto* E:TArray<UMaterialExpression*>{UV,Mask,C})M->GetExpressionCollection().AddExpression(E);
    M->GetEditorOnlyData()->Opacity.Connect(0,Mask); M->GetEditorOnlyData()->EmissiveColor.Connect(0,C);
    M->PostEditChange(); return Save(M)?M:nullptr;
}
bool Data(UTutorialCourse*& Course,UWeaponDefinition*& Player,UWeaponDefinition*& Enemy)
{
    Course=Load<UTutorialCourse>(Root+TEXT("Data/DA_ForestTutorial"));
    if(!Course) { Course=NewAsset<UTutorialCourse>(TEXT("Data/DA_ForestTutorial")); UTutorialCourse::PopulateDefaults(*Course); if(!Save(Course))return false; }
    auto* Source=Load<UWeaponDefinition>(TEXT("/Game/Third/DataAsset/DA_TestSword")); if(!Source)return false;
    auto Copy=[Source](const TCHAR* Name)->UWeaponDefinition*
    {
        const FString P=Root+TEXT("Data/")+Name;
        if(auto* E=Load<UWeaponDefinition>(P))return E;
        auto* O=DuplicateObject<UWeaponDefinition>(Source,CreatePackage(*P),FName(Name));
        O->SetFlags(RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(O); return O;
    };
    const bool NewPlayer=!FPackageName::DoesPackageExist(Root+TEXT("Data/DA_TrainingSword"));
    Player=Copy(TEXT("DA_TrainingSword"));
    if(NewPlayer)
    {
        Player->WeaponId=TEXT("TutorialSword"); Player->DisplayName=FText::FromString(TEXT("训练用剑"));
        Player->Damage=20.f; Player->MeleeRange=150.f; Player->MeleeRadius=50.f; Player->AttackCooldown=.5f;
        Player->DiveLandingImpactRadius=120.f;
        Player->ActionSet=Load<UActionSet>(TEXT("/Game/Third/Actions/Sword/DA_SwordActionSet"));
        if(!Player->ActionSet || !Save(Player))return false;
    }
    const bool NewEnemy=!FPackageName::DoesPackageExist(Root+TEXT("Data/DA_TrainingEnemySword"));
    Enemy=Copy(TEXT("DA_TrainingEnemySword"));
    if(NewEnemy)
    {
        Enemy->WeaponId=TEXT("TutorialEnemySword"); Enemy->DisplayName=FText::FromString(TEXT("陪练用剑"));
        Enemy->Damage=8.f; Enemy->MeleeRange=150.f; Enemy->MeleeRadius=50.f; Enemy->AttackCooldown=1.f;
        Enemy->ActionSet=nullptr;
        // Preserve the already-working unarmed enemy's attack slot / notify contract.
        auto* BP=LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
        if(const auto* E=BP?BP->GetDefaultObject<AEnemyCharacter>():nullptr)
        {
            const auto* C=E->FindComponentByClass<UCombatComponent>();
            if(auto* P=FindFProperty<FArrayProperty>(UCombatComponent::StaticClass(),TEXT("AttackMontages")); C && P)
            {
                const auto* Moves=P->ContainerPtrToValuePtr<TArray<TObjectPtr<UAnimMontage>>>(C);
                if(Moves && !Moves->IsEmpty())Enemy->AttackMontages={(*Moves)[0]};
            }
        }
        if(Enemy->AttackMontages.IsEmpty() || !Save(Enemy))return false;
    }
    return true;
}

struct FScene
{
    UWorld* W;
    UStaticMesh* Cube=Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube"));
    UStaticMesh* Cylinder=Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder"));
    UStaticMesh* Plane=Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane"));
    UMaterialInterface* Gold=nullptr;
    UMaterialInterface* Dark=nullptr;
    UMaterialInterface* Wood=nullptr;
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Batches;
    AActor* Instances=nullptr;
    int32 Counter=0;
    explicit FScene(UWorld* World):W(World)
    {
        Gold=Color(TEXT("M_TrainingGold"),FLinearColor(.65f,.44f,.10f),true);
        Dark=Color(TEXT("M_TrainingForest"),FLinearColor(.028f,.095f,.058f));
        Wood=Load<UMaterialInterface>(TEXT("/Game/RoyalCapital/Materials/M_RC_Wood"));
        Instances=W->SpawnActor<AActor>(); Instances->SetActorLabel(TEXT("Tutorial_InstancedScenery"));
        auto* RootComp=NewObject<USceneComponent>(Instances,TEXT("SceneryRoot"),RF_Transactional);
        Instances->SetRootComponent(RootComp); Instances->AddInstanceComponent(RootComp); RootComp->SetMobility(EComponentMobility::Static); RootComp->RegisterComponent();
    }
    AStaticMeshActor* Mesh(const FString& Label,UStaticMesh* M,FVector Pos,FVector Scale=FVector::OneVector,float Yaw=0,UMaterialInterface* Material=nullptr,bool Collision=true)
    {
        if(!M)return nullptr;
        auto* A=W->SpawnActor<AStaticMeshActor>(Pos,FRotator(0,Yaw,0));
        A->SetActorLabel(TEXT("Tutorial_")+Label); A->Tags.Add(TEXT("TutorialGenerated"));
        auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(M);
        C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision")); C->SetCanEverAffectNavigation(Collision);
        if(Material) C->SetMaterial(0,Material); A->SetActorScale3D(Scale); return A;
    }
    AStaticMeshActor* Box(const FString& Label,FVector Pos,FVector Size,UMaterialInterface* Material,bool Collision=true,float Yaw=0)
    { return Mesh(Label,Cube,Pos,Size/100.f,Yaw,Material,Collision); }
    void Instance(UStaticMesh* M,FVector Pos,FVector Scale=FVector::OneVector,float Yaw=0,bool Collision=false)
    {
        if(!M)return;
        const FString Key=M->GetPathName()+(Collision?TEXT("_solid"):TEXT("_decor"));
        auto*& H=Batches.FindOrAdd(Key);
        if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Instances,*FString::Printf(TEXT("Batch_%d"),Batches.Num()),RF_Transactional);
            Instances->AddInstanceComponent(H); H->SetupAttachment(Instances->GetRootComponent()); H->SetMobility(EComponentMobility::Static);
            H->SetStaticMesh(M); H->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision")); H->SetCanEverAffectNavigation(Collision);
            H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),Pos,Scale),true);
    }
    void Board(FVector Pos,float Yaw,const FString& Label)
    {
        Box(TEXT("Signboard"),Pos+FVector(0,0,190),FVector(18,440,160),Dark,false,Yaw);
        Box(TEXT("Signpost"),Pos+FVector(0,0,90),FVector(18,18,180),Wood,true);
        auto* Text=W->SpawnActor<AActor>(Pos+FRotator(0,Yaw,0).Vector()*12+FVector(0,0,210),FRotator(0,Yaw,0));
        Text->SetActorLabel(TEXT("Tutorial_Sign_")+Label.Replace(TEXT("\n"),TEXT("_")));
        auto* T=NewObject<UTextRenderComponent>(Text,TEXT("Engraving"),RF_Transactional);
        Text->SetRootComponent(T); Text->AddInstanceComponent(T); T->SetMobility(EComponentMobility::Static);
        T->SetText(FText::FromString(Label)); T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
        T->SetWorldSize(42); T->SetTextRenderColor(FColor(242,220,155)); T->RegisterComponent();
        // SetRootComponent after SpawnActor needs the authored world transform reapplied.
        Text->SetActorLocationAndRotation(Pos+FRotator(0,Yaw,0).Vector()*12+FVector(0,0,210),FRotator(0,Yaw,0));
    }
    void Flag(FVector Pos,float Yaw=0)
    {
        Mesh(TEXT("BannerPole"),Cylinder,Pos+FVector(0,0,210),FVector(.06f,.06f,4.2f),0,Gold,false);
        Box(TEXT("Banner"),Pos+FVector(0,0,305),FVector(4,110,180),Dark,false,Yaw);
        Box(TEXT("BannerTrim"),Pos+FVector(0,0,212),FVector(6,110,8),Gold,false,Yaw);
    }
};
bool Navigation(UWorld* W)
{
    auto* V=W->SpawnActor<ANavMeshBoundsVolume>(FVector(0,0,400),FRotator::ZeroRotator); V->SetActorLabel(TEXT("Tutorial_Navigation"));
    V->Brush=NewObject<UModel>(V,NAME_None,RF_Transactional); V->Brush->Initialize(V,true);
    V->Brush->Polys=NewObject<UPolys>(V->Brush,NAME_None,RF_Transactional); V->GetBrushComponent()->Brush=V->Brush;
    auto* B=NewObject<UCubeBuilder>(V); B->X=12000; B->Y=9000; B->Z=1600; V->BrushBuilder=B;
    if(!B->Build(W,V))return false;
    V->GetBrushComponent()->BuildSimpleBrushCollision(); V->GetBrushComponent()->UpdateBounds(); V->PostEditChange();
    FlushAsyncLoading(); W->UpdateWorldComponents(true,false);
    for (TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Sets(*It);
        for (auto* Set : Sets) Set->BuildTreeIfOutdated(false,true);
    }
    auto* N=UNavigationSystemV1::GetCurrent(W); if(!N)return false;
    N->OnNavigationBoundsUpdated(V);
    // NewMap initialized an empty world. Rescan now that navigable geometry and bounds exist.
    N->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
    N->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    N->Build(); for(TActorIterator<ANavigationData> It(W);It;++It)It->EnsureBuildCompletion();
    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_NAV bounds=%s nav=%s"),*V->GetComponentsBoundingBox(true).ToString(),*GetNameSafe(N->GetDefaultNavDataInstance()));
    bool Ready=true;
    for(const auto& P:Centers) { FNavLocation Out; const bool Projected=N->ProjectPointToNavigation(P,Out,FVector(150,150,250)); Ready&=Projected;
        UE_LOG(LogTemp,Display,TEXT("TUTORIAL_NAV_POINT %s ready=%d"),*P.ToString(),Projected); }
    return Ready;
}
}
#endif

bool UTutorialAssetTools::BuildForestTutorial()
{
#if WITH_EDITOR
    using namespace ForestBuild;
    // Idempotence is intentional: never silently replace a designer-edited level.
    if(FPackageName::DoesPackageExist(MapPath))
    { UE_LOG(LogTemp,Display,TEXT("Tutorial map already exists; preserving all saved edits.")); return ValidateForestTutorial(); }
    if(!GEditor)return false;
    UTutorialCourse* Course=nullptr; UWeaponDefinition* Player=nullptr; UWeaponDefinition* Enemy=nullptr;
    if(!Data(Course,Player,Enemy))return false;
    auto* Ground=Grass(); auto* Marker=Ring(); if(!Ground||!Marker)return false;
    UWorld* W=GEditor->NewMap(); if(!W)return false;
    W->GetWorldSettings()->DefaultGameMode=ATutorialGameMode::StaticClass();
    ForestBuild::FScene S(W);
    S.Box(TEXT("Ground"),FVector(0,0,-68),FVector(12000,9000,100),Ground);
    auto* Tile=Load<UStaticMesh>(Props+TEXT("SM_floor_tile"));
    auto* Arch=Load<UStaticMesh>(Props+TEXT("SM_wall_arch"));
    auto* Column=Load<UStaticMesh>(Props+TEXT("SM_column_c"));
    auto* Wall=Load<UStaticMesh>(Props+TEXT("SM_wall_a"));
    if(!Tile||!Arch||!Column||!Wall)return false;
    // Broad courts with a continuous two-tile path; all playable floors have collision.
    for(int32 L=1;L<7;++L)
        for(int32 X=-2;X<=2;++X)for(int32 Y=-2;Y<=2;++Y)
            S.Instance(Tile,Centers[L]+FVector(X*400,Y*400,0),FVector::OneVector,0,true);
    const TArray<FVector> Route={ {-5300,-2400,0},{4400,-2400,0},{4400,2600,0},{-5200,2600,0},{-5200,-2400,0} };
    for(int32 I=0;I<Route.Num()-1;++I)
    {
        FVector Delta=Route[I+1]-Route[I]; const float Length=Delta.Size2D(); const FVector Along=Delta.GetSafeNormal();
        const FVector Across(-Along.Y,Along.X,0);
        for(float D=0;D<Length;D+=400)
            for(float Side : {-200.f,200.f}) S.Instance(Tile,Route[I]+Along*D+Across*Side+FVector(0,0,-1),FVector::OneVector,0,true);
    }
    // Low edges and ruins, never tall pillars in the player's combat/camera space.
    for(int32 L=1;L<7;++L)
    {
        for(float Side:{-1.f,1.f})
        {
            S.Instance(Column,Centers[L]+FVector(-1000,Side*1000,0),FVector(.5f,.5f,.65f),0,false);
            S.Instance(Column,Centers[L]+FVector(1000,Side*1000,0),FVector(.5f,.5f,.65f),0,false);
            S.Flag(Centers[L]+FVector(850,Side*1000,0),90);
        }
    }
    // Seven logical areas, checkpoints and permanently saved spawn locations.
    auto* D=W->SpawnActor<ATutorialDirector>(); D->SetActorLabel(TEXT("Tutorial_Director")); D->Course=Course; D->PlayerWeapon=Player; D->EnemyWeapon=Enemy;
    const TCHAR* Titles[]={TEXT("01  MOVEMENT"),TEXT("02  SWORD"),TEXT("03  DODGE"),TEXT("04  GUARD"),TEXT("05  REST"),TEXT("06  COMBAT"),TEXT("07  AIR COMBO")};
    for(int32 L=0;L<7;++L)
    {
        auto* Z=W->SpawnActor<ATutorialZone>(Centers[L]+FVector(0,0,150),FRotator::ZeroRotator);
        Z->SetActorLabel(FString::Printf(TEXT("Tutorial_Entry_%d"),L)); Z->bEntry=true; Z->LessonIndex=L; Z->TargetId=Course->Lessons[L].Id;
        Z->Bounds->SetBoxExtent(L==0?FVector(2000,900,400):FVector(1120,1100,500)); Z->Marker->SetVisibility(false); Z->Number->SetVisibility(false);
        Z->Checkpoint=FTransform(FRotator(0,Yaws[L],0),Starts[L]);
        FVector TargetPos=Centers[L]+FVector(0,0,108);
        if(L==1)TargetPos.X=50; if(L==3)TargetPos.Y=450; if(L==5)TargetPos.X=-550; if(L==6)TargetPos.X=-3700;
        Z->TargetSpawn=FTransform(FRotator(0,Yaws[L]+180,0),TargetPos);
        auto* Start=W->SpawnActor<APlayerStart>(Starts[L],FRotator(0,Yaws[L],0)); Start->PlayerStartTag=FName(*FString::Printf(TEXT("Tutorial_%d"),L));
        Start->SetActorLabel(FString::Printf(TEXT("Tutorial_Checkpoint_%d"),L));
        S.Board(Starts[L]+FVector(0,-620,-112),Yaws[L]+180,Titles[L]);
    }
    auto MakeMarker=[&](int32 L,FName Id,ETutorialSignal Signal,FVector Pos,FVector Extent,const TCHAR* Text)
    {
        auto* Z=W->SpawnActor<ATutorialZone>(Pos+FVector(0,0,130),FRotator::ZeroRotator);
        Z->SetActorLabel(TEXT("Tutorial_Goal_")+Id.ToString()); Z->LessonIndex=L; Z->TargetId=Id; Z->Signal=Signal;
        Z->Bounds->SetBoxExtent(Extent); Z->Marker->SetStaticMesh(S.Plane); Z->Marker->SetMaterial(0,Marker);
        Z->Marker->SetRelativeLocation(FVector(0,0,-117)); Z->Marker->SetRelativeScale3D(FVector(Extent.X*2/100,Extent.Y*2/100,1));
        Z->Number->SetText(FText::FromString(Text)); Z->Number->SetRelativeRotation(FRotator(0,180,0));
    };
    MakeMarker(0,TEXT("Walk01"),ETutorialSignal::WalkMarker,FVector(-4550,-2400,0),FVector(130,180,180),TEXT("1"));
    MakeMarker(0,TEXT("Walk02"),ETutorialSignal::WalkMarker,FVector(-4000,-2400,0),FVector(130,180,180),TEXT("2"));
    MakeMarker(0,TEXT("Sprint"),ETutorialSignal::SprintMarker,FVector(-3000,-2400,0),FVector(150,220,180),TEXT("3"));
    S.Box(TEXT("JumpObstacle"),FVector(-2330,-2400,42),FVector(85,700,64),S.Dark);
    MakeMarker(0,TEXT("Jump"),ETutorialSignal::JumpLanding,FVector(-1890,-2400,0),FVector(210,300,180),TEXT("4"));
    MakeMarker(2,TEXT("DodgeForward"),ETutorialSignal::ForwardDodge,FVector(2550,-2400,0),FVector(160,160,180),TEXT("1"));
    MakeMarker(2,TEXT("DodgeSide"),ETutorialSignal::SideDodge,FVector(2550,-2040,0),FVector(175,175,180),TEXT("2"));
    // Visible gates are supplemented by zone validation, so jumping around one never grants progress.
    const FVector Gates[]={ {-1430,-2400,0},{1320,-2400,0},{4400,-1400,0},{3320,2600,0},{750,2600,0},{-2200,2600,0} };
    const float GateYaw[]={0,0,90,180,180,180};
    for(int32 I=0;I<6;++I)
    {
        S.Mesh(TEXT("RuinedArch"),Arch,Gates[I],FVector(.65f,.85f,1.f),GateYaw[I]+90,nullptr,false);
        auto* G=W->SpawnActor<ATutorialGate>(Gates[I]+FVector(0,0,155),FRotator(0,GateYaw[I],0));
        G->SetActorLabel(FString::Printf(TEXT("Tutorial_Gate_%d"),I+1)); G->UnlockLesson=I+1;
        G->Bars->SetStaticMesh(S.Cube); G->Bars->SetMaterial(0,S.Dark); G->SetActorScale3D(FVector(.22f,5.8f,3.1f));
    }
    // A quiet supply table uses the same stone/wood vocabulary as the ruins.
    const FVector Table=Centers[4]+FVector(0,400,0);
    S.Box(TEXT("SupplyTable"),Table+FVector(0,0,90),FVector(250,110,16),S.Wood);
    for(float X:{-100.f,100.f})for(float Y:{-35.f,35.f})S.Box(TEXT("TableLeg"),Table+FVector(X,Y,42),FVector(15,15,85),S.Wood);
    auto* Bottle=Color(TEXT("M_TrainingBottle"),FLinearColor(.12f,.42f,.26f));
    for(float X:{-50.f,50.f})
    { S.Mesh(TEXT("PotionBottle"),S.Cylinder,Table+FVector(X,0,113),FVector(.20f,.20f,.28f),0,Bottle,false);
      S.Mesh(TEXT("PotionCap"),S.Cylinder,Table+FVector(X,0,132),FVector(.10f,.10f,.12f),0,S.Gold,false); }
    // Perimeter planting. Repeated scenery is instanced; combat courts and connecting paths stay clear.
    auto* TreeA=Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/trees/SM_tree_a"));
    auto* TreeB=Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/trees/SM_tree_b"));
    auto* Bush=Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/plants/SM_bush_a"));
    auto* GrassMesh=Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/plants/SM_grass_a"));
    auto* Stone=Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/stones/SM_stone_b"));
    FRandomStream R(90826);
    for(int32 I=0;I<190;++I)
    {
        FVector P(R.FRandRange(-6200,6200),R.FRandRange(-4600,4600),-15);
        bool NearCourt=false; for(const auto& C:Centers)NearCourt|=FVector::Dist2D(P,C)<1500;
        const bool Central=(FMath::Abs(P.X)<2800 && FMath::Abs(P.Y)<850);
        const bool Outer=FMath::Abs(P.X)>5600 || FMath::Abs(P.Y)>3650;
        if(NearCourt || (!Central&&!Outer))continue;
        const float Scale=R.FRandRange(.75f,1.22f);
        S.Instance(I%2?TreeA:TreeB,P,FVector(Scale),R.FRandRange(0,360),false);
        for(int32 J=0;J<4;++J)S.Instance(Bush,P+FVector(R.FRandRange(-260,260),R.FRandRange(-260,260),0),FVector(R.FRandRange(.6f,1.f)),R.FRandRange(0,360),false);
    }
    for(int32 I=0;I<720;++I)
    {
        FVector P(R.FRandRange(-5800,5800),R.FRandRange(-4200,4200),-14);
        bool NearCourt=false;for(const auto& C:Centers)NearCourt|=(FMath::Abs(P.X-C.X)<1120 && FMath::Abs(P.Y-C.Y)<1120);
        const bool Path=FMath::Abs(P.Y+2400)<650 || FMath::Abs(P.Y-2600)<650 || FMath::Abs(P.X-4400)<650 || FMath::Abs(P.X+5200)<650;
        if(!NearCourt&&!Path)S.Instance(I%7==0?Bush:GrassMesh,P,FVector(R.FRandRange(.65f,1.05f)),R.FRandRange(0,360));
    }
    for(int32 I=0;I<16;++I)
    {
        const float X=-5700+I*760;
        for(float Y:{-4210.f,4210.f}) {S.Instance(Wall,FVector(X,Y,-18),FVector(.95f,1.2f,.36f),0,true);S.Instance(Stone,FVector(X+200,Y-140,-18),FVector(.7f),R.FRandRange(0,360));}
    }
    for(int32 I=0;I<11;++I)for(float X:{-5890.f,5890.f})S.Instance(Wall,FVector(X,-3900+I*760,-18),FVector(.95f,1.2f,.36f),90,true);
    // Daylight with a fixed local exposure: no global rendering settings are changed.
    auto* Sun=W->SpawnActor<ADirectionalLight>(FVector(0,0,1800),FRotator(-42,-32,0)); Sun->SetActorLabel(TEXT("Tutorial_MorningSun"));
    auto* SunC=Cast<UDirectionalLightComponent>(Sun->GetLightComponent()); SunC->SetMobility(EComponentMobility::Movable);
    Sun->SetActorRotation(FRotator(-42,-32,0));
    SunC->SetIntensity(8.f); SunC->SetLightColor(FLinearColor(1.f,.93f,.79f)); SunC->SetAtmosphereSunLight(true);
    // Scattering scale is an absolute coefficient (Earth default ~= .0331), not a multiplier.
    auto* Atmos=W->SpawnActor<ASkyAtmosphere>(); Atmos->SetActorLabel(TEXT("Tutorial_SkyAtmosphere")); Atmos->GetComponent()->SetRayleighScatteringScale(.0265f);
    auto* Sky=W->SpawnActor<ASkyLight>(); Sky->SetActorLabel(TEXT("Tutorial_SkyFill")); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.25f); Sky->GetLightComponent()->SetRealTimeCapture(true);
    auto* Fog=W->SpawnActor<AExponentialHeightFog>(); Fog->SetActorLabel(TEXT("Tutorial_DistantMist"));
    Fog->GetComponent()->SetFogDensity(.008f); Fog->GetComponent()->SetStartDistance(1400.f);
    auto* PP=W->SpawnActor<APostProcessVolume>(); PP->SetActorLabel(TEXT("Tutorial_Exposure")); PP->bUnbound=true;
    PP->Settings.bOverride_AutoExposureMinBrightness=true; PP->Settings.AutoExposureMinBrightness=0.f;
    PP->Settings.bOverride_AutoExposureMaxBrightness=true; PP->Settings.AutoExposureMaxBrightness=0.f;
    PP->Settings.bOverride_BloomIntensity=true; PP->Settings.BloomIntensity=.12f;
    if(!Navigation(W)) { UE_LOG(LogTemp,Error,TEXT("Tutorial navigation did not cover all courts"));return false; }
    const FString File=FPackageName::LongPackageNameToFilename(MapPath,FPackageName::GetMapPackageExtension());
    const bool Saved=FEditorFileUtils::SaveLevel(W->PersistentLevel,File);
    UE_LOG(LogTemp,Display,TEXT("FOREST_TUTORIAL_BUILT saved=%d path=%s"),Saved,*MapPath); return Saved;
#else
    return false;
#endif
}
bool UTutorialAssetTools::ValidateForestTutorial()
{
#if WITH_EDITOR
    using namespace ForestBuild;
    auto* C=Load<UTutorialCourse>(Root+TEXT("Data/DA_ForestTutorial")); FString Error;
    if(!C || !C->IsValidCourse(Error) || !FPackageName::DoesPackageExist(MapPath))return false;
    auto* P=Load<UWeaponDefinition>(Root+TEXT("Data/DA_TrainingSword"));
    auto* E=Load<UWeaponDefinition>(Root+TEXT("Data/DA_TrainingEnemySword"));
    return P && P->ActionSet && P->ActionSet->GroundCombo.Num()==4 && P->Damage==20.f && P->DiveLandingImpactRadius==120.f && E && E->Damage==8.f && !E->AttackMontages.IsEmpty();
#else
    return false;
#endif
}
