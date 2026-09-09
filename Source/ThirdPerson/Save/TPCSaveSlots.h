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
