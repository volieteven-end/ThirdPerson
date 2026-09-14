#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "InventoryPresentation.generated.h"
class ATPCCharacter;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UImage;

/** 属性页的只读快照：分别显示装备基础攻击力与当前有效基础伤害，不包含单招倍率和目标减伤。 */
USTRUCT(BlueprintType)
struct FPlayerAttributeSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Level=1;
    UPROPERTY(BlueprintReadOnly) int32 Experience=0;
    UPROPERTY(BlueprintReadOnly) int32 NextLevelExperience=0;
    UPROPERTY(BlueprintReadOnly) float Health=0;
    UPROPERTY(BlueprintReadOnly) float MaxHealth=0;
    UPROPERTY(BlueprintReadOnly) float Stamina=0;
    UPROPERTY(BlueprintReadOnly) float MaxStamina=0;
    UPROPERTY(BlueprintReadOnly) FText WeaponName;
    UPROPERTY(BlueprintReadOnly) float WeaponBaseDamage=0;
    UPROPERTY(BlueprintReadOnly) float EffectiveBaseDamage=0;
    UPROPERTY(BlueprintReadOnly) float BuffMultiplier=1;
    UPROPERTY(BlueprintReadOnly) bool bArmed=false;
    UPROPERTY(BlueprintReadOnly) bool bDoubleJump=false;
    UPROPERTY(BlueprintReadOnly) FText Upgrades;
};
/** 把角色各组件汇总为属性快照，复用战斗系统的伤害计算，避免 UMG 再维护一套公式。 */
UCLASS()
class THIRDPERSON_API UInventoryPresentation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Inventory|Attributes") static FPlayerAttributeSnapshot ReadAttributes(ATPCCharacter* Player);
    static FText FormatAttributes(const FPlayerAttributeSnapshot& Value);
};

/** 背包中的纯展示对象，仅使用模型和场景捕获组件；不是可战斗 Pawn，不触发伤害或动画特效通知。 */
UCLASS()
class THIRDPERSON_API AInventoryPreviewActor : public AActor
{
    GENERATED_BODY()
public:
    AInventoryPreviewActor();
    void UpdatePreview(ATPCCharacter* Player,float MouseYaw);
    void BindImage(UImage* Image);
    int32 GetCaptureCount() const { return CaptureCount; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WeaponPivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SkeletalWeapon;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StaticWeapon;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneCaptureComponent2D> Capture;
private:
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ImageMaterial;
    bool bLastArmed=false;
    int32 CaptureCount=0;
};
