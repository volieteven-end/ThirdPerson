// 角色进度的读取／恢复接口，仅处理可保存的角色数据；地图位置和门状态由所属地图流程负责。
#pragma once
#include "CoreMinimal.h"
class ATPCCharacter;
class UTPCSaveGame;

/** Common checkpoint / portal serialization. Never captures world-owned actor pointers. */
namespace TPCPlayerProgress
{
    void Capture(const ATPCCharacter& Player, UTPCSaveGame& Save, bool bRestoreVitals = false);
    void Apply(ATPCCharacter& Player, const UTPCSaveGame& Save);
    bool OwnsCheckpoint(const UTPCSaveGame& Save, const UObject* WorldContext);
}
