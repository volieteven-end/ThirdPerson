#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LevelComponent.generated.h"

UENUM(BlueprintType)
enum class ELevelUpgradeType : uint8
{
	Vitality,
	Power,
	Frenzy,
	Endurance,
	LongReach,
	IronSkin
};

/** 一次升级选项的描述与数值配置，选择后由等级组件应用对应效果。 */
USTRUCT(BlueprintType)
struct FLevelUpgradeChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ELevelUpgradeType Type = ELevelUpgradeType::Vitality;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText Description;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnLevelProgressChanged,
	int32, Level,
	int32, CurrentExperience,
	int32, ExperienceToNextLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLevelUp, int32, NewLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnUpgradeChoicesReady,
	const FLevelUpgradeChoice&, ChoiceA,
	const FLevelUpgradeChoice&, ChoiceB,
	const FLevelUpgradeChoice&, ChoiceC);

/** 管理等级、经验和永久升级，应用选择后通知其他组件及 UI；存档保存进度而非界面状态。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API ULevelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULevelComponent();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void AddExperience(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Level")
	bool SelectUpgrade(int32 ChoiceIndex);

	void RestoreProgress(
		int32 SavedLevel,
		int32 SavedExperience,
		const TArray<ELevelUpgradeType>& SavedUpgrades);

	int32 GetLevel() const { return Level; }
	/** Attributes are copied separately: do not apply level/upgrade bonuses twice. */
	void RestoreRespawnProgress(const ULevelComponent& Source);
	void NotifyPendingUpgradeChoices();
	int32 GetPendingUpgradeSelections() const { return PendingUpgradeSelections; }
	const TArray<FLevelUpgradeChoice>& GetPendingUpgradeChoices() const { return CurrentChoices; }
	void RestorePendingUpgradeChoices(int32 Count, const TArray<uint8>& Choices);
	int32 GetCurrentExperience() const { return CurrentExperience; }
	int32 GetExperienceToNextLevel() const;
	bool HasPendingUpgradeChoice() const { return PendingUpgradeSelections > 0; }
	const TArray<ELevelUpgradeType>& GetSelectedUpgrades() const { return SelectedUpgrades; }

	UPROPERTY(BlueprintAssignable, Category = "Level")
	FOnLevelProgressChanged OnLevelProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Level")
	FOnPlayerLevelUp OnLevelUp;

	UPROPERTY(BlueprintAssignable, Category = "Level")
	FOnUpgradeChoicesReady OnUpgradeChoicesReady;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (ClampMin = "1"))
	int32 BaseExperienceRequirement = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (ClampMin = "1.0"))
	float ExperienceGrowth = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Level|Automatic Growth", meta = (ClampMin = "0.0"))
	float MaxHealthPerLevel = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Level|Automatic Growth", meta = (ClampMin = "0.0"))
	float DamagePerLevel = 2.f;

private:
	friend struct FTPCRespawnTestAccess;
	void LevelUpOnce();
	void GenerateUpgradeChoices();
	FLevelUpgradeChoice MakeUpgradeChoice(ELevelUpgradeType Type) const;
	void ApplyUpgrade(ELevelUpgradeType Type);
	void BroadcastProgress();

	int32 Level = 1;
	int32 CurrentExperience = 0;
	int32 PendingUpgradeSelections = 0;
	bool bProgressRestored = false;
	TArray<FLevelUpgradeChoice> CurrentChoices;
	TArray<ELevelUpgradeType> SelectedUpgrades;
};
