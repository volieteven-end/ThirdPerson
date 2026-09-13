#include "CombatVFXAssetTools.h"
#if WITH_EDITOR
#include "../Boss/BossDefinition.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Size/ParticleModuleSizeScale.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Orientation/ParticleModuleOrientationAxisLock.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Acceleration/ParticleModuleAccelerationConstant.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace CountessVFXAuthoring
{
const TCHAR* DefinitionPath = TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss");
const FString Root = TEXT("/Game/Third/Effects/Combat/Countess/");
bool Save(UObject* Object)
{
    Object->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
        *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
UParticleSystem* ProjectCopy(const TCHAR* Source, const TCHAR* Name)
{
    const FString Path = Root + Name;
    if (FPackageName::DoesPackageExist(Path)) return LoadObject<UParticleSystem>(nullptr, *(Path + TEXT(".") + Name));
    auto* Original = LoadObject<UParticleSystem>(nullptr, Source);
    if (!Original) return nullptr;
    auto* Result = DuplicateObject<UParticleSystem>(Original, CreatePackage(*Path), Name);
    Result->SetFlags(RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);
    return Result;
}
UMaterialInstanceConstant* GroundRingMaterial()
{
    const FString Path = Root + TEXT("MI_FeastGroundRing");
    auto* M = LoadObject<UMaterialInstanceConstant>(nullptr, *(Path + TEXT(".MI_FeastGroundRing")), nullptr, LOAD_NoWarn);
    if (!M)
    {
        auto* Source = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Game/ParagonCountess/FX/Materials/Flares/M_Countess_ImpactFlare_RingInst"));
        if (!Source) return nullptr;
        M = DuplicateObject<UMaterialInstanceConstant>(Source, CreatePackage(*Path), TEXT("MI_FeastGroundRing"));
        M->SetFlags(RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(M);
    }
    // The airborne flare blends over 200 cm. A ring 6 cm above the floor was
    // almost entirely depth-faded; use a short intersection fade on our copy.
    M->SetScalarParameterValueEditorOnly(TEXT("FadeDistanceA"), 8.f);
    M->SetScalarParameterValueEditorOnly(TEXT("FadeDistanceB"), 3.f);
    M->PostEditChange();
    return Save(M) ? M : nullptr;
}
void Constant(UObject* Outer, FRawDistributionFloat& Raw, float Value)
{
    auto* D = NewObject<UDistributionFloatConstant>(Outer); D->Constant = Value; D->bIsDirty = true; Raw.Distribution = D;
}
void Constant(UObject* Outer, FRawDistributionVector& Raw, FVector Value)
{
    auto* D = NewObject<UDistributionVectorConstant>(Outer); D->Constant = Value; D->bIsDirty = true; Raw.Distribution = D;
}
template<class T> T* Module(UParticleSystem* System, UParticleLODLevel* LOD)
{
    auto* M = NewObject<T>(System); M->LODValidity = 1 << LOD->Level; LOD->Modules.Add(M); return M;
}
void Burst(UParticleModuleSpawn* Spawn, int32 Count)
{
    Spawn->BurstList.Reset(); FParticleBurst B; B.Count = Count; B.CountLow = -1; B.Time = 0; Spawn->BurstList.Add(B);
}

// Two particles only: a fast central flash and an expanding ground-locked edge.
// Names make reruns replace our own emitters without stacking or altering the vendor layers.
void AddPulse(UParticleSystem* System, FName Name, UMaterialInterface* Material, float Diameter, float Lifetime, bool bGround)
{
    System->Emitters.RemoveAll([Name](const auto& E) { return E && E->EmitterName == Name; });
    auto* E = NewObject<UParticleSpriteEmitter>(System);
    E->EmitterName = Name; E->CreateLODLevel(0); System->Emitters.Add(E);
    auto* LOD = E->LODLevels[0].Get(); auto* Required = LOD->RequiredModule.Get();
    Required->Material = Material; Required->EmitterLoops = 1; Required->EmitterDuration = Lifetime;
    Required->bUseLocalSpace = false; Required->ScreenAlignment = PSA_Square;
    Constant(LOD->SpawnModule, LOD->SpawnModule->Rate, 0.f); Burst(LOD->SpawnModule, 1);
    auto* Life = Module<UParticleModuleLifetime>(System, LOD); Constant(Life, Life->Lifetime, Lifetime);
    auto* Size = Module<UParticleModuleSize>(System, LOD); Constant(Size, Size->StartSize, FVector(Diameter));
    auto* Location = Module<UParticleModuleLocation>(System, LOD); Constant(Location, Location->StartLocation, FVector(0, 0, bGround ? 6.f : 30.f));
    auto* Color = Module<UParticleModuleColorOverLife>(System, LOD);
    Constant(Color, Color->ColorOverLife, bGround ? FVector(12.f, .12f, .24f) : FVector(24.f, 1.2f, 1.2f));
    auto* Alpha = NewObject<UDistributionFloatConstantCurve>(Color);
    Alpha->ConstantCurve.AddPoint(0.f, 1.f); Alpha->ConstantCurve.AddPoint(.35f, .85f); Alpha->ConstantCurve.AddPoint(1.f, 0.f);
    Alpha->bIsDirty = true; Color->AlphaOverLife.Distribution = Alpha;
    auto* Scale = Module<UParticleModuleSizeScale>(System, LOD);
    auto* Curve = NewObject<UDistributionVectorConstantCurve>(Scale);
    Curve->ConstantCurve.AddPoint(0.f, FVector(bGround ? .15f : .6f));
    Curve->ConstantCurve.AddPoint(.65f, FVector(bGround ? .8f : 1.f));
    Curve->ConstantCurve.AddPoint(1.f, FVector(1.f)); Curve->bIsDirty = true; Scale->SizeScale.Distribution = Curve;
    if (bGround) Module<UParticleModuleOrientationAxisLock>(System, LOD)->LockAxisFlags = EPAL_Z;
    // Match the existing system's LOD count; pulses must not disappear at a LOD transition.
    const int32 NumLODs = System->Emitters[0]->LODLevels.Num();
    for (int32 I = 1; I < NumLODs; ++I) E->CreateLODLevel(I);
}
FString Describe(UObject* Object)
{
    FString Result = Object->GetPathName() + TEXT(" [") + Object->GetClass()->GetName() + TEXT("]\n");
    for (TFieldIterator<FProperty> P(Object->GetClass()); P; ++P)
        if (P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_Transient))
        {
            FString Value;
            P->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
            Result += TEXT("  ") + P->GetName() + TEXT("=") + Value + TEXT("\n");
        }
    return Result;
}
}

FString UCombatVFXAssetTools::InspectCountessReadability()
{
    auto* D = LoadObject<UBossDefinition>(nullptr, TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss"));
    if (!D) return TEXT("Missing Countess definition");
    FString Result;
    for (UParticleSystem* System : {D->WaveFlightEffect.Get(), D->WaveImpactEffect.Get(), D->FeastEffect.Get()})
    {
        if (!System) continue;
        Result += TEXT("SYSTEM ") + System->GetPathName() + TEXT("\n");
        TArray<UObject*> Objects;
        GetObjectsWithOuter(System, Objects, EGetObjectsFlags::IncludeNestedObjects);
        for (UObject* Object : Objects) Result += CountessVFXAuthoring::Describe(Object);
        TSet<UMaterialInterface*> Materials;
        for (UParticleEmitter* E : System->Emitters)
            if (E) for (UParticleLODLevel* LOD : E->LODLevels)
                if (LOD && LOD->RequiredModule && LOD->RequiredModule->Material) Materials.Add(LOD->RequiredModule->Material);
        for (UMaterialInterface* M : Materials) Result += CountessVFXAuthoring::Describe(M);
    }
    return Result;
}

bool UCombatVFXAssetTools::BuildCountessReadability()
{
    using namespace CountessVFXAuthoring;
    auto* D = LoadObject<UBossDefinition>(nullptr, DefinitionPath);
    if (!D) return false;
    auto* Flight = ProjectCopy(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/RollingDark/FX/p_RollingDark_SegmentFX"), TEXT("P_BloodWaveFlight"));
    auto* Impact = ProjectCopy(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/RollingDark/FX/p_RollingDark_ImpactFX"), TEXT("P_BloodWaveImpact"));
    auto* Ground = ProjectCopy(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/Ultimate/FX/p_CountessUlt_GroundImpactFX"), TEXT("P_FeastGroundImpact"));
    auto* Ring = GroundRingMaterial();
    auto* Flash = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ParagonCountess/FX/Materials/Flares/M_Countess_ImpactFlare"));
    const auto* Feast = D->FindAction(EBossAction::BloodFeast);
    if (!Flight || !Impact || !Ground || !Ring || !Flash || !Feast || Feast->Stages.IsEmpty()) return false;
    for (UParticleEmitter* E : Flight->Emitters)
        if (E) for (UParticleLODLevel* LOD : E->LODLevels)
        {
            if (!LOD || !LOD->RequiredModule || !LOD->SpawnModule) continue;
            LOD->RequiredModule->EmitterLoops = 0;
            LOD->RequiredModule->EmitterDuration = FMath::Max(.35f, LOD->RequiredModule->EmitterDuration);
            // Vendor meshes burst once per segment. Keep a compact moving front alive
            // instead of leaving disconnected meshes every 315 cm at 900 cm/s.
            if (Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
            {
                LOD->RequiredModule->bUseLocalSpace = true;
                Constant(LOD->SpawnModule, LOD->SpawnModule->Rate, 12.f); Burst(LOD->SpawnModule, 1);
                for (UParticleModule* M : LOD->Modules)
                {
                    if (auto* Life = Cast<UParticleModuleLifetime>(M)) Constant(Life, Life->Lifetime, .22f);
                    if (auto* Velocity = Cast<UParticleModuleVelocity>(M))
                    { Constant(Velocity, Velocity->StartVelocity, FVector::ZeroVector); Constant(Velocity, Velocity->StartVelocityRadial, 0.f); }
                    if (auto* Acceleration = Cast<UParticleModuleAccelerationConstant>(M)) Acceleration->Acceleration = FVector::ZeroVector;
                }
            }
            else if (E->EmitterName == TEXT("HotSmoke"))
            {
                Constant(LOD->SpawnModule, LOD->SpawnModule->Rate, 14.f); Burst(LOD->SpawnModule, 1);
            }
        }
    AddPulse(Impact, TEXT("ReadabilityImpactFlash"), Flash, 160.f, .18f, false);
    AddPulse(Ground, TEXT("ReadabilityGroundFlash"), Flash, 180.f, .16f, false);
    // Never advertise a damaging radius outside the actual radial hit.
    AddPulse(Ground, TEXT("ReadabilityGroundRing"), Ring, Feast->Stages[0].Radius * 1.9f, .38f, true);
    for (auto* System : {Flight, Impact, Ground})
    {
        System->UpdateAllModuleLists(); System->BuildEmitters(); System->PostEditChange();
        if (!Save(System)) return false;
    }
    D->WaveFlightEffect = Flight; D->WaveImpactEffect = Impact; D->FeastEffect = Ground;
    D->WaveEffectScale = .65f; D->WaveImpactScale = .85f;
    return Save(D);
}
#else
FString UCombatVFXAssetTools::InspectCountessReadability() { return TEXT("Editor only"); }
bool UCombatVFXAssetTools::BuildCountessReadability() { return false; }
#endif
