
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

// This class does not need to be modified.
UINTERFACE()
/** 供 Unreal 反射识别的交互接口类型；实际交互约定由 IInteractable 定义。 */
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/** 可交互对象的共用约定：提供提示文本并接收交互发起者，由各 Actor 实现具体行为。 */
class THIRDPERSON_API IInteractable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual FText GetInteractionText() const=0;
	virtual void Interact(APawn* InstigatorPawn)=0;
};
