// 无世界状态的近战几何辅助函数，统一从角色原点缩放检测距离，避免不同攻击路径使用不同范围公式。
#pragma once
#include "CoreMinimal.h"

namespace TPCMeleeTrace
{
    // Extend horizontal reach without moving the authored hit plane vertically.
    inline FVector Extend(const FVector& Point, const FVector& Origin, float Scale)
    {
        return FVector(Origin.X + (Point.X-Origin.X)*Scale,
            Origin.Y + (Point.Y-Origin.Y)*Scale, Point.Z);
    }
}
