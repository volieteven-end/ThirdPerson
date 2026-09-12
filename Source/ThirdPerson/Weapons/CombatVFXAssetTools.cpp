#include "CombatVFXAssetTools.h"
#if WITH_EDITOR
#include "WeaponActor.h"
#include "WeaponVFXComponent.h"
#include "WeaponVFXProfile.h"
#include "../Boss/BossDefinition.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace CombatVFXAuthoring
{
const FString Root = TEXT("/Game/Third/Effects/Combat/");
template<class T> T* Load(const FString& Path)
{
    return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path)));
}
bool Save(UObject* Object)
{
    if (!Object) return false;
    Object->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
        *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
template<class T> T* Copy(const FString& Source, const FString& Name)
{
    const FString Path = Root + Name;
    if (FPackageName::DoesPackageExist(Path)) return Load<T>(Path);
    T* Original = Load<T>(Source);
    if (!Original) return nullptr;
    T* Result = DuplicateObject<T>(Original, CreatePackage(*Path), *FPackageName::GetLongPackageAssetName(Path));
    Result->SetFlags(RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);
    return Save(Result) ? Result : nullptr;
}
}

FString UCombatVFXAssetTools::InspectSources()
{
    using namespace CombatVFXAuthoring;
    FString Result;
    for (const TCHAR* Name : {TEXT("SlashTrail/NS_SlashTrail_Basic"), TEXT("SlashTrail/NS_SlashTrail_Ice"),
        TEXT("SlashTrail/NS_SlashTrail_Electric"), TEXT("Sword/NS_Sword_Electric")})
    {
        UNiagaraSystem* System = Load<UNiagaraSystem>(FString(TEXT("/Game/SlashTrailElemental/Niagara/")) + Name);
        if (!System) continue;
        const auto& Store = System->GetExposedParameters();
        TArray<FNiagaraVariable> Variables; Store.GetParameters(Variables);
        for (const auto& V : Variables)
        {
            Result += FString::Printf(TEXT("PARAM %s %s type=%s"), Name, *V.GetName().ToString(), *V.GetType().GetName());
            if (V.GetType() == FNiagaraTypeDefinition::GetFloatDef()) Result += FString::Printf(TEXT(" value=%f"), Store.GetParameterValue<float>(V));
            if (V.GetType() == FNiagaraTypeDefinition::GetVec3Def() || V.GetType() == FNiagaraTypeDefinition::GetPositionDef()) Result += TEXT(" value=") + Store.GetParameterValue<FVector3f>(V).ToString();
            Result += TEXT("\n");
        }
        for (const auto& E : System->GetEmitterHandles())
            if (const auto* D = E.GetInstance().GetEmitterData())
            {
                Result += FString::Printf(TEXT("EMITTER %s %s local=%d enabled=%d\n"), Name, *E.GetName().ToString(), D->bLocalSpace, E.GetIsEnabled());
            }
    }
    const auto* Boss = Load<UBossDefinition>(TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss"));
    if (Boss)
        for (const auto* FX : {Boss->SiphonCastEffect.Get(), Boss->SiphonHitEffect.Get(), Boss->RushSlashEffect.Get(),
            Boss->FeastSlashEffect.Get(), Boss->WaveFlightEffect.Get(), Boss->WaveImpactEffect.Get()})
            if (FX) for (const UParticleEmitter* E : FX->Emitters)
                if (E) for (const UParticleLODLevel* LOD : E->LODLevels)
                    if (LOD && LOD->RequiredModule)
                        Result += FString::Printf(TEXT("CASCADE %s %s loops=%d duration=%.3f\n"), *FX->GetName(), *E->GetName(),
                            LOD->RequiredModule->EmitterLoops, LOD->RequiredModule->EmitterDuration);
    return Result;
}

bool UCombatVFXAssetTools::BuildCombatVFX()
{
    using namespace CombatVFXAuthoring;
    const FString Path = Root + TEXT("DA_SwordElementalVFX");
    UWeaponVFXProfile* Profile = FPackageName::DoesPackageExist(Path) ? Load<UWeaponVFXProfile>(Path) : nullptr;
    if (!Profile)
    {
        Profile = NewObject<UWeaponVFXProfile>(CreatePackage(*Path), TEXT("DA_SwordElementalVFX"), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Profile);
    }
    Profile->BasicTrail = Copy<UNiagaraSystem>(TEXT("/Game/SlashTrailElemental/Niagara/SlashTrail/NS_SlashTrail_Basic"), TEXT("Sword/NS_SwordTrail_Basic"));
    Profile->IceTrail = Copy<UNiagaraSystem>(TEXT("/Game/SlashTrailElemental/Niagara/SlashTrail/NS_SlashTrail_Ice"), TEXT("Sword/NS_SwordTrail_Ice"));
    Profile->ElectricTrail = Copy<UNiagaraSystem>(TEXT("/Game/SlashTrailElemental/Niagara/SlashTrail/NS_SlashTrail_Electric"), TEXT("Sword/NS_SwordTrail_Electric"));
    Profile->BuffSword = Copy<UNiagaraSystem>(TEXT("/Game/SlashTrailElemental/Niagara/Sword/NS_Sword_Electric"), TEXT("Sword/NS_SwordBuff_Electric"));
    Profile->LegacyTrail = Load<UParticleSystem>(TEXT("/Game/SwordAnimsetPro/Demo/FX/Particles/P_Trail"));
    if (!Profile->BasicTrail || !Profile->IceTrail || !Profile->ElectricTrail || !Profile->BuffSword || !Save(Profile)) return false;
    auto* BP = Load<UBlueprint>(TEXT("/Game/Third/Weapon/BP_SwordWeapon"));
    if (!BP || !BP->GeneratedClass) return false;
    // This BP has no SetAttackEffectActive override. Preserve every graph and all unrelated defaults.
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto* CDO = Cast<AWeaponActor>(BP->GeneratedClass->GetDefaultObject());
    if (!CDO || !CDO->WeaponVFX) return false;
    CDO->WeaponVFX->Profile = Profile;
    if (!Save(BP)) return false;

    auto* Boss = Load<UBossDefinition>(TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss"));
    if (!Boss) return false;
    auto CopyFX = [](UParticleSystem* Source, const TCHAR* Name)
    {
        return Source ? Copy<UParticleSystem>(Source->GetOutermost()->GetName(), FString(TEXT("Countess/")) + Name) : nullptr;
    };
    Boss->SiphonCastEffect = CopyFX(Boss->SiphonCastEffect, TEXT("P_SiphonCast"));
    Boss->SiphonHitEffect = CopyFX(Boss->SiphonHitEffect, TEXT("P_SiphonHit"));
    Boss->RushSlashEffect = CopyFX(Boss->RushSlashEffect, TEXT("P_RushSlash"));
    Boss->FeastSlashEffect = CopyFX(Boss->FeastSlashEffect, TEXT("P_FeastSlash"));
    Boss->WaveFlightEffect = CopyFX(Boss->WaveFlightEffect, TEXT("P_BloodWaveFlight"));
    Boss->WaveImpactEffect = CopyFX(Boss->WaveImpactEffect, TEXT("P_BloodWaveImpact"));
    // The vendor "segment" is a short one-shot. A moving pooled projectile needs emission
    // throughout its 1200 cm flight; pool release remains the sole owner of its lifetime.
    if (Boss->WaveFlightEffect)
    {
        for (UParticleEmitter* E : Boss->WaveFlightEffect->Emitters)
            if (E) for (UParticleLODLevel* LOD : E->LODLevels)
                if (LOD && LOD->RequiredModule)
                {
                    LOD->RequiredModule->EmitterLoops = 0;
                    LOD->RequiredModule->EmitterDuration = FMath::Max(.35f, LOD->RequiredModule->EmitterDuration);
                }
        if (!Save(Boss->WaveFlightEffect)) return false;
    }
    return Boss->SiphonCastEffect && Boss->SiphonHitEffect && Boss->RushSlashEffect && Boss->FeastSlashEffect &&
        Boss->WaveFlightEffect && Boss->WaveImpactEffect && Save(Boss);
}
#else
FString UCombatVFXAssetTools::InspectSources() { return TEXT("Editor only"); }
bool UCombatVFXAssetTools::BuildCombatVFX() { return false; }
#endif
