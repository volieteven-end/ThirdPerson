#include "TPCCharacterAudioComponent.h"
#include "../Character/TPCCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/HealthComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UTPCCharacterAudioComponent::UTPCCharacterAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Reuse individual one-shots, not long pre-recorded walking/running loops.
	auto Add = [this](ETPCAudioCue Cue, const TCHAR* Directory, const TCHAR* Stem, int32 First, int32 Last, bool bPad)
	{
		for (int32 I = First; I <= Last; ++I)
		{
			const FString Name = FString(Stem) + (bPad ? FString::Printf(TEXT("%02d"), I) : FString::FromInt(I));
			const FString Path = FString(Directory) + TEXT("/") + Name + TEXT(".") + Name;
			ConstructorHelpers::FObjectFinder<USoundBase> Sound(*Path);
			if (Sound.Succeeded()) SoundBanks.FindOrAdd(Cue).Sounds.Add(Sound.Object);
		}
	};
	const TCHAR* Feet = TEXT("/Game/ProSoundCollection/Footsteps/Wavs");
	Add(ETPCAudioCue::FootWalk, Feet, TEXT("footstep_concrete_walk_"), 1, 4, true);
	Add(ETPCAudioCue::FootCrouch, Feet, TEXT("footstep_concrete_walk_"), 1, 4, true);
	Add(ETPCAudioCue::FootRun, Feet, TEXT("footstep_concrete_run_"), 1, 4, true);
	Add(ETPCAudioCue::FootSprint, Feet, TEXT("footstep_concrete_run_"), 10, 13, true);
	Add(ETPCAudioCue::Landing, Feet, TEXT("footstep_concrete_run_"), 10, 13, true);
	// These imported assets use an extra suffix; keep their existing package paths.
	auto AddSwings = [this](ETPCAudioCue Cue, const TCHAR* Weight)
	{
		for (int32 I = 1; I <= 3; ++I)
		{
			const FString Name = FString::Printf(TEXT("Woosh_Sword_%s_%02d_Wav"), Weight, I);
			const FString Path = TEXT("/Game/Weapons_woosh/Wavs/") + Name + TEXT(".") + Name;
			ConstructorHelpers::FObjectFinder<USoundBase> Sound(*Path);
			if (Sound.Succeeded()) SoundBanks.FindOrAdd(Cue).Sounds.Add(Sound.Object);
		}
	};
	AddSwings(ETPCAudioCue::SwordLight, TEXT("Normal"));
	AddSwings(ETPCAudioCue::SwordHeavy, TEXT("Heavy"));
	const TCHAR* Sword = TEXT("/Game/Sword_Fighting_SFX/Wavs");
	Add(ETPCAudioCue::FleshLight, Sword, TEXT("Sword_Hit_Flesh_"), 1, 3, false);
	Add(ETPCAudioCue::FleshHeavy, Sword, TEXT("Sword_Slash_Flesh_And_Bones_"), 1, 3, false);
	Add(ETPCAudioCue::Block, Sword, TEXT("Sword_Hit_Shield_"), 1, 3, false);
	Add(ETPCAudioCue::Parry, Sword, TEXT("Sword_Hit_"), 1, 3, false);
	Add(ETPCAudioCue::PlayerHurt, TEXT("/Game/ProSoundCollection/Voice/HumanMaleB/Wavs"),
		TEXT("voice_male_b_hurt_pain_set_1_"), 1, 3, true);
}

void UTPCCharacterAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (Health) HitHandle = Health->OnCombatHitResolved.AddUObject(this, &ThisClass::HandleResolvedHit);
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(600.f,0,0);
	Attenuation->Attenuation.FalloffDistance = 1800.f;
	Concurrency = NewObject<USoundConcurrency>(this);
	Concurrency->Concurrency.MaxCount = 8;
	Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopQuietest;
	Concurrency->Concurrency.VoiceStealReleaseTime = .04f;
}

void UTPCCharacterAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Health) Health->OnCombatHitResolved.Remove(HitHandle);
	if (ActiveHurtVoice.IsValid()) ActiveHurtVoice->FadeOut(.05f, 0.f);
	Super::EndPlay(Reason);
}

ETPCAudioCue UTPCCharacterAudioComponent::ChooseFootstep(float Speed, bool bCrouched)
{
	if (bCrouched) return ETPCAudioCue::FootCrouch;
	if (Speed > 520.f) return ETPCAudioCue::FootSprint;
	return Speed > 220.f ? ETPCAudioCue::FootRun : ETPCAudioCue::FootWalk;
}

void UTPCCharacterAudioComponent::PlayFootstep(FName FootBone)
{
	auto* Player = Cast<ATPCCharacter>(GetOwner());
	if (!Player || !GetWorld() || !Health || Health->GetCurrentHealth() <= 0.f) return;
	const auto* Movement = Player->GetCharacterMovement();
	const float Speed = Player->GetVelocity().Size2D();
	if (!Movement || !Movement->IsMovingOnGround() || Speed < 15.f) return;
	const ETPCActionState State = Player->ActionComponent->GetActionState();
	if (State != ETPCActionState::Free && State != ETPCActionState::Guard) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const double* LastFoot = LastFootTimes.Find(FootBone);
	// State/blend-space transitions may deliver two notifies for one planted foot.
	if (Now - LastStepTime < .09 || (LastFoot && Now - *LastFoot < .20)) return;
	LastStepTime = Now; LastFootTimes.Add(FootBone, Now);
	const auto Cue = ChooseFootstep(Speed, Player->bIsCrouched);
	float Volume = .32f, Pitch = 1.f;
	if (Cue == ETPCAudioCue::FootCrouch) { Volume = .17f; Pitch = .90f; }
	else if (Cue == ETPCAudioCue::FootRun) { Volume = .46f; Pitch = 1.02f; }
	else if (Cue == ETPCAudioCue::FootSprint) { Volume = .62f; Pitch = 1.09f; }
	Pitch *= FMath::GetMappedRangeValueClamped(FVector2D(50,650), FVector2D(.96f,1.04f), Speed);
	const FVector Location = Player->GetMesh()->GetSocketLocation(FootBone);
	Emit(Cue, Location, Volume * FootstepVolume, Pitch, Speed, FootBone);
}

