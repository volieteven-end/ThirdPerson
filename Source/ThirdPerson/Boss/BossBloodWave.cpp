#include "BossBloodWave.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BossDefinition.h"
#include "BossActionComponent.h"
#include "../Weapons/CombatVFXSettings.h"
ABossBloodWave::ABossBloodWave()
{
 bReturnImmediatelyOnImpact=true; FlightLifetime=1200.f/900.f; MaxPooledInstances=16;
 CollisionSphere->InitSphereRadius(28.f);
 ProjectileMovement->ProjectileGravityScale=0.f;
 FlightEffect=CreateDefaultSubobject<UParticleSystemComponent>(TEXT("BloodWaveFlight"));
 FlightEffect->SetupAttachment(GetRootComponent()); FlightEffect->bAutoActivate=false;
 FlightEffect->SetCollisionEnabled(ECollisionEnabled::NoCollision); FlightEffect->SetCastShadow(false);
}
void ABossBloodWave::InitializeCombatProjectile(const FCombatHitSpec& Spec,float Speed)
{
 FCombatHitSpec Wave=Spec; Wave.bCanBeBlocked=true; Wave.bCanBeParried=false;
 FlightLifetime=1200.f/FMath::Max(1.f,Speed);
 const UBossDefinition* D=GetDefault<UBossDefinition>();
 if (const auto* A=GetOwner()?GetOwner()->FindComponentByClass<UBossActionComponent>():nullptr) D=A->GetDefinition();
 ImpactEffect=D->WaveImpactEffect; ImpactScale=D->WaveImpactScale;
 FlightEffect->DeactivateSystem(); FlightEffect->KillParticlesForced();
 FlightEffect->SetTemplate(D->WaveFlightEffect); FlightEffect->SetRelativeScale3D(FVector(D->WaveEffectScale));
 Super::InitializeCombatProjectile(Wave,Speed);
 if (TPCCombatVFX::IsEnabled() && D->WaveFlightEffect) FlightEffect->ActivateSystem(true);
}
void ABossBloodWave::DeactivateProjectile()
{
 FlightEffect->DeactivateImmediate(); ImpactEffect=nullptr;
 Super::DeactivateProjectile();
}
void ABossBloodWave::PlayImpactEffect(const FHitResult& Hit)
{
 // World-owned one-shot: returning the wave must not truncate its impact burst.
 if (TPCCombatVFX::IsEnabled()) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),ImpactEffect,Hit.ImpactPoint,Hit.ImpactNormal.Rotation(),FVector(ImpactScale),true);
}
