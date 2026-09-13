#include "ArenaAssetTools.h"
#if WITH_EDITOR
#include "ArenaActors.h"
#include "ArenaGameMode.h"
#include "ArenaTravelSubsystem.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/BrushComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Builders/CubeBuilder.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"

namespace ArenaBuild
{
const FName Tag(TEXT("ArenaExpansion_v1"));
const FString Props(TEXT("/Game/green_Island/meshes/construction_props/"));
template<class T> T* Load(const FString& Path)
{ return LoadObject<T>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path))); }
void Mark(AActor* A,const FString& Label)
{ A->Tags.AddUnique(Tag); A->SetActorLabel(TEXT("Arena_")+Label); }
UWorld* Open(const FString& Path)
{
    if (!GEditor) return nullptr;
    if (!FPackageName::DoesPackageExist(Path)) return nullptr;
    if (!FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension()))) return nullptr;
    return GEditor->GetEditorWorldContext().World();
}
bool AlreadyBuilt(UWorld* W)
{
    for (TActorIterator<AArenaPortal> It(W); It; ++It) if (It->ActorHasTag(Tag)) return true;
    return false;
}
bool Save(UWorld* W,const FString& Path)
{
    const bool OK=FEditorFileUtils::SaveLevel(W->PersistentLevel,FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension()));
    UE_LOG(LogTemp,Display,TEXT("ARENA_MAP saved=%d map=%s"),OK,*Path); return OK;
}
AStaticMeshActor* Mesh(UWorld* W,const FString& Name,UStaticMesh* Asset,FVector Pos,FVector Scale,float Yaw=0,bool Solid=false,UMaterialInterface* Material=nullptr)
{
    if (!Asset) return nullptr;
    auto* A=W->SpawnActor<AStaticMeshActor>(Pos,FRotator(0,Yaw,0)); Mark(A,Name);
    auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(Asset);
    C->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision")); C->SetCanEverAffectNavigation(Solid);
    if (Material) C->SetMaterial(0,Material);
    A->SetActorScale3D(Scale); return A;
}
AStaticMeshActor* Box(UWorld* W,const FString& Name,FVector Pos,FVector Size,UMaterialInterface* Material=nullptr)
{ return Mesh(W,Name,Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube")),Pos,Size/100.f,0,true,Material); }
void Sign(UWorld* W,FVector Position,float Yaw,const FString& Text,FColor Color)
{
    auto* A=W->SpawnActor<AActor>(); Mark(A,TEXT("Sign_")+Text.Replace(TEXT("\n"),TEXT("_")));
    auto* T=NewObject<UTextRenderComponent>(A,TEXT("Label"),RF_Transactional);
    A->SetRootComponent(T); A->AddInstanceComponent(T); T->SetMobility(EComponentMobility::Static);
    T->SetText(FText::FromString(Text)); T->SetWorldSize(36); T->SetHorizontalAlignment(EHTA_Center);
    T->SetVerticalAlignment(EVRTA_TextCenter); T->SetTextRenderColor(Color); T->RegisterComponent();
    A->SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));
}
void Start(UWorld* W,FVector Pos,float Yaw,FName Name)
{
    auto* S=W->SpawnActor<APlayerStart>(Pos,FRotator(0,Yaw,0)); S->PlayerStartTag=Name; Mark(S,Name.ToString());
}
void Portal(UWorld* W,FVector Ground,float Facing,const TCHAR* Destination,FName Arrival,const FText& Label,const FString& SignText,FColor Color)
{
    auto* P=W->SpawnActor<AArenaPortal>(Ground+FVector(0,0,140),FRotator(0,Facing,0)); Mark(P,SignText);
    P->Destination=TSoftObjectPtr<UWorld>(FSoftObjectPath(FString(Destination)+TEXT(".")+FPackageName::GetLongPackageAssetName(Destination)));
    P->ArrivalTag=Arrival; P->Label=Label;
    Mesh(W,TEXT("GateArch"),Load<UStaticMesh>(Props+TEXT("SM_wall_arch")),Ground,FVector(.38f,.8f,.475f),Facing+90,false);
    const FVector Right=FRotator(0,Facing,0).RotateVector(FVector::RightVector);
    for (float Side : {-1.f,1.f}) Mesh(W,TEXT("GatePillar"),Load<UStaticMesh>(Props+TEXT("SM_column_c")),Ground+Right*Side*210,FVector(.38f,.38f,.95f),Facing,false);
    Sign(W,Ground+FRotator(0,Facing,0).Vector()*80+FVector(0,0,420),Facing,SignText+TEXT("\n[E]"),Color);
}
bool Navigation(UWorld* W,const FBox& BoxBounds)
{
    auto* V=W->SpawnActor<ANavMeshBoundsVolume>(BoxBounds.GetCenter(),FRotator::ZeroRotator); Mark(V,TEXT("Navigation"));
    V->Brush=NewObject<UModel>(V,NAME_None,RF_Transactional); V->Brush->Initialize(V,true);
    V->Brush->Polys=NewObject<UPolys>(V->Brush,NAME_None,RF_Transactional); V->GetBrushComponent()->Brush=V->Brush;
    auto* B=NewObject<UCubeBuilder>(V); const FVector Size=BoxBounds.GetSize(); B->X=Size.X; B->Y=Size.Y; B->Z=Size.Z; V->BrushBuilder=B;
    if (!B->Build(W,V)) return false;
    V->GetBrushComponent()->BuildSimpleBrushCollision(); V->GetBrushComponent()->UpdateBounds(); V->PostEditChange();
    FlushAsyncLoading(); W->UpdateWorldComponents(true,false);
    auto* N=UNavigationSystemV1::GetCurrent(W); if (!N) return false;
    N->OnNavigationBoundsUpdated(V); N->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
    N->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    N->Build(); for (TActorIterator<ANavigationData> It(W); It; ++It) It->EnsureBuildCompletion();
    return N->GetDefaultNavDataInstance() != nullptr;
}
void Perimeter(UWorld* W,const FBox& Floor)
{
    const FVector C=Floor.GetCenter(), E=Floor.GetExtent(); const float Z=Floor.Max.Z;
    auto* Material=Load<UMaterialInterface>(TEXT("/Game/RoyalCapital/Materials/M_RC_Stone"));
    // Full visible perimeter, with a 7 m opening on the arrival side; no invisible arena walls.
    Box(W,TEXT("EastBoundary"),FVector(Floor.Max.X-20,C.Y,Z+150),FVector(40,E.Y*2,300),Material);
    for (float Side : {-1.f,1.f})
    {
        Box(W,TEXT("SideBoundary"),FVector(C.X,C.Y+Side*(E.Y-20),Z+150),FVector(E.X*2,40,300),Material);
        Box(W,TEXT("EntryBoundary"),FVector(Floor.Min.X+20,C.Y+Side*(E.Y+350)/2,Z+150),FVector(40,E.Y-350,300),Material);
        for (int32 I=0; I<5; ++I)
            Mesh(W,TEXT("PerimeterRuins"),Load<UStaticMesh>(Props+TEXT("SM_column_c")),FVector(FMath::Lerp(Floor.Min.X+250,Floor.Max.X-250,I/4.f),C.Y+Side*(E.Y-80),Z),FVector(.7f,.7f,1.f),0,false);
    }
}
bool Tutorial()
{
    auto* W=Open(UArenaTravelSubsystem::TutorialMap()); if (!W) return false;
    if (AlreadyBuilt(W)) return true;
    APlayerStart* Origin=nullptr;
    for (TActorIterator<APlayerStart> It(W); It; ++It) if (It->PlayerStartTag==TEXT("Tutorial_0")) { Origin=*It; break; }
    if (!Origin) return false;
    FVector Ground=Origin->GetActorLocation(); Ground.Z-=112;
    const FVector Forward=Origin->GetActorForwardVector(), Right=Origin->GetActorRightVector();
    for (float Side : {-1.f,1.f})
    {
        const FVector Gate=Ground+Forward*120+Right*Side*650;
        const float Facing=(-Right*Side).Rotation().Yaw;
        Portal(W,Gate,Facing,Side>0?UArenaTravelSubsystem::BossMap():UArenaTravelSubsystem::WaveMap(),TEXT("ArenaArrival"),
            FText::FromString(Side>0?TEXT("进入 Countess Boss 场地"):TEXT("进入随机战斗场")),Side>0?TEXT("COUNTESS BOSS"):TEXT("RANDOM ARENA"),Side>0?FColor(255,120,115):FColor(120,215,255));
        Start(W,Gate-Right*Side*280+FVector(0,0,112),Origin->GetActorRotation().Yaw,Side>0?TEXT("FromBoss"):TEXT("FromRandomArena"));
        for (int32 I=1; I<=3; ++I) Mesh(W,TEXT("HubPath"),Load<UStaticMesh>(Props+TEXT("SM_floor_tile")),Ground+Forward*120+Right*Side*I*180+FVector(0,0,1),FVector(.48f),0,false);
    }
    return Save(W,UArenaTravelSubsystem::TutorialMap());
}
bool Boss()
{
    auto* W=Open(UArenaTravelSubsystem::BossMap()); if (!W) return false;
    if (AlreadyBuilt(W)) return true;
    ACountessBossCharacter* Boss=nullptr;
    for (TActorIterator<ACountessBossCharacter> It(W); It; ++It) { Boss=*It; break; }
    if (!Boss) return false;
    FBox Floor(ForceInit); float BestArea=0;
    for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
    {
        const FBox B=It->GetComponentsBoundingBox(true); const FVector E=B.GetExtent();
        if (E.X>1000 && E.Y>1000 && E.Z<120 && E.X*E.Y>BestArea &&
            Boss->GetActorLocation().X>=B.Min.X && Boss->GetActorLocation().X<=B.Max.X && Boss->GetActorLocation().Y>=B.Min.Y && Boss->GetActorLocation().Y<=B.Max.Y)
        { Floor=B; BestArea=E.X*E.Y; }
    }
    if (!Floor.IsValid) { UE_LOG(LogTemp,Error,TEXT("Arena: cannot identify saved Boss floor; map not changed")); return false; }
    UE_LOG(LogTemp,Display,TEXT("ARENA_BOSS existing floor=%s"),*Floor.ToString());
    auto* Region=W->SpawnActor<AArenaBounds>(FVector(Floor.GetCenter().X,Floor.GetCenter().Y,Floor.Max.Z+450),FRotator::ZeroRotator);
    Mark(Region,TEXT("BossBoundary")); Region->Bounds->SetBoxExtent(FVector(Floor.GetExtent().X-30,Floor.GetExtent().Y-30,1100));
    Boss->BossActions->Modify(); Boss->BossActions->ArenaBoundary=Region;
    const FVector Lobby(Floor.Min.X-425,Floor.GetCenter().Y,Floor.Max.Z);
    Box(W,TEXT("BossArrivalFloor"),Lobby-FVector(0,0,20),FVector(900,700,40));
    Start(W,Lobby+FVector(0,0,112),0,TEXT("ArenaArrival"));
    Portal(W,Lobby-FVector(340,0,0),0,UArenaTravelSubsystem::TutorialMap(),TEXT("FromBoss"),FText::FromString(TEXT("返回教程（重置本次 Boss 挑战）")),TEXT("RETURN TO TUTORIAL"),FColor(245,215,145));
    Perimeter(W,Floor);
    W->GetWorldSettings()->DefaultGameMode=AArenaGameMode::StaticClass();
    FBox Nav(FVector(Floor.Min.X-950,Floor.Min.Y-100,Floor.Max.Z-150),FVector(Floor.Max.X+100,Floor.Max.Y+100,Floor.Max.Z+1000));
    return Navigation(W,Nav) && Save(W,UArenaTravelSubsystem::BossMap());
}
bool Waves()
{
    if (FPackageName::DoesPackageExist(UArenaTravelSubsystem::WaveMap())) return true;
    UWorld* W=GEditor->NewMap(); if (!W) return false;
    W->GetWorldSettings()->DefaultGameMode=AArenaGameMode::StaticClass();
    auto* Grass=Load<UMaterialInterface>(TEXT("/Game/Third/Tutorial/Materials/M_TrainingGrass"));
    auto* Stone=Load<UMaterialInterface>(TEXT("/Game/RoyalCapital/Materials/M_RC_Stone"));
    Box(W,TEXT("ForestGround"),FVector(0,0,-90),FVector(5800,5400,100),Grass);
    Box(W,TEXT("CombatFloor"),FVector(0,0,-20),FVector(4000,4000,40),Stone);
    Box(W,TEXT("ArrivalFloor"),FVector(-2425,0,-20),FVector(900,700,40),Stone);
    auto* Tile=Load<UStaticMesh>(Props+TEXT("SM_floor_tile"));
    for (int32 X=-4; X<=4; ++X) for (int32 Y=-4; Y<=4; ++Y)
        Mesh(W,TEXT("CourtTile"),Tile,FVector(X*400,Y*400,-2),FVector(.95f),0,false);
    Perimeter(W,FBox(FVector(-2000,-2000,-40),FVector(2000,2000,0)));
    for (const FVector Pos : {FVector(550,650,0),FVector(-500,-700,0)})
    {
        Box(W,TEXT("LowCover"),Pos+FVector(0,0,65),FVector(260,120,130),Stone);
        Mesh(W,TEXT("CoverRuins"),Load<UStaticMesh>(Props+TEXT("SM_wall_a")),Pos,FVector(.5f,.6f,.4f),0,false);
    }
    for (float Side : {-1.f,1.f}) for (int32 I=0; I<5; ++I)
        Mesh(W,TEXT("ForestTree"),Load<UStaticMesh>(TEXT("/Game/green_Island/meshes/trees/SM_tree_a")),FVector(-1900+I*900,Side*2350,-35),FVector(.75f),I*47,false);
    auto* Region=W->SpawnActor<AArenaBounds>(FVector(0,0,450),FRotator::ZeroRotator); Mark(Region,TEXT("WaveBoundary"));
    Region->Bounds->SetBoxExtent(FVector(1950,1950,1100));
    auto* Director=W->SpawnActor<AArenaWaveDirector>(); Director->Arena=Region; Mark(Director,TEXT("WaveDirector"));
    Start(W,FVector(-2425,0,112),0,TEXT("ArenaArrival"));
    Portal(W,FVector(-2765,0,0),0,UArenaTravelSubsystem::TutorialMap(),TEXT("FromRandomArena"),FText::FromString(TEXT("返回教程（保留奖励，结束本轮）")),TEXT("RETURN TO TUTORIAL"),FColor(245,215,145));
    auto* Sun=W->SpawnActor<ADirectionalLight>(FVector(0,0,900),FRotator(-50,-30,0)); Mark(Sun,TEXT("Sun"));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sun->GetLightComponent()->SetIntensity(4.f);
    auto* Sky=W->SpawnActor<ASkyLight>(); Mark(Sky,TEXT("Sky")); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sky->GetLightComponent()->SetIntensity(1.f);
    auto* Atmosphere=W->SpawnActor<AActor>(); Mark(Atmosphere,TEXT("Atmosphere"));
    auto* AtmosphereComponent=NewObject<USkyAtmosphereComponent>(Atmosphere,TEXT("SkyAtmosphere"),RF_Transactional);
    Atmosphere->SetRootComponent(AtmosphereComponent); Atmosphere->AddInstanceComponent(AtmosphereComponent); AtmosphereComponent->RegisterComponent();
    return Navigation(W,FBox(FVector(-2950,-2150,-150),FVector(2150,2150,1400))) && Save(W,UArenaTravelSubsystem::WaveMap());
}
bool RefinePresentation()
{
    const FName Revision(TEXT("ArenaPresentation_v2"));
    for (const TCHAR* Path : {UArenaTravelSubsystem::TutorialMap(),UArenaTravelSubsystem::BossMap(),UArenaTravelSubsystem::WaveMap()})
    {
        auto* W=Open(Path); if (!W) return false;
        bool Changed=false;
        // Keep the original movement signboard intact; offset only our new hub additions.
        if (FString(Path)==UArenaTravelSubsystem::TutorialMap())
        {
            const FName Placement(TEXT("ArenaHubPlacement_v3"));
            APlayerStart* Origin=nullptr; AArenaPortal* Anchor=nullptr;
            for (TActorIterator<APlayerStart> It(W); It; ++It) if (It->PlayerStartTag==TEXT("Tutorial_0")) Origin=*It;
            for (TActorIterator<AArenaPortal> It(W); It; ++It) if (It->ActorHasTag(Tag) && !It->ActorHasTag(Placement)) { Anchor=*It; break; }
            if (Origin && Anchor)
            {
                const FVector Forward=Origin->GetActorForwardVector();
                const FVector Shift=Forward*(450.f-FVector::DotProduct(Anchor->GetActorLocation()-Origin->GetActorLocation(),Forward));
                for (TActorIterator<AActor> It(W); It; ++It) if (It->ActorHasTag(Tag) && !It->ActorHasTag(Placement))
                { It->SetActorLocation(It->GetActorLocation()+Shift); It->Tags.Add(Placement); Changed=true; }
            }
        }
        TArray<AArenaPortal*> Portals;
        for (TActorIterator<AArenaPortal> It(W); It; ++It) Portals.Add(*It);
        for (TActorIterator<AActor> It(W); It; ++It)
        {
            if (!It->ActorHasTag(Tag) || It->ActorHasTag(Revision)) continue;
            if (auto* A=Cast<AStaticMeshActor>(*It))
            {
                if (A->GetActorLabel().StartsWith(TEXT("Arena_GateArch")))
                {
                    AArenaPortal* Nearest=nullptr; double Best=MAX_dbl;
                    for (auto* P:Portals) if (const double D=FVector::DistSquared2D(P->GetActorLocation(),A->GetActorLocation()); D<Best) { Best=D; Nearest=P; }
                    if (Nearest) { A->SetActorRotation(Nearest->GetActorRotation()+FRotator(0,90,0)); A->SetActorScale3D(FVector(.38f,.8f,.475f)); Changed=true; }
                }
                else if (A->GetActorLabel().StartsWith(TEXT("Arena_GatePillar")))
                { A->SetActorScale3D(FVector(.38f,.38f,.95f)); Changed=true; }
                else if (FString(Path)==UArenaTravelSubsystem::WaveMap() && A->GetActorLabel().StartsWith(TEXT("Arena_CourtTile")))
                { A->GetStaticMeshComponent()->SetMaterial(0,Load<UMaterialInterface>(TEXT("/Game/RoyalCapital/Materials/M_RC_Paving"))); Changed=true; }
            }
            if (auto* Text=It->FindComponentByClass<UTextRenderComponent>(); Text && It->GetActorLabel().StartsWith(TEXT("Arena_Sign_")))
            {
                AArenaPortal* Nearest=nullptr; double Best=MAX_dbl;
                for (auto* P:Portals) if (const double D=FVector::DistSquared2D(P->GetActorLocation(),It->GetActorLocation()); D<Best) { Best=D; Nearest=P; }
                if (Nearest) { FVector Pos=It->GetActorLocation(); Pos.Z=Nearest->GetActorLocation().Z+280; It->SetActorLocation(Pos); Changed=true; }
            }
            It->Tags.AddUnique(Revision);
        }
        if (FString(Path)==UArenaTravelSubsystem::WaveMap())
        {
            bool HasExposure=false;
            for (TActorIterator<APostProcessVolume> It(W); It; ++It) if (It->ActorHasTag(Revision)) HasExposure=true;
            if (!HasExposure)
            {
                auto* P=W->SpawnActor<APostProcessVolume>(); Mark(P,TEXT("ReadabilityExposure")); P->Tags.Add(Revision); P->bUnbound=true;
                P->Settings.bOverride_AutoExposureBias=true; P->Settings.AutoExposureBias=-1.f;
                P->Settings.bOverride_BloomIntensity=true; P->Settings.BloomIntensity=.15f; Changed=true;
            }
        }
        if (Changed && !Save(W,Path)) return false;
    }
    return true;
}
}
#endif

