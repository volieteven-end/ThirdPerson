#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Boss/BossDefinition.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Orientation/ParticleModuleOrientationAxisLock.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCountessVFXReadabilityTest, "ThirdPerson.VFX.CountessReadability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCountessVFXReadabilityTest::RunTest(const FString&)
{
    const auto* D = LoadObject<UBossDefinition>(nullptr, TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss"));
    if (!TestNotNull(TEXT("Saved definition"), D)) return false;
    const auto* Feast = D->FindAction(EBossAction::BloodFeast);
    if (!TestNotNull(TEXT("Existing feast move"), Feast) || Feast->Stages.IsEmpty()) return false;
    TestTrue(TEXT("Impact strength independent of compact flight"), D->WaveImpactScale > D->WaveEffectScale);
    for (const auto* FX : {D->FeastEffect.Get(), D->WaveFlightEffect.Get(), D->WaveImpactEffect.Get()})
        if (!TestTrue(TEXT("Only project copies tuned"), FX && FX->GetPathName().StartsWith(TEXT("/Game/Third/Effects/Combat/Countess/")))) return false;
    int32 Cores = 0, Rings = 0, Flashes = 0;
    for (const UParticleEmitter* E : D->WaveFlightEffect->Emitters)
        if (E) for (const UParticleLODLevel* LOD : E->LODLevels)
            if (LOD && LOD->bEnabled && Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
            {
                ++Cores;
                TestTrue(TEXT("Flight core follows projectile instead of scattering behind it"), LOD->RequiredModule->bUseLocalSpace);
                TestTrue(TEXT("Flight core emits continuously"), LOD->SpawnModule->Rate.GetValue() >= 10.f);
                TestEqual(TEXT("Flight core remains pool-owned"), LOD->RequiredModule->EmitterLoops, 0);
            }
    TestTrue(TEXT("At least one active continuous core"), Cores > 0);
    for (const auto* FX : {D->FeastEffect.Get(), D->WaveImpactEffect.Get()})
        for (const UParticleEmitter* E : FX->Emitters)
            if (E) for (const UParticleLODLevel* LOD : E->LODLevels)
            {
                if (!LOD) continue;
                TestTrue(TEXT("Ground/impact systems cannot loop after hit"), LOD->RequiredModule->EmitterLoops > 0);
                const bool bRing = E->EmitterName == TEXT("ReadabilityGroundRing");
                const bool bFlash = E->EmitterName.ToString().Contains(TEXT("Readability")) && !bRing;
                if (!bRing && !bFlash) continue;
                Rings += bRing; Flashes += bFlash;
                TestEqual(TEXT("Accent is a single particle, not a smoke stack"), LOD->SpawnModule->BurstList.Num(), 1);
                if (!LOD->SpawnModule->BurstList.IsEmpty()) TestEqual(TEXT("One burst particle"), LOD->SpawnModule->BurstList[0].Count, 1);
                bool bGroundLocked = false;
                for (UParticleModule* M : LOD->Modules)
                {
                    if (auto* Life = Cast<UParticleModuleLifetime>(M))
                        TestTrue(TEXT("Short visual pulse clears within 0.4 seconds"), Life->Lifetime.GetValue() > 0 && Life->Lifetime.GetValue() <= .4f);
                    if (auto* Size = Cast<UParticleModuleSize>(M); Size && bRing)
                        TestTrue(TEXT("Ring diameter stays inside damage diameter"), Size->StartSize.GetValue().GetMax() <= Feast->Stages[0].Radius * 2.f);
                    if (const auto* Lock = Cast<UParticleModuleOrientationAxisLock>(M)) bGroundLocked |= Lock->LockAxisFlags == EPAL_Z;
                }
                if (bRing)
                {
                    TestTrue(TEXT("Ring faces the ground, not the camera"), bGroundLocked);
                    auto* Material = LOD->RequiredModule->Material.Get();
                    TestTrue(TEXT("Ring tuning does not change vendor material"), Material && Material->GetPathName().StartsWith(TEXT("/Game/Third/Effects/Combat/Countess/")));
                    float FadeDistance = 0;
                    TestTrue(TEXT("Ground fade is short enough for a 6 cm floor offset"), Material &&
                        Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("FadeDistanceA")), FadeDistance) && FadeDistance > 0 && FadeDistance <= 8.f);
                }
            }
    TestTrue(TEXT("Ground ring present"), Rings > 0);
    TestTrue(TEXT("Wave and ground impact flashes present"), Flashes >= 2);
    return true;
}
#endif
