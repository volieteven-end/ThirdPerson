#include "DoorActor.h"

#include "Components/StaticMeshComponent.h"
#include "../Components/InventoryComponent.h"
#include "GameFramework/Pawn.h"
ADoorActor::ADoorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);
	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	SetRootComponent(DoorHinge);
	DoorHinge->SetMobility(EComponentMobility::Movable);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));

	DoorMesh->SetupAttachment(DoorHinge);
	DoorMesh->SetMobility(EComponentMobility::Movable);

	DoorMesh->SetUsingAbsoluteLocation(false);
	DoorMesh->SetUsingAbsoluteRotation(false);
	DoorMesh->SetUsingAbsoluteScale(false);

	DoorMesh->SetRelativeLocation(FVector(100.f, 0.f, 0.f));
}

void ADoorActor::BeginPlay()
{
	Super::BeginPlay();
	ClosedRotation = GetActorRotation();
	TargetRotation = ClosedRotation;
}

void ADoorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(),TargetRotation,DeltaTime,OpenSpeed);

	SetActorRotation(NewRotation);
	if (NewRotation.Equals(TargetRotation, 0.1f))
	{
		SetActorRotation(TargetRotation);
		SetActorTickEnabled(false);
	}
}

FText ADoorActor::GetInteractionText() const
{
	if (bIsOpen)
	{
		return FText::FromString(TEXT("Close door"));
	}

	if (bRequiresKey)
	{
		return FText::FromString(TEXT("Open door (Key required)"));
	}

	return FText::FromString(TEXT("Open door"));
}

void ADoorActor::Interact(APawn* InstigatorPawn)
{
	if (!bIsOpen && bRequiresKey)
	{
		UInventoryComponent* Inventory =InstigatorPawn? InstigatorPawn->FindComponentByClass<UInventoryComponent>(): nullptr;

		if (!Inventory ||!Inventory->HasItem(RequiredKeyId))
		{
			
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Door locked. Required key: %s"),
				*RequiredKeyId.ToString());

			return;
		}
		if (bConsumeKeyOnOpen &&!Inventory->RemoveItem(RequiredKeyId, 1))
		{
			return;
		}
	}
	bIsOpen = !bIsOpen;
	TargetRotation = ClosedRotation;
	if (bIsOpen)
	{
		TargetRotation.Yaw += OpenYaw;
		DoorMesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
	}
	else
	{
		DoorMesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
	}

	SetActorTickEnabled(true);
}
void ADoorActor::RestoreOpenState(bool bShouldBeOpen)
{
	bIsOpen = bShouldBeOpen;
	TargetRotation = ClosedRotation;
	if (bIsOpen)
	{
		TargetRotation.Yaw += OpenYaw;
		DoorMesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
	}
	else
	{
		DoorMesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
	}
	SetActorRotation(TargetRotation);
	SetActorTickEnabled(false);
}