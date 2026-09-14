// 存档与药水回归：检查药水使用、进度恢复和重生交接，测试数据与正式槽位分离。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/LevelComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Items/ItemDefinition.h"
#include "GameFramework/WorldSettings.h"

struct FTPCRespawnTestAccess
{
	static FVector4 CombatStats(const UCombatComponent& C)
	{
		return FVector4(C.GetEffectiveDamage(), C.GetEffectiveAttackCooldown(),
			C.GetEffectiveAttackRange(), C.GetEffectiveAttackRadius());
	}
	static TArray<ELevelUpgradeType> Choices(const ULevelComponent& Level)
	{
		TArray<ELevelUpgradeType> Result;
		for (const auto& Choice : Level.CurrentChoices) Result.Add(Choice.Type);
		return Result;
	}
};

namespace RespawnTest
{
struct FWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	~FWorld()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		if (World->IsRooted()) World->RemoveFromRoot();
	}
	ATPCCharacter* Player()
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ATPCCharacter>(ATPCCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCPotionFloorTest, "ThirdPerson.Respawn.PotionFloorAndFullBag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCPotionFloorTest::RunTest(const FString&)
{
	RespawnTest::FWorld World;
	auto* Source = World.Player()->InventoryComponent.Get();
	auto* Target = World.Player()->InventoryComponent.Get();
	auto* Potion = Source->HealthPotionDefinition.Get();
	if (!TestNotNull(TEXT("Canonical pickup potion resolves"), Potion)) return false;
	auto* Key = LoadObject<UItemDefinition>(nullptr, TEXT("/Game/Third/DataAsset/DA_Key.DA_Key"));
	if (!TestNotNull(TEXT("Real key definition resolves"), Key)) return false;
	for (const int32 Count : {0,1,2,4,14})
	{
		Source->Slots.Reset(); Source->MaxSlots = 20;
		TestTrue(TEXT("Seed keys"), Source->AddItem(Key, 3));
		if (Count) TestTrue(TEXT("Seed potion stacks"), Source->AddItem(Potion, Count));
		Target->RestoreRespawnInventory(*Source);
		TestEqual(TEXT("Death uses max(previous count, 2)"), Target->GetHealthPotionCount(), FMath::Max(Count,2));
		TestEqual(TEXT("Transfer never edits old inventory"), Source->GetHealthPotionCount(), Count);
		TestEqual(TEXT("Other item slot count is retained"), Target->Slots[0].Count, Source->Slots[0].Count);
		TestEqual(TEXT("Other item definition is retained"), Target->Slots[0].ItemDefinition.Get(), Key);
		TestTrue(TEXT("Refill is idempotent"), Target->RefillHealthPotionsAfterDeath());
		TestEqual(TEXT("Second refill never adds extra"), Target->GetHealthPotionCount(), FMath::Max(Count,2));
	}
	Source->Slots.Reset(); Source->MaxSlots = 20;
	TestTrue(TEXT("Fill all 20 slots"), Source->AddItem(Key, 20));
	Target->RestoreRespawnInventory(*Source);
	TestEqual(TEXT("Full bag still receives 2 potions"), Target->GetHealthPotionCount(), 2);
	TestEqual(TEXT("Only one extra slot is needed"), Target->MaxSlots, 21);
	TestEqual(TEXT("No key was evicted"), Target->Slots.Num(), 21);
	TestTrue(TEXT("All 20 keys remain removable"), Target->RemoveItem(Key->ItemId, 20));
	// Definitions with MaxStack=1 need two slots, and existing stacks count together.
	auto* SinglePotion = DuplicateObject<UItemDefinition>(Potion, GetTransientPackage());
	SinglePotion->MaxStack = 1;
	Source->Slots.Reset(); Source->MaxSlots = 1; Source->HealthPotionDefinition = SinglePotion;
	Source->AddItem(Key, 1); Target->RestoreRespawnInventory(*Source);
	TestEqual(TEXT("Two non-stackable bottles both fit"), Target->Slots.Num(), 3);
	TestEqual(TEXT("Separate stacks contribute to one counter"), Target->GetHealthPotionCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCPotionUseTest, "ThirdPerson.Respawn.PotionConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCPotionUseTest::RunTest(const FString&)
{
	RespawnTest::FWorld World;
	auto* P = World.Player(); auto* Bag = P->InventoryComponent.Get();
	TestTrue(TEXT("Initial death refill"), Bag->RefillHealthPotionsAfterDeath());
	P->HealthComponent->SetCurrentHealth(100.f);
	TestFalse(TEXT("Full health cannot waste a potion"), Bag->UseFirstConsumable());
	P->HealthComponent->SetCurrentHealth(40.f);
	TestTrue(TEXT("Healing consumes exactly one"), Bag->UseFirstConsumable());
	TestEqual(TEXT("Real potion heals 25"), P->HealthComponent->GetCurrentHealth(), 65.f);
	TestEqual(TEXT("Using a bottle does not auto-refill"), Bag->GetHealthPotionCount(), 1);
	P->HealthComponent->SetCurrentHealth(0.f);
	TestFalse(TEXT("Dead characters cannot consume bottles"), Bag->UseFirstConsumable());
	TestEqual(TEXT("Failed use retains the last bottle"), Bag->GetHealthPotionCount(), 1);
	P->HealthComponent->SetCurrentHealth(40.f);
	TestTrue(TEXT("Last bottle is usable"), Bag->UseFirstConsumable());
	TestEqual(TEXT("Inventory may reach zero until next death"), Bag->GetHealthPotionCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCRespawnStatsTest, "ThirdPerson.Respawn.PermanentAttributesAndPendingChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCRespawnStatsTest::RunTest(const FString&)
{
	RespawnTest::FWorld World; auto* Source = World.Player(); auto* Target = World.Player();
	Source->LevelComponent->RestoreProgress(9, 0, {ELevelUpgradeType::Vitality, ELevelUpgradeType::Power,
		ELevelUpgradeType::Frenzy, ELevelUpgradeType::Endurance, ELevelUpgradeType::LongReach, ELevelUpgradeType::IronSkin});
	Source->LevelComponent->AddExperience(Source->LevelComponent->GetExperienceToNextLevel() + 17);
	Source->HealthComponent->AddMaxHealth(11.f, false);
	Source->StaminaComponent->AddMaxStamina(13.f, false);
	Source->CombatComponent->AddDamageBonus(3.f);
	Source->HealthComponent->SetCurrentHealth(0.f);
	Source->StaminaComponent->SetCurrentStamina(0.f);
	const auto Choices = FTPCRespawnTestAccess::Choices(*Source->LevelComponent);
	for (int32 Repeat = 0; Repeat < 3; ++Repeat)
	{
		Target->HealthComponent->RestoreRespawnAttributes(*Source->HealthComponent);
		Target->StaminaComponent->RestoreRespawnAttributes(*Source->StaminaComponent);
		Target->CombatComponent->RestoreRespawnAttributes(*Source->CombatComponent);
		Target->LevelComponent->RestoreRespawnProgress(*Source->LevelComponent);
		TestEqual(TEXT("Max health does not reset or grow twice"), Target->HealthComponent->GetMaxHealth(), Source->HealthComponent->GetMaxHealth());
		TestEqual(TEXT("Respawn has full upgraded health"), Target->HealthComponent->GetCurrentHealth(), Source->HealthComponent->GetMaxHealth());
		TestEqual(TEXT("Respawn has full upgraded stamina"), Target->StaminaComponent->GetCurrentStamina(), Source->StaminaComponent->GetMaxStamina());
		TestTrue(TEXT("Damage, cooldown, reach and radius retain all modifiers"),
			FTPCRespawnTestAccess::CombatStats(*Source->CombatComponent).Equals(FTPCRespawnTestAccess::CombatStats(*Target->CombatComponent)));
		TestEqual(TEXT("Level is retained"), Target->LevelComponent->GetLevel(), 10);
		TestEqual(TEXT("Unspent XP is retained"), Target->LevelComponent->GetCurrentExperience(), 17);
		TestTrue(TEXT("Selected upgrade history retained"), Target->LevelComponent->GetSelectedUpgrades() == Source->LevelComponent->GetSelectedUpgrades());
		TestTrue(TEXT("Unspent upgrade selection retained"), Target->LevelComponent->HasPendingUpgradeChoice());
		TestTrue(TEXT("Death does not reroll offered upgrades"), FTPCRespawnTestAccess::Choices(*Target->LevelComponent) == Choices);
	}
	const float FullHealth = Target->HealthComponent->GetCurrentHealth();
	Target->HealthComponent->ApplyDamage(10.f);
	TestTrue(TEXT("Iron Skin damage reduction retained"), FMath::IsNearlyEqual(FullHealth - Target->HealthComponent->GetCurrentHealth(), 9.f));
	TestTrue(TEXT("Pending upgrade can still be selected"), Target->LevelComponent->SelectUpgrade(0));
	TestFalse(TEXT("One pending selection is consumed once"), Target->LevelComponent->HasPendingUpgradeChoice());
	return true;
}

#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../GameMode/TPCGameMode.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../Save/TPCRespawnSubsystem.h"
#include "../UI/HealthPotionWidget.h"
#include "../Weapons/WeaponDefinition.h"

namespace RespawnTest
{
struct FSaveScope
{
	FString OriginalCommandLine = FCommandLine::Get();
	FString Slot = TEXT("RespawnAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FSaveScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=") + Slot + TEXT(" ") + OriginalCommandLine)); }
	~FSaveScope()
	{
		// This randomly named file is created only by this test, never a player save.
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		FCommandLine::Set(*OriginalCommandLine);
	}
};

bool CaptureHUD(UWorld* W, const FString& Name)
{
	auto Viewport = W->GetGameViewport()->GetGameViewportWidget();
	if (!Viewport.IsValid()) return false;
	TArray<FColor> Pixels; FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size)) return false;
	TArray64<uint8> PNG;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("RespawnPotion");
	IFileManager::Get().MakeDirectory(*Directory, true);
	return FFileHelper::SaveArrayToFile(PNG, *(Directory / Name));
}

class FScenario : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TSharedPtr<FSaveScope> SaveScope;
	TWeakObjectPtr<ATPCCharacter> OldPlayer;
	int32 Case = 0, Stage = 0;
	const int32 Counts[5] = {0,1,2,4,14};
	double Started = FPlatformTime::Seconds(), StageTime = 0;
	float MaxHealth = 0, MaxStamina = 0;
	FVector4 CombatStats;
	TWeakObjectPtr<UWeaponDefinition> Weapon;
	TArray<FInventorySlot> PreviousSlots;
	TArray<ELevelUpgradeType> PendingChoices;
	FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d bottles before death: %s"), Counts[Case], Text); }
public:
	FScenario(FAutomationTestBase* InTest, TSharedPtr<FSaveScope> InSaveScope) : Test(InTest), SaveScope(InSaveScope) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 150) { Test->AddError(TEXT("Respawn PIE timed out")); return true; }
		UWorld* W = GEditor ? GEditor->PlayWorld : nullptr;
		if (!W || !W->HasBegunPlay()) return false;
		auto* PC = Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
		auto* P = PC ? Cast<ATPCCharacter>(PC->GetPawn()) : nullptr;
		if (!P || !PC->GetHealthPotionWidget()) return false;
		if (Stage == 0)
		{
			for (TActorIterator<AEnemyCharacter> It(W); It; ++It) It->Destroy();
			P->LevelComponent->RestoreProgress(9, 17, {ELevelUpgradeType::Vitality});
			P->HealthComponent->AddMaxHealth(11.f, true);
			P->HealthComponent->MultiplyDamageReceived(.8f);
			P->StaminaComponent->AddMaxStamina(13.f, true);
			P->CombatComponent->MultiplyDamage(1.17f);
			P->CombatComponent->MultiplyAttackCooldown(.8f);
			P->CombatComponent->MultiplyMeleeReach(1.21f);
			P->SetDoubleJumpUnlocked(true);
			auto* GM = Cast<ATPCGameMode>(W->GetAuthGameMode());
			if (!Test->TestNotNull(TEXT("Actual map game mode"), GM)) return true;
			GM->RestoreEnemiesDefeated(3);
			Test->TestEqual(TEXT("Automation save is isolated"), TPCSaveSlots::Resolve(), SaveScope->Slot);
			Stage = 1;
		}
		if (Stage == 1)
		{
			PC->RestoreGameplayInput();
			auto* Bag = P->InventoryComponent.Get(); Bag->Slots.Reset(); Bag->MaxSlots = 20;
			auto* Key = LoadObject<UItemDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_Key.DA_Key"));
			Test->TestTrue(Label(TEXT("seed non-potion loot")), Bag->AddItem(Key, Case == 0 ? 20 : 3));
			if (Counts[Case]) Test->TestTrue(Label(TEXT("seed bottles")), Bag->AddItem(Bag->HealthPotionDefinition, Counts[Case]));
			if (Case == 1)
			{
				auto* Bow = LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestBow.DA_TestBow"));
				Test->TestTrue(TEXT("Equip a non-default weapon"), P->EquipmentComponent->EquipWeapon(Bow));
			}
			if (Case == 4)
			{
				P->LevelComponent->AddExperience(P->LevelComponent->GetExperienceToNextLevel());
				PendingChoices = FTPCRespawnTestAccess::Choices(*P->LevelComponent);
				PC->RestoreGameplayInput();
			}
			MaxHealth = P->HealthComponent->GetMaxHealth(); MaxStamina = P->StaminaComponent->GetMaxStamina();
			CombatStats = FTPCRespawnTestAccess::CombatStats(*P->CombatComponent);
			Weapon = P->EquipmentComponent->GetEquippedWeaponDefinition(); PreviousSlots = Bag->Slots;
			// Deliberately stale checkpoint: any accidental disk restore rolls level/loot/kills back.
			auto* StaleSave = NewObject<UTPCSaveGame>(); StaleSave->PlayerLevel = 1; StaleSave->PlayerExperience = 0;
			StaleSave->PlayerTransform = FTransform(FVector(800,800,500));
			Test->TestTrue(TEXT("Write disposable stale checkpoint"), UGameplayStatics::SaveGameToSlot(StaleSave, SaveScope->Slot, 0));
			Test->TestEqual(Label(TEXT("HUD reads actual bag before death")), PC->GetHealthPotionWidget()->GetDisplayedCount(), Counts[Case]);
			OldPlayer = P;
			P->HealthComponent->ApplyDamage(1000000.f);
			Test->TestEqual(Label(TEXT("damage really killed the player")), P->HealthComponent->GetCurrentHealth(), 0.f);
			Stage = 2; return false;
		}
		if (Stage == 2)
		{
			if (!PC->IsDeathScreenOpen()) return false;
			if (Case == 0)
			{
				auto* GM = W->GetAuthGameMode(); auto PreviousPawnClass = GM->DefaultPawnClass;
				GM->DefaultPawnClass = nullptr;
				PC->RestartAfterDeath();
				GM->DefaultPawnClass = PreviousPawnClass;
				Test->TestTrue(TEXT("Failed spawn preserves the old pawn"), PC->GetPawn() == OldPlayer.Get());
				Test->TestEqual(TEXT("Failed spawn preserves all 20 occupied slots"), P->InventoryComponent->Slots.Num(), 20);
				Test->TestTrue(TEXT("Failed spawn offers a retry"), PC->IsDeathScreenOpen());
			}
			PC->RestartAfterDeath(); P = Cast<ATPCCharacter>(PC->GetPawn());
			if (!Test->TestTrue(Label(TEXT("real restart creates a new pawn")), P && P != OldPlayer.Get())) return true;
			Test->TestFalse(Label(TEXT("old pawn destroyed only after handoff")), OldPlayer.IsValid());
			Test->TestFalse(Label(TEXT("handoff cannot leak to later spawns")), W->GetSubsystem<UTPCRespawnSubsystem>()->PendingSource.IsValid());
			Test->TestEqual(Label(TEXT("full upgraded health")), P->HealthComponent->GetCurrentHealth(), MaxHealth);
			Test->TestEqual(Label(TEXT("full upgraded stamina")), P->StaminaComponent->GetCurrentStamina(), MaxStamina);
			Test->TestTrue(Label(TEXT("all combat attributes preserved")), CombatStats.Equals(FTPCRespawnTestAccess::CombatStats(*P->CombatComponent)));
			Test->TestEqual(Label(TEXT("selected weapon preserved")), P->EquipmentComponent->GetEquippedWeaponDefinition(), Weapon.Get());
			Test->TestTrue(Label(TEXT("unlocked movement ability preserved")), P->bDoubleJumpUnlocked && P->JumpMaxCount == 2);
			Test->TestEqual(Label(TEXT("level not rolled back")), P->LevelComponent->GetLevel(), Case == 4 ? 10 : 9);
			Test->TestEqual(Label(TEXT("XP not rolled back")), P->LevelComponent->GetCurrentExperience(), 17);
			Test->TestEqual(Label(TEXT("upgrade history retained")), P->LevelComponent->GetSelectedUpgrades().Num(), 1);
			Test->TestEqual(Label(TEXT("kill progress not rolled back")), Cast<ATPCGameMode>(W->GetAuthGameMode())->GetEnemiesDefeated(), 3);
			for (int32 I = 0; I < PreviousSlots.Num(); ++I)
			{
				if (!Test->TestTrue(Label(TEXT("old slot retained")), P->InventoryComponent->Slots.IsValidIndex(I))) continue;
				const auto& Before = PreviousSlots[I]; const auto& After = P->InventoryComponent->Slots[I];
				Test->TestEqual(Label(TEXT("item identity retained")), After.ItemDefinition.Get(), Before.ItemDefinition.Get());
				if (Before.ItemDefinition != P->InventoryComponent->HealthPotionDefinition)
					Test->TestEqual(Label(TEXT("non-potion counts unchanged")), After.Count, Before.Count);
			}
			Test->TestEqual(Label(TEXT("potion floor in actual bag")), P->InventoryComponent->GetHealthPotionCount(), FMath::Max(2,Counts[Case]));
			Test->TestEqual(Label(TEXT("HUD rebound to new inventory")), PC->GetHealthPotionWidget()->GetInventory(), P->InventoryComponent.Get());
			Test->TestEqual(Label(TEXT("HUD refreshed immediately on respawn")), PC->GetHealthPotionWidget()->GetDisplayedCount(), FMath::Max(2,Counts[Case]));
			Test->TestFalse(Label(TEXT("death screen dismissed")), PC->IsDeathScreenOpen());
			if (Case == 0)
			{
				// A death-expanded bag must also survive the existing checkpoint reload path.
				auto* ExpandedSave = NewObject<UTPCSaveGame>();
				ExpandedSave->InventoryCapacity = P->InventoryComponent->MaxSlots;
				ExpandedSave->PlayerTransform = P->GetActorTransform();
				ExpandedSave->CheckpointMap = UGameplayStatics::GetCurrentLevelName(P, true);
				ExpandedSave->EnemiesDefeated = 3;
				for (const auto& Slot : P->InventoryComponent->Slots)
				{
					auto& Saved = ExpandedSave->InventorySlots.AddDefaulted_GetRef();
					Saved.ItemDefinition = Slot.ItemDefinition; Saved.Count = Slot.Count;
				}
				Test->TestTrue(TEXT("Save expanded bag to disposable checkpoint"),
					UGameplayStatics::SaveGameToSlot(ExpandedSave, SaveScope->Slot, 0));
				FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto* Reloaded = W->SpawnActor<ATPCCharacter>(P->GetClass(), P->GetActorTransform(), Params);
				if (Test->TestNotNull(TEXT("Fresh pawn loads the expanded checkpoint"), Reloaded))
				{
					Test->TestEqual(TEXT("Reload keeps all 21 occupied slots"), Reloaded->InventoryComponent->Slots.Num(), 21);
					Test->TestEqual(TEXT("Reload keeps expanded capacity"), Reloaded->InventoryComponent->MaxSlots, 21);
					Test->TestEqual(TEXT("Reload keeps the two recovery bottles"), Reloaded->InventoryComponent->GetHealthPotionCount(), 2);
					Reloaded->Destroy();
				}
			}
			if (Case == 4)
			{
				Test->TestTrue(TEXT("Pending upgrade survives actual death"), P->LevelComponent->HasPendingUpgradeChoice());
				Test->TestTrue(TEXT("Actual death preserves the three offered choices"), FTPCRespawnTestAccess::Choices(*P->LevelComponent) == PendingChoices);
				Test->TestTrue(TEXT("Upgrade selection UI reopens after possession"), UGameplayStatics::IsGamePaused(W));
			}
			else Test->TestFalse(Label(TEXT("gameplay unpaused")), UGameplayStatics::IsGamePaused(W));
			StageTime = FPlatformTime::Seconds(); Stage = 3; return false;
		}
		if (Stage == 3 && FPlatformTime::Seconds() - StageTime > .8)
		{
			if (Case == 3 && FParse::Param(FCommandLine::Get(), TEXT("RespawnVisualAudit")))
				Test->TestTrue(TEXT("Capture real gameplay HUD including Slate"), CaptureHUD(W, TEXT("HealthPotionHUD_4.png")));
			P->HealthComponent->SetCurrentHealth(MaxHealth - 50.f);
			Test->TestTrue(Label(TEXT("new pawn can use its inherited potion")), P->InventoryComponent->UseFirstConsumable());
			Test->TestEqual(Label(TEXT("HUD decrements without replenishing")), PC->GetHealthPotionWidget()->GetDisplayedCount(), FMath::Max(2,Counts[Case])-1);
			Test->AddInfo(Label(TEXT("death, transfer, refill, UI rebind and consumption verified")));
			if (++Case == 5) { PC->SetPause(false); return true; }
			Stage = 1;
		}
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCRespawnPIETest, "ThirdPerson.Respawn.PIE.RetainsProgressWithStaleCheckpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCRespawnPIETest::RunTest(const FString&)
{
	auto SaveScope = MakeShared<RespawnTest::FSaveScope>();
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
	// This regression intentionally exercises same-world campaign respawn, not arena reload.
	GEditor->GetEditorWorldContext().World()->GetWorldSettings()->DefaultGameMode =
		LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(RespawnTest::FScenario(this, SaveScope));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
#endif
