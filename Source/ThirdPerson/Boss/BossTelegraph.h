#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossTelegraph.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
/** Cosmetic ground warning. Collision/damage is exclusively owned by BossActionComponent. */
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
