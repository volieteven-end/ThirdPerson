// 从动画通知事件中提取蒙太奇实例编号，供动作归属校验使用；不能只比较动画资源指针。
#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/ActiveMontageInstanceScope.h"

inline int32 GetCombatNotifyMontageInstanceId(const FAnimNotifyEventReference& Reference)
{
    const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    return Context ? Context->MontageInstanceID : INDEX_NONE;
}
