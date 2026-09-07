#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/ActiveMontageInstanceScope.h"

inline int32 GetCombatNotifyMontageInstanceId(const FAnimNotifyEventReference& Reference)
{
    const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    return Context ? Context->MontageInstanceID : INDEX_NONE;
}
