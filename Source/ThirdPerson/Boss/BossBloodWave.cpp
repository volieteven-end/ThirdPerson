#include "BossBloodWave.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BossDefinition.h"
ABossBloodWave::ABossBloodWave()
{
 bReturnImmediatelyOnImpact=true; FlightLifetime=1200.f/900.f; MaxPooledInstances=16;
 CollisionSphere->InitSphereRadius(28.f);
 ProjectileMovement->ProjectileGravityScale=0.f;
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BloodWaveVisual")); Visual->SetupAttachment(GetRootComponent());
 ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere"));
 Visual->SetStaticMesh(Mesh.Object); Visual->SetRelativeScale3D(FVector(.12f,.8f,.55f));
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetCastShadow(false);
}
void ABossBloodWave::InitializeCombatProjectile(const FCombatHitSpec& Spec,float Speed)
{
 FCombatHitSpec Wave=Spec; Wave.bCanBeBlocked=true; Wave.bCanBeParried=false;
 FlightLifetime=1200.f/FMath::Max(1.f,Speed);
 if (auto* Parent=GetDefault<UBossDefinition>()->WarningMaterial.Get())
 {
  auto* M=Visual->CreateDynamicMaterialInstance(0,Parent);
  M->SetScalarParameterValue(TEXT("Circle"),0.f); M->SetVectorParameterValue(TEXT("WarningColor"),FLinearColor(1,.01f,.035f));
 }
 Super::InitializeCombatProjectile(Wave,Speed);
}