bool UArenaAssetTools::BuildArenaExpansion()
{
#if WITH_EDITOR
    return ArenaBuild::Waves() && ArenaBuild::Boss() && ArenaBuild::Tutorial() && ArenaBuild::RefinePresentation() && ValidateArenaExpansion();
#else
    return false;
#endif
}
bool UArenaAssetTools::ValidateArenaExpansion()
{
#if WITH_EDITOR
    bool OK=true;
    for (const TCHAR* Path : {UArenaTravelSubsystem::TutorialMap(),UArenaTravelSubsystem::BossMap(),UArenaTravelSubsystem::WaveMap()})
    {
        UWorld* W=ArenaBuild::Open(Path); if (!W) return false;
        int32 Portals=0,Starts=0;
        for (TActorIterator<AArenaPortal> It(W); It; ++It)
        { ++Portals; OK &= FPackageName::DoesPackageExist(It->Destination.ToSoftObjectPath().GetLongPackageName()); }
        for (TActorIterator<APlayerStart> It(W); It; ++It) if (It->ActorHasTag(ArenaBuild::Tag)) ++Starts;
        const bool Tutorial=FString(Path)==UArenaTravelSubsystem::TutorialMap();
        OK &= Portals==(Tutorial?2:1) && Starts==(Tutorial?2:1);
        if (!Tutorial)
        {
            OK &= W->GetWorldSettings()->DefaultGameMode==AArenaGameMode::StaticClass();
            auto* Nav=UNavigationSystemV1::GetCurrent(W);
            for (TActorIterator<APlayerStart> It(W); It; ++It) if (It->PlayerStartTag==TEXT("ArenaArrival"))
            { FNavLocation P; OK &= Nav && Nav->ProjectPointToNavigation(It->GetActorLocation(),P,FVector(120,120,250)); }
        }
        UE_LOG(LogTemp,Display,TEXT("ARENA_VALIDATE map=%s portals=%d arrivalStarts=%d cumulativeOK=%d"),Path,Portals,Starts,OK);
    }
    return OK;
#else
    return false;
#endif
}
