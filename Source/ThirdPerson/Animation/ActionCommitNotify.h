#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ActionCommitNotify.generated.h"

/** Commits movement, buff or equipment once on the owning action instance. */
UCLASS(meta=(DisplayName="Action Commit"))
class THIRDPERSON_API UActionCommitNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& Reference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Action Commit"); }
};
