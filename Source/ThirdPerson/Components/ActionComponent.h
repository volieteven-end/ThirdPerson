#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Actions/ActionDefinition.h"
#include "../Animation/CombatActionRules.h"
#include "ActionComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActionPlaybackStarted, uint64, ETPCActionState, const UActionDefinition*);

/** 主角全身动作的唯一状态通道：管理输入缓存、动作实例、取消和转向权限；战斗组件负责命中，角色负责物理移动。 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UActionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UActionComponent();
    UFUNCTION(BlueprintPure) ETPCActionState GetActionState() const { return State; }
    UFUNCTION(BlueprintPure) ETPCMovementContext GetMovementContext() const;
    UFUNCTION(BlueprintPure) const UActionDefinition* GetActiveDefinition() const { return Definition; }
    UFUNCTION(BlueprintPure) const UActionSet* GetActionSet() const;
    UFUNCTION(BlueprintPure) bool IsMovementLocked() const;
    UFUNCTION(BlueprintPure) bool OwnsRotation() const;
    UFUNCTION(BlueprintPure) bool CanRequest(ETPCActionIntent Intent) const;
    UFUNCTION(BlueprintPure) bool IsInvulnerable() const;
    UFUNCTION(BlueprintPure) bool IsFacingWindowOpen() const;
    UFUNCTION(BlueprintPure) float GetMontagePosition() const;
    uint64 GetActionInstanceId() const { return InstanceId; }
    int32 GetMontageInstanceId() const { return MontageInstanceId; }
    /** 只用于观察：绑定实际蒙太奇实例后才广播，不允许监听者代替动作系统发起播放。 */
    FOnActionPlaybackStarted OnActionPlaybackStarted;
    bool AuthorizeOrBuffer(ETPCActionIntent Intent);
    void BufferIntent(ETPCActionIntent Intent, bool bRequireGround = false);
    void ClearInputBuffers();
    /** 播放前调用并生成新的动作编号，旧回调不能结束新动作。 */
    uint64 BeginAction(ETPCActionState InState, const UActionDefinition* InDefinition = nullptr);
    void BindMontage(uint64 ExpectedId, UAnimMontage* Montage, int32 InMontageInstanceId);
    void EndAction(uint64 ExpectedId);
    void EnterDead();
    void SetInputSuppressed(bool bSuppressed);
    void SetDamageWindowActive(bool bActive) { bDamageActive = bActive; }
    TPCActionRules::FComboBuffer& GetComboBuffer() { return ComboBuffer; }
    void SetFallbackInvulnerability(float Duration) { FallbackInvulnerabilityEnd = Duration; }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin="0.15",ClampMax="0.25"))
    float InputBufferDuration = 0.2f;
protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FTPCSwordActionTestAccess;
    void CleanupActionTransient();
    void DispatchIntent(ETPCActionIntent Intent);
    UPROPERTY(Transient) ETPCActionState State = ETPCActionState::Free;
    UPROPERTY(Transient) TObjectPtr<const UActionDefinition> Definition;
    TWeakObjectPtr<UAnimMontage> ActiveMontage;
    uint64 InstanceId = 0;
    int32 MontageInstanceId = INDEX_NONE;
    TPCActionRules::FComboBuffer ComboBuffer;
    ETPCActionIntent BufferedIntent = ETPCActionIntent::None;
    double BufferedUntil = 0.;
    float StartedAt = 0.f;
    float FallbackInvulnerabilityEnd = 0.f;
    bool bDamageActive = false;
    bool bCommitted = false;
    uint64 PublishedPlaybackId = 0;
    bool bInputSuppressed = false;
    bool bBufferedRequiresGround = false;
};
