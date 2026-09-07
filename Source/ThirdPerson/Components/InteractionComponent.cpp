// Fill out your copyright notice in the Description page of Project Settings.
#include "InteractionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ThirdPerson/Interfaces/Interactable.h"
#include "ThirdPerson/Items/PickupActor.h"
#include "EngineUtils.h"

// Sets default values for this component's properties
UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ...
	
}

void UInteractionComponent::TryInteract()
{
	APawn* OwnerPawn=Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}
	APlayerController* PlayerController=Cast<APlayerController>(OwnerPawn->GetController());
	if (!PlayerController)
	{
		return;
	}
	UWorld* World=GetWorld();
	if (!World)
	{
		return;
	}
	// Pickups use a character-facing semicircle, so the player does not need
	// to aim the camera precisely at a small dropped item.
	if (APickupActor* Pickup = FindBestPickupInFront(OwnerPawn))
	{
		if (IInteractable* Interactable = Cast<IInteractable>(Pickup))
		{
			Interactable->Interact(OwnerPawn);
		}
		return;
	}
	FVector Start;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(Start,ViewRotation);
	const FVector End=Start+ViewRotation.Vector()*TraceDistance;
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionTrace),false,OwnerPawn);
	QueryParams.AddIgnoredActor(OwnerPawn);
	const bool bHit=World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,QueryParams);
	DrawDebugLine(World,Start,End,bHit?FColor::Green:FColor::Red,false,2.f,0,1.5f);
	if (bHit)
	{
		AActor* HitActor=Hit.GetActor();
		// A pickup that failed the semicircle test must not fall back to camera interaction.
		if (HitActor && HitActor->IsA<APickupActor>())
		{
			return;
		}
		const float DistaneToTarget=FVector::Dist(OwnerPawn->GetActorLocation(),Hit.ImpactPoint);
		if (DistaneToTarget>MaxInteractionDistance)
		{
			UE_LOG(LogTemp, Warning, TEXT("Distance too big!"));
			return;
		}
		if (IInteractable* Interactable =Cast<IInteractable>(HitActor))
		{
			Interactable->Interact(OwnerPawn);
		}
		else
		{
			UE_LOG(LogTemp,Warning,TEXT("%s is not interactable"),*GetNameSafe(HitActor));
		}
	}
	}

void UInteractionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);

	UpdateInteractionPrompt();
}
void UInteractionComponent::UpdateInteractionPrompt()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		return;
	}
	APlayerController* PlayerController =Cast<APlayerController>(OwnerPawn->GetController());
	UWorld* World = GetWorld();
	if (!PlayerController || !World)
	{
		return;
	}
	FVector Start;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(Start,ViewRotation);
	const FVector End =Start + ViewRotation.Vector() * TraceDistance;
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionPromptTrace),false,OwnerPawn);
	AActor* NewFocusedActor = nullptr;
	FText NewPromptText = FText::GetEmpty();
	if (APickupActor* Pickup = FindBestPickupInFront(OwnerPawn))
	{
		if (IInteractable* Interactable = Cast<IInteractable>(Pickup))
		{
			NewFocusedActor = Pickup;
			NewPromptText = FText::Format(
				FText::FromString(TEXT("[E] {0}")),
				Interactable->GetInteractionText());
		}
	}
	else if (World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,QueryParams) &&
		Hit.GetActor() && !Hit.GetActor()->IsA<APickupActor>())
	{
		const float DistanceToTarget = FVector::Dist(OwnerPawn->GetActorLocation(),Hit.ImpactPoint);

		if (DistanceToTarget <= MaxInteractionDistance)
		{
			if (IInteractable* Interactable =Cast<IInteractable>(Hit.GetActor()))
			{
				NewFocusedActor = Hit.GetActor();
				NewPromptText = FText::Format(FText::FromString(TEXT("[E] {0}")),Interactable->GetInteractionText());
			}
		}
	}

	if (FocusedActor == NewFocusedActor)
	{
		return;
	}

	FocusedActor = NewFocusedActor;

	OnInteractionPromptChanged.Broadcast(NewPromptText);
}

APickupActor* UInteractionComponent::FindBestPickupInFront(
	const APawn* OwnerPawn) const
{
	UWorld* World = GetWorld();
	if (!OwnerPawn || !World || PickupRadius <= 0.f)
	{
		return nullptr;
	}

	const FVector Origin = OwnerPawn->GetActorLocation();
	const FVector Forward = OwnerPawn->GetActorForwardVector().GetSafeNormal2D();
	const float RadiusSquared = FMath::Square(PickupRadius);
	APickupActor* BestPickup = nullptr;
	float BestDistanceSquared = BIG_NUMBER;

	for (TActorIterator<APickupActor> It(World); It; ++It)
	{
		APickupActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}

		const FVector Delta = Candidate->GetActorLocation() - Origin;
		if (FMath::Abs(Delta.Z) > PickupVerticalTolerance)
		{
			continue;
		}

		const FVector Delta2D(Delta.X, Delta.Y, 0.f);
		const float DistanceSquared = Delta2D.SizeSquared();
		if (DistanceSquared > RadiusSquared)
		{
			continue;
		}

		// Dot >= 0 is exactly the 180-degree half circle in front of the actor.
		if (!Delta2D.IsNearlyZero() &&
			FVector::DotProduct(Forward, Delta2D.GetSafeNormal()) < 0.f)
		{
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestPickup = Candidate;
		}
	}

	return BestPickup;
}
