// Fill out your copyright notice in the Description page of Project Settings.


#include "PickupActor.h"

#include "Components/StaticMeshComponent.h"
#include "../Interfaces/Interactable.h"
#include "GameFramework/Pawn.h"
#include "../Components//InventoryComponent.h"
#include "../Items/ItemDefinition.h"
#include "GameFramework/RotatingMovementComponent.h"
// Sets default values
APickupActor::APickupActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PickupMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	RotatingMovement =CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
	RotatingMovement->RotationRate =FRotator(0.f, 90.f, 0.f);
	SetRootComponent(PickupMesh);
	// Pickups are query objects: they can be aimed at, but never hold a pawn in
	// the air or behave like a solid wall.
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void APickupActor::BeginPlay()
{
	Super::BeginPlay();
	SnapToGround();
}

void APickupActor::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World || GroundTraceDistance <= 0.f)
	{
		return;
	}

	const FVector ActorLocation = GetActorLocation();
	FVector BoundsOrigin;
	FVector BoundsExtent;
	GetActorBounds(true, BoundsOrigin, BoundsExtent);
	const float PivotToBottom = ActorLocation.Z - (BoundsOrigin.Z - BoundsExtent.Z);
	const FVector Start = ActorLocation + FVector(0.f, 0.f, 100.f);
	const FVector End = ActorLocation - FVector(0.f, 0.f, GroundTraceDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PickupGroundTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	if (World->LineTraceSingleByObjectType(
		Hit, Start, End, ObjectParams, QueryParams))
	{
		FVector GroundedLocation = ActorLocation;
		GroundedLocation.Z = Hit.ImpactPoint.Z + PivotToBottom + GroundClearance;
		SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FText APickupActor::GetInteractionText() const
{
	if (!ItemDefinition)
	{
		return FText::FromString("Invalid Item Definition");
	}
	return ItemDefinition->DisplayName;
}

void APickupActor::Interact(APawn* InstigatorPawn)
{
	if (!ItemDefinition)
	{
		UE_LOG(LogTemp,Warning,TEXT("Pickup has No Item Definition"));
		return;
	}
	if (!InstigatorPawn)
	{
		return;
	}
	UInventoryComponent* Inventory=InstigatorPawn->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Inventory Component"));
		return;
	}
	if (Inventory->AddItem(ItemDefinition, ItemCount))
	{
		UE_LOG(LogTemp,Warning,TEXT("Pick up:%s x%d"),*ItemDefinition->DisplayName.ToString(),ItemCount);
		Destroy();
	}else
	{
		UE_LOG(
		LogTemp,
		Warning,
		TEXT("Inventory full; cannot pick up: %s"),
		*ItemDefinition->DisplayName.ToString());
	}
	
	
}
