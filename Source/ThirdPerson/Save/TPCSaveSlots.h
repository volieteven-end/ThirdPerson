// 集中解析存档槽位；自动化可通过 TPCSaveSlot 指定临时槽位，不能覆盖玩家正式存档。
#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace TPCSaveSlots
{
// Development automation can use a unique disposable slot without touching PlayerSave.
inline FString Resolve(const FString& DefaultSlot = TEXT("PlayerSave"))
{
#if WITH_DEV_AUTOMATION_TESTS
	FString TestSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("TPCSaveSlot="), TestSlot) && !TestSlot.IsEmpty()) return TestSlot;
#endif
	return DefaultSlot;
}
}
