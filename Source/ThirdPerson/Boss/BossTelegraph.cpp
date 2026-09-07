#include "BossTelegraph.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
ABossTelegraph::ABossTelegraph()
{
 PrimaryActorTick.bCanEverTick=false;
 Plane=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningPlane")); SetRootComponent(Plane);
 ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Plane"));
 Plane->SetStaticMesh(Mesh.Object); Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Plane->SetGenerateOverlapEvents(false); Plane->SetCastShadow(false); Plane->SetCanEverAffectNavigation(false);
}
void ABossTelegraph::Configure(bool bCircle,float Size,float Width,const FLinearColor& Color,UMaterialInterface* Parent)
{
 if (Parent)
 {
  Material=UMaterialInstanceDynamic::Create(Parent,this); Plane->SetMaterial(0,Material);
  Material->SetVectorParameterValue(TEXT("WarningColor"),Color);
  Material->SetScalarParameterValue(TEXT("Circle"),bCircle?1.f:0.f);
 }
 Plane->SetWorldScale3D(bCircle?FVector(Size*.02f,Size*.02f,1):FVector(Size*.01f,Width*.01f,1));
}
