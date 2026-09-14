#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Components/CombatHitTypes.h"
#include "TPCCharacterAudioComponent.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class UAudioComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class ETPCAudioCue : uint8
{
	FootWalk, FootRun, FootSprint, FootCrouch, Landing,
	SwordLight, SwordHeavy, FleshLight, FleshHeavy, Block, Parry, PlayerHurt
};

/** 一组同用途音效及其随机选择配置，供角色音频组件读取。 */
USTRUCT(BlueprintType)
struct FTPCAudioBank
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<USoundBase>> Sounds;
};

/** 一次声音播放请求：携带事件、位置和动作来源，用于去重与播放策略选择。 */
struct FTPCAudioRequest
{
	ETPCAudioCue Cue;
	USoundBase* Sound = nullptr;
	FVector Location = FVector::ZeroVector;
	float Volume = 0.f, Pitch = 1.f, Speed = 0.f;
	double Time = 0.;
	FName Foot;
	bool bAudioComponentCreated = false;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTPCAudioRequested, const FTPCAudioRequest&);

/** 角色声音的统一入口：协调脚步、运动和战斗反馈，管理重复请求及音效生命周期，不改变战斗结算。 */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UTPCCharacterAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTPCCharacterAudioComponent();
	void PlayFootstep(FName FootBone);
	void PlayLanding(float FallSpeed);
	void PlaySwordSwing(uint64 AttackSerial, FName WindowId, bool bHeavy, float PlaybackRate);
	FOnTPCAudioRequested OnAudioRequested;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Audio")
	TMap<ETPCAudioCue, FTPCAudioBank> SoundBanks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Audio", meta=(ClampMin="0", ClampMax="2"))
	float MasterVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Audio", meta=(ClampMin="0", ClampMax="2"))
	float FootstepVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Audio", meta=(ClampMin="0", ClampMax="2"))
	float CombatVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Audio", meta=(ClampMin="0", ClampMax="2"))
	float HurtVoiceVolume = 1.f;
	static ETPCAudioCue ChooseFootstep(float Speed, bool bCrouched);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend struct FTPCCharacterAudioTestAccess;
	void HandleResolvedHit(const FCombatHitSpec& Spec, const FCombatHitResult& Result, AActor* Source);
	UAudioComponent* Emit(ETPCAudioCue Cue, const FVector& Location, float Volume, float Pitch = 1.f,
		float Speed = 0.f, FName Foot = NAME_None);
	USoundBase* SelectSound(ETPCAudioCue Cue);
	UPROPERTY(Transient) TObjectPtr<UHealthComponent> Health;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient) TObjectPtr<USoundConcurrency> Concurrency;
	TWeakObjectPtr<UAudioComponent> ActiveHurtVoice;
	TMap<ETPCAudioCue, int32> LastVariation;
	TMap<FName, double> LastFootTimes;
	FDelegateHandle HitHandle;
	uint64 LastSwingSerial = MAX_uint64;
	TSet<FName> PlayedSwingWindows;
	double LastStepTime = -100., LastHurtTime = -100.;
};
