#include "TutorialDirector.h"
#include "TutorialActors.h"
#include "TutorialGameMode.h"
#include "TutorialWidgets.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Items/ItemDefinition.h"
#include "../Weapons/WeaponDefinition.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "../Arena/ArenaTravelSubsystem.h"

ATutorialDirector::ATutorialDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}
ATutorialDirector* ATutorialDirector::Find(const UObject* Context)
{
    if (UWorld* W = Context ? Context->GetWorld() : nullptr)
        for (TActorIterator<ATutorialDirector> It(W); It; ++It) return *It;
    return nullptr;
}
void ATutorialDirector::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!Course || !Course->IsValidCourse(Error) || !PlayerWeapon || !EnemyWeapon)
    { UE_LOG(LogTemp,Error,TEXT("Tutorial configuration is incomplete: %s"),*Error); SetActorTickEnabled(false); return; }
    Progress.Initialize(Course);
    if (auto* Travel = UArenaTravelSubsystem::Get(this)) Travel->RestoreTutorial(*this);
    for (TActorIterator<ATutorialZone> It(GetWorld()); It; ++It) Zones.Add(*It);
    bInitialized = true;
    BindPlayer(Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0)));
    RefreshWorld();
}
void ATutorialDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    UnbindPlayer();
    if (HUD) HUD->RemoveFromParent();
    Super::EndPlay(Reason);
}
void ATutorialDirector::UnbindPlayer()
{
    if (auto* P = Player.Get())
    {
        P->ActionComponent->OnActionPlaybackStarted.RemoveAll(this);
        P->HealthComponent->OnCombatHitResolved.RemoveAll(this);
        P->InventoryComponent->OnConsumableUsed.RemoveAll(this);
    }
    Player.Reset();
}
void ATutorialDirector::BindPlayer(ATPCCharacter* Pawn)
{
    if (!Pawn || Player.Get() == Pawn) return;
    UnbindPlayer(); Player = Pawn;
    Pawn->ActionComponent->OnActionPlaybackStarted.AddUObject(this,&ThisClass::OnPlayback);
    Pawn->HealthComponent->OnCombatHitResolved.AddUObject(this,&ThisClass::OnPlayerHit);
    Pawn->InventoryComponent->OnConsumableUsed.AddUObject(this,&ThisClass::OnPotionUsed);
    InitializePlayer(); ResetActionTracking();
    // A placed director may bind while the pawn's Blueprint BeginPlay is still equipping
    // its default sword. Finalize the training kit after that initialization, without retrying progress.
    TWeakObjectPtr<ATutorialDirector> WeakThis(this);
    TWeakObjectPtr<ATPCCharacter> WeakPawn(Pawn);
    GetWorldTimerManager().SetTimerForNextTick([WeakThis,WeakPawn]()
    {
        if (!WeakThis.IsValid() || !WeakPawn.IsValid() || WeakThis->Player != WeakPawn) return;
        WeakThis->InitializePlayer();
        if (WeakThis->bEntered && WeakThis->Progress.Lesson == 4) WeakPawn->HealthComponent->SetCurrentHealth(50.f);
    });
    if (!HUD) if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
    {
        HUD = CreateWidget<UTutorialHUDWidget>(PC,UTutorialHUDWidget::StaticClass());
        if (HUD) { HUD->Director = this; HUD->AddToViewport(12); }
    }
}
void ATutorialDirector::InitializePlayer()
{
    auto* P = Player.Get(); if (!P) return;
    P->HealthComponent->MaxHealth = 100.f; P->HealthComponent->SetCurrentHealth(100.f);
    P->StaminaComponent->SetCurrentStamina(P->StaminaComponent->GetMaxStamina());
    P->EquipmentComponent->EquipWeapon(PlayerWeapon); P->EquipmentComponent->SetWeaponDrawn(true);
    P->SetDoubleJumpUnlocked(false);
    auto* Bag = P->InventoryComponent.Get();
    Bag->Slots.Reset(); Bag->OnInventoryChanged.Broadcast();
    Bag->AddItem(Bag->HealthPotionDefinition,2);
}
void ATutorialDirector::ResetActionTracking()
{
    GroundNext = AirNext = ChainPhase = 0; LastTargetAction = LastDodgeAction = 0;
    bSawJump = bWasFalling = bForwardDodge = bSideDodge = false;
    LastDodgeTime = LastLandingTime = -100.; PendingLandingZone.Reset();
}
ATutorialZone* ATutorialDirector::EntryFor(int32 Index) const
{
    for (const auto& Z : Zones) if (Z.IsValid() && Z->bEntry && Z->LessonIndex == Index) return Z.Get();
    // GameMode asks for a spawn before BeginPlay has cached the zones.
    if (GetWorld()) for (TActorIterator<ATutorialZone> It(GetWorld()); It; ++It)
        if (It->bEntry && It->LessonIndex == Index) return *It;
    return nullptr;
}
int32 ATutorialDirector::GetCheckpointIndex() const { return Progress.Lesson == INDEX_NONE ? LastLesson : Progress.Lesson; }
FTransform ATutorialDirector::GetCheckpoint() const
{
    const auto* E = EntryFor(GetCheckpointIndex()); return E ? E->Checkpoint : FTransform(FVector(-4700,-3000,100));
}
void ATutorialDirector::PlayerRestarted(ATPCCharacter* Pawn)
{
    if (!bInitialized) return;
    bRespawnInitializing = true;
    Progress.Retry(); ClearTarget(); bEntered = false; ResetActionTracking(); BindPlayer(Pawn);
    // The existing respawn function finishes health/position restoration after RestartPlayer returns.
    TWeakObjectPtr<ATutorialDirector> WeakThis(this);
    GetWorldTimerManager().SetTimerForNextTick([WeakThis]()
    {
        if (!WeakThis.IsValid()) return;
        WeakThis->bRespawnInitializing = false;
        WeakThis->bEntered = false;
        WeakThis->InitializePlayer(); WeakThis->bRefreshPending = true;
    });
}
void ATutorialDirector::EnterCurrentLesson()
{
    if (bEntered || bRespawnInitializing || !Progress.CurrentObjective() || !Player.IsValid()) return;
    bEntered = true; LastLesson = Progress.Lesson; ResetActionTracking();
    InitializePlayer();
    if (Progress.Lesson == 4) Player->HealthComponent->SetCurrentHealth(50.f);
    SpawnTarget(); RefreshWorld();
}
void ATutorialDirector::ClearTarget()
{
    if (auto* E = Target.Get())
    {
        if (auto* H = E->FindComponentByClass<UHealthComponent>()) H->OnCombatHitResolved.RemoveAll(this);
        if (auto* C = E->GetController()) C->Destroy();
        E->Destroy();
    }
    Target.Reset(); SpawnAfter = -1.;
}
void ATutorialDirector::SpawnTarget()
{
    if (!bEntered || Target.IsValid() || !Player.IsValid()) return;
    const int32 L = Progress.Lesson;
    if (L != 1 && L != 3 && L != 5 && L != 6) return;
    const auto* Entry = EntryFor(L); if (!Entry) return;
    auto* E = GetWorld()->SpawnActorDeferred<ATutorialTrainingEnemy>(ATutorialTrainingEnemy::StaticClass(),Entry->TargetSpawn,
        this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!E) { UE_LOG(LogTemp,Error,TEXT("Tutorial target spawn failed")); return; }
    E->TrainingMode = L == 3 ? ETutorialEnemyMode::Guard : L == 5 ? ETutorialEnemyMode::Combat : ETutorialEnemyMode::Passive;
    E->TrainingId = L == 1 ? TEXT("SwordTarget") : L == 3 ? TEXT("GuardTarget") : L == 5 ? TEXT("CombatTarget") : TEXT("AirTarget");
    E->TrainingWeapon = EnemyWeapon; E->Director = this; Target = E;
    E->FinishSpawning(Entry->TargetSpawn);
    E->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.AddUObject(this,&ThisClass::OnTargetHit,TWeakObjectPtr<ATutorialTrainingEnemy>(E));
    LastTargetHitTime = GetWorld()->GetTimeSeconds();
}
void ATutorialDirector::HandleZone(ATutorialZone* Zone, ATPCCharacter* Pawn)
{
    if (!bInitialized || !Zone || Pawn != Player.Get() || Pawn->HealthComponent->GetCurrentHealth() <= 0) return;
    if (Zone->bEntry)
    {
        if (!Progress.IsUnlocked(Zone->LessonIndex))
        { ShowToast(FText::FromString(TEXT("先完成当前练习；也可以在 Esc 菜单跳过。"))); return; }
        if (Progress.BasicsFinished() && Progress.Lesson != Zone->LessonIndex)
        {
            ClearTarget(); Progress.StartLesson(Zone->LessonIndex,true); bEntered = false; bSummaryPending = false;
        }
        if (Progress.Lesson == Zone->LessonIndex) EnterCurrentLesson();
        return;
    }
    if (!bEntered || Zone->LessonIndex != Progress.Lesson) return;
    auto* M = Pawn->GetCharacterMovement();
    const double Now = GetWorld()->GetTimeSeconds();
    switch (Zone->Signal)
    {
    case ETutorialSignal::WalkMarker:
        if (M->IsMovingOnGround()) SendSignal(Zone->Signal,++EventSerial,Zone->TargetId);
        break;
    case ETutorialSignal::SprintMarker:
        if (M->IsMovingOnGround() && Pawn->GetVelocity().Size2D() > 520.f && Pawn->ActionComponent->GetActionState() == ETPCActionState::Free)
            SendSignal(Zone->Signal,++EventSerial,Zone->TargetId);
        break;
    case ETutorialSignal::JumpLanding:
        if (M->IsFalling() && bSawJump) PendingLandingZone = Zone;
        else if (bSawJump && Now - LastLandingTime < .3) { SendSignal(Zone->Signal,++EventSerial,Zone->TargetId); bSawJump = false; }
        break;
    case ETutorialSignal::ForwardDodge:
    case ETutorialSignal::SideDodge:
        if (Now - LastDodgeTime < 1.1 && FVector::Dist2D(DodgeStart,Pawn->GetActorLocation()) > 70.f &&
            ((Zone->Signal == ETutorialSignal::ForwardDodge && bForwardDodge) || (Zone->Signal == ETutorialSignal::SideDodge && bSideDodge)))
            SendSignal(Zone->Signal,LastDodgeAction,Zone->TargetId);
        break;
    default: break;
    }
}
void ATutorialDirector::SendSignal(ETutorialSignal Signal, uint64 Serial, FName TargetId)
{
    if (!bEntered) return;
    const int32 Previous = Progress.Lesson;
    const auto Result = Progress.Observe(Signal,Serial,TargetId);
    if (Result == ETutorialProgressResult::Ignored) return;
    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_PROGRESS lesson=%d objective=%d signal=%d result=%d"),Previous,Progress.Objective,int32(Signal),int32(Result));
    if (Result == ETutorialProgressResult::ObjectiveCompleted)
    { GroundNext = AirNext = ChainPhase = 0; ShowToast(FText::FromString(TEXT("完成！继续下一项。"))); }
    if (Result == ETutorialProgressResult::LessonCompleted)
    {
        LastLesson = Previous; bEntered = false; bRefreshPending = true;
        ShowToast(FText::FromString(Progress.Lesson == INDEX_NONE ? TEXT("训练完成！所有区域已开放。") : TEXT("本节完成，沿金色石路前往下一站。")));
        if (Progress.BasicsFinished() && !Progress.bPractice)
        { bSummaryPending = true; SummaryAfter = GetWorld()->GetTimeSeconds() + 1.5; }
    }
    RefreshWorld();
}
void ATutorialDirector::OnPlayback(uint64 Instance, ETPCActionState State, const UActionDefinition* Def)
{
    if (!bEntered || !Player.IsValid() || !Def) return;
    const auto* Set = Player->ActionComponent->GetActionSet(); if (!Set) return;
    if (State == ETPCActionState::Dodge)
    {
        LastDodgeTime = GetWorld()->GetTimeSeconds(); LastDodgeAction = Instance; DodgeStart = Player->GetActorLocation();
        bForwardDodge = Set->Dodges.IsValidIndex(0) && Set->Dodges[0] == Def;
        bSideDodge = (Set->Dodges.IsValidIndex(2) && Set->Dodges[2] == Def) || (Set->Dodges.IsValidIndex(3) && Set->Dodges[3] == Def);
    }
    const int32 Ground = Set->GroundCombo.IndexOfByPredicate([Def](const auto& D){return D == Def;});
    if (Ground == 0 || Ground != GroundNext) GroundNext = 0;
    const int32 Air = Set->AirCombo.IndexOfByPredicate([Def](const auto& D){return D == Def;});
    if (Def == Set->Rising) { ChainPhase = AirNext = 0; }
    else if (Air == 0 && Player->GetCharacterMovement()->IsFalling())
    { AirNext = 1; ChainPhase = ChainPhase == 1 ? 2 : 0; }
    else if (Air == 1 && AirNext == 1 && Player->GetCharacterMovement()->IsFalling())
    {
        AirNext = 2; ChainPhase = ChainPhase == 2 ? 3 : 0;
        SendSignal(ETutorialSignal::AirPair,Instance);
    }
    else if (Def != Set->Dive) { AirNext = ChainPhase = 0; }
}
void ATutorialDirector::OnTargetHit(const FCombatHitSpec&, const FCombatHitResult& Hit, AActor* Source, TWeakObjectPtr<ATutorialTrainingEnemy> E)
{
    if (!bEntered || Source != Player.Get() || !E.IsValid() || E != Target || Hit.ActualDamage <= 0.f || Hit.bBlocked || Hit.bParried) return;
    const auto* A = Player->ActionComponent.Get(); const auto* Set = A->GetActionSet(); const auto* Def = A->GetActiveDefinition();
    const uint64 Serial = A->GetActionInstanceId();
    if (!Set || !Def || Serial == LastTargetAction) return;
    LastTargetAction = Serial; LastTargetHitTime = GetWorld()->GetTimeSeconds();
    const auto* Goal = Progress.CurrentObjective(); if (!Goal) return;
    const int32 Ground = Set->GroundCombo.IndexOfByPredicate([Def](const auto& D){return D == Def;});
    if (Goal->Signal == ETutorialSignal::SwordHit && Ground != INDEX_NONE)
    { SendSignal(ETutorialSignal::SwordHit,Serial,E->TrainingId); return; }
    if (Goal->Signal == ETutorialSignal::GroundCombo)
    {
        if (Ground == 0) GroundNext = 1;
        else if (Ground == GroundNext) ++GroundNext;
        else GroundNext = 0;
        if (GroundNext == 4) { SendSignal(ETutorialSignal::GroundCombo,Serial,E->TrainingId); GroundNext = 0; }
        return;
    }
    if (Def == Set->Rising)
    {
        ChainPhase = 1;
        SendSignal(ETutorialSignal::RisingHit,Serial,E->TrainingId);
    }
    else if (Def == Set->Dive)
    {
        if (Goal->Signal == ETutorialSignal::DiveHit) SendSignal(ETutorialSignal::DiveHit,Serial,E->TrainingId);
        else if (ChainPhase == 3)
            SendSignal(ETutorialSignal::AirChain,Serial,E->TrainingId);
        ChainPhase = 0;
    }
}
void ATutorialDirector::OnPlayerHit(const FCombatHitSpec& Spec, const FCombatHitResult& Hit, AActor* Source)
{
    auto* E = Cast<ATutorialTrainingEnemy>(Source);
    if (!E || E != Target.Get() || E->TrainingMode != ETutorialEnemyMode::Guard) return;
    if (Hit.bParried) SendSignal(ETutorialSignal::Parried,uint64(Spec.ActionSerial),E->TrainingId);
    else if (Hit.bBlocked && !Hit.bGuardBroken) SendSignal(ETutorialSignal::Blocked,uint64(Spec.ActionSerial),E->TrainingId);
}
void ATutorialDirector::OnPotionUsed(UItemDefinition* Item, float Healing, int32)
{
    if (Player.IsValid() && Item == Player->InventoryComponent->HealthPotionDefinition && Healing > 0.f)
        SendSignal(ETutorialSignal::PotionUsed,++EventSerial);
}
void ATutorialDirector::TrainingEnemyDefeated(ATutorialTrainingEnemy* E)
{
    if (!E || E != Target.Get() || E->TrainingMode != ETutorialEnemyMode::Combat ||
        E->FindComponentByClass<UHealthComponent>()->GetLastDamageSource() != Player.Get()) return;
    SendSignal(ETutorialSignal::EnemyDefeated,uint64(E->GetUniqueID()),E->TrainingId);
    if (Progress.Lesson == 5) SpawnAfter = GetWorld()->GetTimeSeconds() + 2.1;
}
void ATutorialDirector::RefreshWorld()
{
    for (TActorIterator<ATutorialGate> It(GetWorld()); It; ++It) It->SetOpen(Progress.IsUnlocked(It->UnlockLesson));
    const auto* O = Progress.CurrentObjective();
    for (const auto& Z : Zones) if (Z.IsValid())
        Z->SetHighlighted(O && !Z->bEntry && Z->LessonIndex == Progress.Lesson && Z->Signal == O->Signal && (O->TargetId.IsNone() || O->TargetId == Z->TargetId));
}
void ATutorialDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (!bInitialized) return;
    auto* P = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (P != Player.Get()) BindPlayer(P);
    if (!P || P->HealthComponent->GetCurrentHealth() <= 0) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (bRefreshPending) { bRefreshPending = false; ClearTarget(); ResetActionTracking(); RefreshWorld(); }
    if (SpawnAfter > 0 && Now >= SpawnAfter) { ClearTarget(); SpawnTarget(); }
    if (SummaryAfter > 0 && Now >= SummaryAfter)
    {
        SummaryAfter = -1.;
        if (!UGameplayStatics::IsGamePaused(this)) if (auto* PC = Cast<ATPCPlayerController>(P->GetController())) PC->TogglePauseMenu();
    }
    const FVector Location = P->GetActorLocation();
    if (Location.Z < -350.f || FMath::Abs(Location.X) > 6400.f || FMath::Abs(Location.Y) > 4900.f)
    { RetryLesson(); ShowToast(FText::FromString(TEXT("已回到当前练习起点。"))); return; }
    if (!bEntered) if (auto* E = EntryFor(Progress.Lesson); E && E->Contains(Location)) EnterCurrentLesson();
    // Locked areas cannot be bypassed by jumping around the visible gate.
    for (const auto& Z : Zones) if (Z.IsValid() && Z->bEntry && !Progress.IsUnlocked(Z->LessonIndex) && Z->Contains(Location))
    { RetryLesson(); return; }
    const bool Falling = P->GetCharacterMovement()->IsFalling();
    if (Falling && P->GetVelocity().Z > 100.f && P->ActionComponent->GetActionState() != ETPCActionState::Attack) bSawJump = true;
    if (bWasFalling && !Falling)
    {
        LastLandingTime = Now;
        if (PendingLandingZone.IsValid() && PendingLandingZone->Contains(Location) && bSawJump)
        { SendSignal(ETutorialSignal::JumpLanding,++EventSerial,PendingLandingZone->TargetId); bSawJump = false; }
        PendingLandingZone.Reset();
    }
    bWasFalling = Falling;
    const auto* CurrentSet=P->ActionComponent->GetActionSet();
    const bool bFinishingDive=CurrentSet && P->CombatComponent->IsMeleeAttackInProgress() && P->ActionComponent->GetActiveDefinition()==CurrentSet->Dive;
    if (!Falling && Now - LastLandingTime > .6 && !bFinishingDive) { AirNext = ChainPhase = 0; bSawJump = false; }
    if (P->ActionComponent->GetActionState() == ETPCActionState::Free) GroundNext = 0;
    // Poll only the active marker: arriving before acceleration/landing must not require backing out.
    const auto* O = Progress.CurrentObjective();
    if (O) for (const auto& Z : Zones)
        if (Z.IsValid() && !Z->bEntry && Z->LessonIndex == Progress.Lesson && Z->Signal == O->Signal && Z->Contains(Location)) HandleZone(Z.Get(),P);
    if (auto* E = Target.Get(); E && E->TrainingMode == ETutorialEnemyMode::Passive &&
        E->GetLaunchPhase() == EEnemyLaunchPhase::None && !E->IsHitReacting() && Now - LastTargetHitTime > 3.)
    {
        auto* H = E->FindComponentByClass<UHealthComponent>(); H->Heal(H->GetMaxHealth());
        if (const auto* Entry = EntryFor(Progress.Lesson); Entry && FVector::Dist2D(E->GetActorLocation(),Entry->TargetSpawn.GetLocation()) > 350.f)
        { E->GetCharacterMovement()->StopMovementImmediately(); E->SetActorTransform(Entry->TargetSpawn,false,nullptr,ETeleportType::TeleportPhysics); }
    }
}
void ATutorialDirector::CloseMenu()
{
    if (Player.IsValid()) if (auto* PC = Cast<ATPCPlayerController>(Player->GetController())) PC->RestoreGameplayInput();
}
void ATutorialDirector::RespawnAtCheckpoint()
{
    auto* Old = Player.Get(); auto* PC = Old ? Cast<APlayerController>(Old->GetController()) : nullptr;
    auto* GM = GetWorld()->GetAuthGameMode(); if (!PC || !GM) return;
    PC->UnPossess(); GM->RestartPlayer(PC);
    if (PC->GetPawn() && PC->GetPawn() != Old) Old->Destroy();
    else { PC->Possess(Old); UE_LOG(LogTemp,Error,TEXT("Tutorial restart failed; original pawn retained")); }
}
void ATutorialDirector::RetryLesson()
{
    CloseMenu(); bSummaryPending = false; SummaryAfter = -1.;
    if (Progress.Lesson == INDEX_NONE) Progress.StartLesson(LastLesson,true);
    Progress.Retry(); bEntered = false; ClearTarget(); ResetActionTracking(); RespawnAtCheckpoint(); RefreshWorld();
}
void ATutorialDirector::SkipLesson()
{
    CloseMenu(); bSummaryPending = false; SummaryAfter = -1.;
    if (Progress.Lesson != INDEX_NONE) LastLesson = Progress.Lesson;
    Progress.Skip(); bEntered = false; ClearTarget(); ResetActionTracking(); RefreshWorld();
    ShowToast(FText::FromString(TEXT("已跳过本节，记录保留为“跳过”。所有练习均可重试。")));
    if (Progress.BasicsFinished()) { bSummaryPending = true; SummaryAfter = GetWorld()->GetTimeSeconds() + .3; }
}
void ATutorialDirector::RestartCourse()
{
    CloseMenu(); Progress.Initialize(Course); LastLesson = 0; bEntered = bSummaryPending = false; SummaryAfter = -1.;
    ClearTarget(); ResetActionTracking(); RespawnAtCheckpoint(); RefreshWorld();
}
void ATutorialDirector::ContinuePractice() { bSummaryPending = false; CloseMenu(); }
void ATutorialDirector::ReturnToCampaign()
{
    if (!Course || Course->ReturnMap.IsNull()) return;
    CloseMenu();
    if (auto* Travel = UArenaTravelSubsystem::Get(this))
        Travel->Travel(Player.Get(), Course->ReturnMap.ToSoftObjectPath().GetLongPackageName(), TEXT("Campaign"));
}

