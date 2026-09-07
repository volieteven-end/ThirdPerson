#include "StarterWeaponNPC.h"

#include "../Components/EquipmentComponent.h"
#include "../UI/StarterWeaponWidget.h"
#include "../Weapons/WeaponDefinition.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"

AStarterWeaponNPC::AStarterWeaponNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	SetRootComponent(InteractionBox);
	InteractionBox->SetBoxExtent(FVector(45.f, 45.f, 90.f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	NPCMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NPCMesh"));
	NPCMesh->SetupAttachment(InteractionBox);
	NPCMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FText AStarterWeaponNPC::GetInteractionText() const
{
	return bWeaponClaimed && bOnlyGrantOnce
		? FText::FromString(TEXT("No weapons left"))
		: FText::FromString(TEXT("Talk"));
}

void AStarterWeaponNPC::Interact(APawn* InstigatorPawn)
{
	if (!InstigatorPawn || (bWeaponClaimed && bOnlyGrantOnce) || SelectionWidget)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(InstigatorPawn->GetController());
	if (!PlayerController || !SelectionWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Starter NPC requires a SelectionWidgetClass"));
		return;
	}

	if (!InstigatorPawn->FindComponentByClass<UEquipmentComponent>())
	{
		UE_LOG(LogTemp, Warning, TEXT("Interacting pawn has no EquipmentComponent"));
		return;
	}

	PendingPawn = InstigatorPawn;
	SelectionWidget = CreateWidget<UStarterWeaponWidget>(PlayerController, SelectionWidgetClass);
	if (!SelectionWidget)
	{
		PendingPawn = nullptr;
		return;
	}

	SelectionWidget->InitializeSelection(this);
	SelectionWidget->AddToViewport(50);

	PlayerController->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SelectionWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}

bool AStarterWeaponNPC::CompleteWeaponChoice(EStarterWeaponChoice Choice)
{
	if (!PendingPawn || (bWeaponClaimed && bOnlyGrantOnce))
	{
		return false;
	}

	UWeaponDefinition* ChosenWeapon = Choice == EStarterWeaponChoice::Melee
		? MeleeWeaponDefinition.Get()
		: RangedWeaponDefinition.Get();
	UEquipmentComponent* Equipment =
		PendingPawn->FindComponentByClass<UEquipmentComponent>();

	if (!ChosenWeapon || !Equipment || !Equipment->EquipWeapon(ChosenWeapon))
	{
		return false;
	}

	bWeaponClaimed = true;
	OnWeaponGranted(ChosenWeapon);
	PendingPawn = nullptr;
	SelectionWidget = nullptr;
	return true;
}

void AStarterWeaponNPC::CancelWeaponSelection()
{
	PendingPawn = nullptr;
	SelectionWidget = nullptr;
}

FText AStarterWeaponNPC::GetMeleeWeaponName() const
{
	return MeleeWeaponDefinition
		? MeleeWeaponDefinition->DisplayName
		: FText::FromString(TEXT("Melee weapon unavailable"));
}

FText AStarterWeaponNPC::GetRangedWeaponName() const
{
	return RangedWeaponDefinition
		? RangedWeaponDefinition->DisplayName
		: FText::FromString(TEXT("Ranged weapon unavailable"));
}
