#include "InventoryPresentation.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/LevelComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Actions/ActionDefinition.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Weapons/WeaponActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

FPlayerAttributeSnapshot UInventoryPresentation::ReadAttributes(ATPCCharacter* P)
{
    FPlayerAttributeSnapshot S; if (!P) return S;
    S.Level=P->LevelComponent->GetLevel(); S.Experience=P->LevelComponent->GetCurrentExperience(); S.NextLevelExperience=P->LevelComponent->GetExperienceToNextLevel();
    S.Health=P->HealthComponent->GetCurrentHealth(); S.MaxHealth=P->HealthComponent->GetMaxHealth();
    S.Stamina=P->StaminaComponent->GetCurrentStamina(); S.MaxStamina=P->StaminaComponent->GetMaxStamina();
    const auto* Actions=P->CombatComponent->GetActionSet();
    S.bDoubleJump=P->bDoubleJumpUnlocked || (Actions && Actions->bAllowDoubleJump); S.EffectiveBaseDamage=P->CombatComponent->GetPresentationDamage(); S.BuffMultiplier=P->CombatComponent->GetSwordBuffMultiplier();
    const auto* Weapon=P->EquipmentComponent->GetEquippedWeaponDefinition();
    S.bArmed=Weapon && P->EquipmentComponent->IsWeaponDrawn();
    S.WeaponName=Weapon?(Weapon->DisplayName.IsEmpty()?FText::FromString(Weapon->GetName()):Weapon->DisplayName):FText::FromString(TEXT("未装备")); S.WeaponBaseDamage=Weapon?Weapon->Damage:0;
    FString Upgrades; const TCHAR* Names[]={TEXT("生命"),TEXT("力量"),TEXT("迅捷"),TEXT("耐力"),TEXT("长锋"),TEXT("铁肤")};
    TMap<int32,int32> Counts; for (auto U:P->LevelComponent->GetSelectedUpgrades()) ++Counts.FindOrAdd(static_cast<int32>(U));
    for (int32 I=0;I<6;++I) if (Counts.Contains(I)) Upgrades+=FString::Printf(TEXT("%s ×%d  "),Names[I],Counts[I]);
    S.Upgrades=FText::FromString(Upgrades.IsEmpty()?TEXT("暂无"):Upgrades); return S;
}
FText UInventoryPresentation::FormatAttributes(const FPlayerAttributeSnapshot& S)
{
    return FText::FromString(FString::Printf(TEXT("角色属性\n\n等级   %d        经验   %d / %d\n\n生命   %.0f / %.0f\n耐力   %.0f / %.0f\n\n装备   %s\n武器基础攻击力   %.1f\n当前有效基础伤害   %.1f  [%s]\n当前增益倍率   ×%.2f\n\n二段跳   %s\n已选升级   %s\n\n有效基础伤害不含单招倍率及目标减伤"),
        S.Level,S.Experience,S.NextLevelExperience,S.Health,S.MaxHealth,S.Stamina,S.MaxStamina,*S.WeaponName.ToString(),S.WeaponBaseDamage,S.EffectiveBaseDamage,
        S.bArmed?TEXT("持械"):TEXT("徒手／收刀"),S.BuffMultiplier,S.bDoubleJump?TEXT("已解锁"):TEXT("未解锁"),*S.Upgrades.ToString()));
}
AInventoryPreviewActor::AInventoryPreviewActor()
{
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PortraitBody")); Body->SetupAttachment(Root); Body->SetRelativeRotation(FRotator(0,-90,0));
    WeaponPivot=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMount")); WeaponPivot->SetupAttachment(Body);
    SkeletalWeapon=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PortraitSword")); SkeletalWeapon->SetupAttachment(WeaponPivot);
    StaticWeapon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortraitStaticWeapon")); StaticWeapon->SetupAttachment(WeaponPivot);
    for (UPrimitiveComponent* C:{static_cast<UPrimitiveComponent*>(Body),static_cast<UPrimitiveComponent*>(SkeletalWeapon),static_cast<UPrimitiveComponent*>(StaticWeapon)})
    { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCanEverAffectNavigation(false); C->SetVisibleInSceneCaptureOnly(true); C->SetCastShadow(false); C->SetLightingChannels(false,true,false); }
    Body->SetComponentTickEnabled(false); SkeletalWeapon->SetComponentTickEnabled(false);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortraitCapture")); Capture->SetupAttachment(Root);
    Capture->SetRelativeLocation(FVector(320,0,95)); Capture->SetRelativeRotation(FRotator(0,180,0)); Capture->FOVAngle=40;
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetTemporalAA(false);
    auto* Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("PortraitLight")); Light->SetupAttachment(Root);
    Light->SetRelativeLocation(FVector(160,-100,230)); Light->SetIntensity(15000); Light->SetAttenuationRadius(1000); Light->SetLightingChannels(false,true,false); Light->SetCastShadows(false);
}
void AInventoryPreviewActor::BindImage(UImage* Image)
{
    if (!Image) return;
    if (!Target)
    {
        Target=NewObject<UTextureRenderTarget2D>(this); Target->SRGB=false; Target->ClearColor=FLinearColor(0,0,0,1); Target->InitCustomFormat(512,512,PF_FloatRGBA,true);
        Capture->TextureTarget=Target;
        Capture->ShowOnlyComponents={Body,SkeletalWeapon,StaticWeapon};
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Third/UI/Inventory/M_CharacterPortrait.M_CharacterPortrait"));
        if (Material) { ImageMaterial=UMaterialInstanceDynamic::Create(Material,this); ImageMaterial->SetTextureParameterValue(TEXT("PortraitTexture"),Target); }
    }
    if (ImageMaterial) Image->SetBrushFromMaterial(ImageMaterial);
}
void AInventoryPreviewActor::UpdatePreview(ATPCCharacter* P,float MouseYaw)
{
    if (!P || !Target) return;
    auto* Source=P->GetMesh(); const bool Armed=P->EquipmentComponent->GetEquippedWeaponDefinition() && P->EquipmentComponent->IsWeaponDrawn();
    if (Body->GetSkeletalMeshAsset()!=Source->GetSkeletalMeshAsset() || !Body->GetSingleNodeInstance() || Armed!=bLastArmed)
    {
        Body->SetSkeletalMeshAsset(Source->GetSkeletalMeshAsset());
        auto* Idle=LoadObject<UAnimSequence>(nullptr,Armed?TEXT("/Game/Third/SwordAnimation/Idle_Anim.Idle_Anim"):TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
        Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); Body->SetAnimation(Idle); bLastArmed=Armed;
    }
    for (int32 I=0;I<Source->GetNumMaterials();++I) if (Body->GetMaterial(I)!=Source->GetMaterial(I)) Body->SetMaterial(I,Source->GetMaterial(I));
    Body->SetRelativeRotation(FRotator(0,-90+FMath::Clamp(MouseYaw,-22.f,22.f),0));
    // Explicit evaluation, without firing animation notifies or running a gameplay AnimBP.
    if (auto* Node=Body->GetSingleNodeInstance())
    {
        const float Length=Node->GetLength(); Node->SetPosition(Length>0?FMath::Fmod(static_cast<float>(FPlatformTime::Seconds()),Length):0,false);
        Body->TickAnimation(0,false); Body->RefreshBoneTransforms();
    }
    const auto* Weapon=P->EquipmentComponent->GetEquippedWeaponActor();
    SkeletalWeapon->SetVisibility(Weapon && Weapon->GetSkeletalWeaponMesh()->GetSkeletalMeshAsset());
    StaticWeapon->SetVisibility(Weapon && Weapon->GetWeaponMesh()->GetStaticMesh());
    if (Weapon)
    {
        const auto* Mount=Weapon->GetRootComponent(); WeaponPivot->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,Mount->GetAttachSocketName()); WeaponPivot->SetRelativeTransform(Mount->GetRelativeTransform());
        const auto* SW=Weapon->GetSkeletalWeaponMesh(); if (SkeletalWeapon->GetSkeletalMeshAsset()!=SW->GetSkeletalMeshAsset()) SkeletalWeapon->SetSkeletalMeshAsset(SW->GetSkeletalMeshAsset()); SkeletalWeapon->SetRelativeTransform(SW->GetRelativeTransform());
        for (int32 I=0;I<SW->GetNumMaterials();++I) if (SkeletalWeapon->GetMaterial(I)!=SW->GetMaterial(I)) SkeletalWeapon->SetMaterial(I,SW->GetMaterial(I));
        const auto* SM=Weapon->GetWeaponMesh(); StaticWeapon->SetStaticMesh(SM->GetStaticMesh()); StaticWeapon->SetRelativeTransform(SM->GetRelativeTransform());
        for (int32 I=0;I<SM->GetNumMaterials();++I) if (StaticWeapon->GetMaterial(I)!=SM->GetMaterial(I)) StaticWeapon->SetMaterial(I,SM->GetMaterial(I));
    }
    Capture->CaptureScene(); ++CaptureCount;
}