void UTPCCharacterAudioComponent::PlayLanding(float FallSpeed)
{
	if (!Cast<ATPCCharacter>(GetOwner()) || !Health || Health->GetCurrentHealth() <= 0.f || FallSpeed < 160.f) return;
	const float Volume = FMath::GetMappedRangeValueClamped(FVector2D(160,1000), FVector2D(.28f,.72f), FallSpeed);
	Emit(ETPCAudioCue::Landing, GetOwner()->GetActorLocation(), Volume * FootstepVolume, .94f);
	LastStepTime = GetWorld()->GetTimeSeconds();
}

void UTPCCharacterAudioComponent::PlaySwordSwing(uint64 AttackSerial, FName WindowId, bool bHeavy, float PlaybackRate)
{
	if (!Health || Health->GetCurrentHealth() <= 0.f) return;
	if (LastSwingSerial != AttackSerial) { LastSwingSerial = AttackSerial; PlayedSwingWindows.Reset(); }
	if (PlayedSwingWindows.Contains(WindowId)) return;
	PlayedSwingWindows.Add(WindowId);
	const float Pitch = FMath::Clamp(PlaybackRate, .8f, 1.25f) * (bHeavy ? .93f : 1.06f);
	Emit(bHeavy ? ETPCAudioCue::SwordHeavy : ETPCAudioCue::SwordLight,
		GetOwner()->GetActorLocation() + FVector(0,0,35), (bHeavy ? .72f : .53f) * CombatVolume, Pitch);
}

void UTPCCharacterAudioComponent::HandleResolvedHit(const FCombatHitSpec& Spec, const FCombatHitResult& Result, AActor* Source)
{
	if (!GetWorld() || Result.Outcome == ECombatHitOutcome::Ignored) return;
	const FVector Point = Spec.ImpactPoint.IsNearlyZero() ? GetOwner()->GetActorLocation() : Spec.ImpactPoint;
	if (Result.bParried) { Emit(ETPCAudioCue::Parry, Point, .78f * CombatVolume, 1.06f); return; }
	if (Result.bBlocked) { Emit(ETPCAudioCue::Block, Point, .63f * CombatVolume, Result.bGuardBroken ? .84f : 1.f); return; }
	if (Result.ActualDamage <= 0.f) return;
	const bool bHeavy = Spec.PoiseDamage >= 30.f || Spec.Damage >= 60.f;
	Emit(bHeavy ? ETPCAudioCue::FleshHeavy : ETPCAudioCue::FleshLight, Point,
		(bHeavy ? .83f : .64f) * CombatVolume, bHeavy ? .93f : 1.f);
	if (Cast<ATPCCharacter>(GetOwner()) && GetWorld()->GetTimeSeconds() - LastHurtTime >= .35)
	{
		LastHurtTime = GetWorld()->GetTimeSeconds();
		if (ActiveHurtVoice.IsValid()) ActiveHurtVoice->FadeOut(.04f,0.f);
		ActiveHurtVoice = Emit(ETPCAudioCue::PlayerHurt, GetOwner()->GetActorLocation(), .67f * HurtVoiceVolume,
			Result.bKilled ? .88f : 1.f);
	}
}

USoundBase* UTPCCharacterAudioComponent::SelectSound(ETPCAudioCue Cue)
{
	const auto* Bank = SoundBanks.Find(Cue);
	if (!Bank || Bank->Sounds.IsEmpty()) return nullptr;
	int32 Index = FMath::RandRange(0, Bank->Sounds.Num()-1);
	if (Bank->Sounds.Num() > 1)
		if (const int32* Previous = LastVariation.Find(Cue); Previous && Index == *Previous)
			Index = (Index + FMath::RandRange(1, Bank->Sounds.Num()-1)) % Bank->Sounds.Num();
	LastVariation.Add(Cue, Index);
	return Bank->Sounds[Index];
}

UAudioComponent* UTPCCharacterAudioComponent::Emit(ETPCAudioCue Cue, const FVector& Location,
	float Volume, float Pitch, float Speed, FName Foot)
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer || MasterVolume <= 0.f || Volume <= 0.f) return nullptr;
	USoundBase* Sound = SelectSound(Cue);
	if (!Sound) return nullptr;
	Volume *= MasterVolume * FMath::FRandRange(.94f,1.f);
	Pitch *= FMath::FRandRange(.97f,1.03f);
	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location,
		FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation, Concurrency, true);
	FTPCAudioRequest Request{Cue, Sound, Location, Volume, Pitch, Speed, GetWorld()->GetTimeSeconds(), Foot, IsValid(Audio)};
	OnAudioRequested.Broadcast(Request);
	return Audio;
}
