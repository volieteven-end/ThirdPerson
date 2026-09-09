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
