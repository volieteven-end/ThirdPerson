#include "ArenaActors.h"
#include "ArenaTravelSubsystem.h"
#include "../Character/TPCCharacter.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AArenaBounds::AArenaBounds()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatBoundary"));
    SetRootComponent(Bounds); Bounds->SetBoxExtent(FVector(2000, 2000, 1200));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision); Bounds->SetHiddenInGame(true);
}
bool AArenaBounds::ContainsLocation(const FVector& Position, float Margin) const
{
    const FVector P = Bounds->GetComponentTransform().InverseTransformPosition(Position).GetAbs();
    const FVector E = Bounds->GetUnscaledBoxExtent() + FVector(Margin);
    return P.X <= E.X && P.Y <= E.Y && P.Z <= E.Z;
}
bool AArenaBounds::ContainsActor(const AActor* Actor) const
{
    if (!IsValid(Actor)) return false;
    const auto* Character = Cast<ACharacter>(Actor);
    return ContainsLocation(Character ? Character->GetCharacterMovement()->GetActorFeetLocation() + FVector(0,0,20) : Actor->GetActorLocation(), 25.f);
}
FVector AArenaBounds::ClampToInterior(const FVector& Position, float Margin) const
{
    FVector P=Bounds->GetComponentTransform().InverseTransformPosition(Position);
    const FVector E=Bounds->GetUnscaledBoxExtent();
    P.X=FMath::Clamp(P.X,-FMath::Max(0.,E.X-Margin),FMath::Max(0.,E.X-Margin));
    P.Y=FMath::Clamp(P.Y,-FMath::Max(0.,E.Y-Margin),FMath::Max(0.,E.Y-Margin));
    return Bounds->GetComponentTransform().TransformPosition(P);
}
AArenaPortal::AArenaPortal()
{
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Interaction"));
    SetRootComponent(InteractionBox); InteractionBox->SetBoxExtent(FVector(65,130,140));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBox->SetCanEverAffectNavigation(false); InteractionBox->SetHiddenInGame(true);
    Label = FText::FromString(TEXT("进入战斗场"));
}
void AArenaPortal::Interact(APawn* Pawn)
{
    auto* Player = Cast<ATPCCharacter>(Pawn);
    if (!Player || !Player->IsPlayerControlled() || Player->GetCharacterMovement()->IsFalling()) return;
    if (auto* Travel = UArenaTravelSubsystem::Get(this))
        Travel->Travel(Player, GetDestinationMap(), ArrivalTag);
}
FString AArenaPortal::GetDestinationMap() const
{ return UWorld::RemovePIEPrefix(Destination.ToSoftObjectPath().GetLongPackageName()); }
AArenaWaveDirector::AArenaWaveDirector()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .2f;
    static ConstructorHelpers::FClassFinder<AEnemyCharacter> Melee(TEXT("/Game/Third/Character/BP_EnemyCharacter"));
    static ConstructorHelpers::FClassFinder<AEnemyCharacter> Ranged(TEXT("/Game/Third/Character/BP_EnemyRangedCharacter"));
    MeleeClass = Melee.Class; RangedClass = Ranged.Class;
}
void AArenaWaveDirector::BeginPlay()
{
    Super::BeginPlay(); Random.Initialize(RandomSeed ? RandomSeed : FMath::Rand());
    if (!Arena || !MeleeClass || !RangedClass)
    { UE_LOG(LogTemp, Error, TEXT("Arena wave configuration incomplete")); SetActorTickEnabled(false); }
}
int32 AArenaWaveDirector::GetRemaining() const
{
    int32 Count = PendingRanged.Num();
    for (const auto& E : Enemies) if (IsValid(E))
        if (auto* H = E->FindComponentByClass<UHealthComponent>(); H && H->GetCurrentHealth() > 0) ++Count;
    return Count;
}
void AArenaWaveDirector::OnEnemyDeath() { bDeathPending = true; }
void AArenaWaveDirector::StartWave()
{
    ++Wave; NextWaveAt = -1.; bCompletedWaveSaved = false;
    const int32 Count = Random.RandRange(3,4), Ranged = Random.RandRange(1,2);
    for (int32 I=0; I<Count; ++I) PendingRanged.Add(I < Ranged);
}
bool AArenaWaveDirector::SpawnOne(bool bRanged)
{
    auto* Player = UGameplayStatics::GetPlayerPawn(this,0);
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    const auto Class = bRanged ? RangedClass : MeleeClass;
    if (!Player || !Nav || !Class || !Arena) return false;
    const auto* CDO = Class->GetDefaultObject<AEnemyCharacter>();
    const float Radius = CDO->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight = CDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Extent = Arena->Bounds->GetUnscaledBoxExtent();
    for (int32 Attempt=0; Attempt<24; ++Attempt)
    {
        const FVector Sample = Arena->GetActorTransform().TransformPosition(FVector(Random.FRandRange(-Extent.X+150,Extent.X-150),Random.FRandRange(-Extent.Y+150,Extent.Y-150),0));
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Sample,Projected,FVector(150,150,1500)) || !Arena->ContainsLocation(Projected.Location,-100)) continue;
        const FVector Point = Projected.Location + FVector(0,0,HalfHeight+5);
        if (FVector::DistSquared2D(Point,Player->GetActorLocation()) < FMath::Square(PlayerSpawnClearance)) continue;
        bool bCrowded = false;
        for (const auto& E : Enemies) if (IsValid(E) && FVector::DistSquared2D(Point,E->GetActorLocation()) < FMath::Square(250.f)) { bCrowded=true; break; }
        if (bCrowded || GetWorld()->OverlapBlockingTestByChannel(Point,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight))) continue;
        auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Point,Player->GetActorLocation());
        if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
        FActorSpawnParameters Params; Params.Owner=this; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
        auto* E = GetWorld()->SpawnActor<AEnemyCharacter>(Class,Point,(Player->GetActorLocation()-Point).Rotation(),Params);
        if (!E) continue;
        Enemies.Add(E); E->FindComponentByClass<UHealthComponent>()->OnDeath.AddDynamic(this,&ThisClass::OnEnemyDeath);
        return true;
    }
    return false;
}
void AArenaWaveDirector::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player || Player->HealthComponent->GetCurrentHealth() <= 0 || !Arena) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bStarted)
    {
        if (Arena->ContainsActor(Player)) { bStarted=true; NextWaveAt=Now+WaveDelay; }
        return;
    }
    // Death delegates finish their XP/drop work before this next director tick saves anything.
    if (bDeathPending) { bDeathPending=false; Enemies.RemoveAll([](const auto& E) { return !IsValid(E) || E->FindComponentByClass<UHealthComponent>()->GetCurrentHealth() <= 0; }); }
    if (!PendingRanged.IsEmpty() && Arena->ContainsActor(Player))
        if (SpawnOne(PendingRanged.Last())) PendingRanged.Pop();
    if (GetRemaining() == 0 && !bCompletedWaveSaved && Now >= NextSaveAttempt)
    {
        NextSaveAttempt=Now+5.;
        if (auto* Travel = UArenaTravelSubsystem::Get(this); Travel && Travel->SaveFormal(Player))
        { bCompletedWaveSaved=true; NextWaveAt=Now+WaveDelay; }
    }
    if (bCompletedWaveSaved && NextWaveAt >= 0 && Now >= NextWaveAt && Arena->ContainsActor(Player)) StartWave();
}
FText AArenaWaveDirector::GetStatusText() const
{
    if (!bStarted) return FText::FromString(TEXT("随机战斗场 · 走入石台开始挑战\n入口处按 E 返回教程"));
    if (GetRemaining()>0) return FText::FromString(FString::Printf(TEXT("第 %d 波  ·  剩余 %d 名敌人"),Wave,GetRemaining()));
    if (!bCompletedWaveSaved) return FText::FromString(TEXT("正在保存本波进度；保存失败时不会启动下一波"));
    return FText::FromString(FString::Printf(TEXT("下一波：%d 秒"),FMath::Max(0,FMath::CeilToInt(NextWaveAt-GetWorld()->GetTimeSeconds()))));
}
void AArenaWaveDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    PendingRanged.Reset();
    for (const auto& E : Enemies) if (IsValid(E)) E->FindComponentByClass<UHealthComponent>()->OnDeath.RemoveAll(this);
    Enemies.Reset();
    // Travel/retry unloads the world: all enemies, corpses, controllers and pooled projectiles end with it.
    Super::EndPlay(Reason);
}