FTutorialTravelSnapshot ATutorialDirector::ExportTravelSnapshot() const
{
    FTutorialTravelSnapshot S;
    S.CoursePath = Course ? Course->GetPathName() : FString();
    for (auto Status : Progress.Status) S.Status.Add(static_cast<uint8>(Status));
    S.Lesson = Progress.Lesson; S.Objective = Progress.Objective; S.Count = Progress.Count;
    S.LastLesson = LastLesson; S.bPractice = Progress.bPractice;
    return S;
}
bool ATutorialDirector::ImportTravelSnapshot(const FTutorialTravelSnapshot& S)
{
    if (!Course || S.CoursePath != Course->GetPathName() || S.Status.Num() != Course->Lessons.Num() ||
        (S.Lesson != INDEX_NONE && !Course->Lessons.IsValidIndex(S.Lesson))) return false;
    for (int32 I = 0; I < S.Status.Num(); ++I) Progress.Status[I] = static_cast<ETutorialLessonStatus>(S.Status[I]);
    Progress.Lesson = S.Lesson; Progress.Objective = S.Objective; Progress.Count = S.Count;
    Progress.bPractice = S.bPractice; LastLesson = S.LastLesson;
    Progress.Seen.Reset(); ResetActionTracking(); bEntered = bSummaryPending = false;
    return true;
}
void ATutorialDirector::ShowToast(const FText& Text) { Toast = Text; ToastUntil = GetWorld()->GetTimeSeconds() + 4.; }
FText ATutorialDirector::GetHeading() const
{
    return Course && Course->Lessons.IsValidIndex(Progress.Lesson) ? Course->Lessons[Progress.Lesson].Title : FText::FromString(TEXT("森林训练营 · 自由练习"));
}
FText ATutorialDirector::GetInstruction() const
{
    if (!Progress.CurrentObjective()) return FText::FromString(TEXT("基础教学已结束。走进任一训练区即可重练；北侧为可选空连场。Esc 可返回原地图。"));
    if (!bEntered) return FText::Format(FText::FromString(TEXT("沿金色石路，前往 {0}")),Course->Lessons[Progress.Lesson].Title);
    return Progress.CurrentObjective()->Description;
}
FText ATutorialDirector::GetKeys() const
{
    const auto* O = Progress.CurrentObjective(); return O && bEntered ? O->Keys : FText::FromString(TEXT("W A S D  移动  ·  Esc  教学菜单"));
}
FText ATutorialDirector::GetProgressText() const
{
    const auto* O = Progress.CurrentObjective();
    return O ? FText::FromString(FString::Printf(TEXT("练习 %d / %d    ·    %d / %d"),Progress.Objective+1,Course->Lessons[Progress.Lesson].Objectives.Num(),Progress.Count,O->RequiredCount))
        : FText::FromString(TEXT("基础课程已结束  ·  进阶练习可选"));
}
FText ATutorialDirector::GetStatusText() const
{
    if (const auto* Travel = UArenaTravelSubsystem::Get(this))
        if (!Travel->GetErrorText().IsEmpty()) return Travel->GetErrorText();
    if (Target.IsValid() && Target->IsThreatening()) return FText::FromString(TEXT("陪练准备出招！面向它，观察挥刀时机。"));
    if (GetWorld() && GetWorld()->GetTimeSeconds() < ToastUntil) return Toast;
    if (Progress.Lesson == 1 && GroundNext > 0) return FText::FromString(FString::Printf(TEXT("连击命中 %d / 4"),GroundNext));
    return FText::FromString(TEXT("卡住了？Esc → 重试本节 / 跳过本节"));
}
FText ATutorialDirector::GetCourseSummary() const
{
    FString Text;
    if (Course) for (int32 I=0; I<Progress.Status.Num(); ++I)
    {
        const auto S = Progress.Status[I];
        Text += FString::Printf(TEXT("%s  ·  %s\n"),*Course->Lessons[I].Title.ToString(),
            S == ETutorialLessonStatus::Completed ? TEXT("完成") : S == ETutorialLessonStatus::Skipped ? TEXT("跳过") : TEXT("待练习"));
    }
    return FText::FromString(Text.TrimEnd());
}
FVector ATutorialDirector::GetGuidanceLocation() const
{
    const auto* O = Progress.CurrentObjective();
    if (O && bEntered)
    {
        for (const auto& Z : Zones) if (Z.IsValid() && !Z->bEntry && Z->LessonIndex == Progress.Lesson && Z->TargetId == O->TargetId) return Z->GetActorLocation();
        if (Target.IsValid()) return Target->GetActorLocation();
    }
    if (const auto* E = EntryFor(Progress.Lesson)) return E->Checkpoint.GetLocation();
    if (const auto* E = EntryFor(6)) return E->Checkpoint.GetLocation();
    return GetActorLocation();
}
