#include "TreasureChestActor.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "../Items/PickupActor.h"
#include "Engine/World.h"

ATreasureChestActor::ATreasureChestActor()
{
	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("ChestMesh"));

	SetRootComponent(ChestMesh);
	ChestMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ATreasureChestActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!OriginalMesh && !bIsOpened) OriginalMesh=ChestMesh->GetStaticMesh();
	RefreshChestAppearance();
}
void ATreasureChestActor::RefreshChestAppearance()
{
	UStaticMesh* Desired=bIsOpened && OpenedMesh ? OpenedMesh.Get() : ClosedMesh ? ClosedMesh.Get() : OriginalMesh.Get();
	if (Desired) ChestMesh->SetStaticMesh(Desired);
}
FText ATreasureChestActor::GetInteractionText() const
{
	return FText::FromString(bIsOpened? TEXT("Empty chest"): TEXT("Open chest"));
}

void ATreasureChestActor::Interact(APawn* InstigatorPawn)
{
	if (bIsOpened || !RewardPickupClass)
	{
		return;
	}

	bIsOpened = true;
	RefreshChestAppearance();
	UWorld* World = GetWorld();
	if (World)
	{
		for (int32 Index = 0; Index < RewardPickupCount; ++Index)
		{
			const FVector RandomOffset(FMath::FRandRange(-DropRadius, DropRadius),FMath::FRandRange(-DropRadius, DropRadius),30.f);
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			World->SpawnActor<APickupActor>(RewardPickupClass,GetActorLocation() + RandomOffset,GetActorRotation(),SpawnParams);
		}
	}
	OnChestOpened();
}
