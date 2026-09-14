#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossTelegraph.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
/** Boss 地面预警的纯表现 Actor，控制范围提示和显示生命周期，不参与命中结算。 */
UCLASS()
class THIRDPERSON_API ABossTelegraph : public AActor
{
 GENERATED_BODY()
public:
 ABossTelegraph();
 void Configure(bool bCircle,float RadiusOrLength,float Width,const FLinearColor& Color,UMaterialInterface* Parent);
protected:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Plane;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
};
