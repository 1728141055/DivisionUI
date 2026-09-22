#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace DivisionEdgePaint
{
    // Solid edge with uniform opacity. Use Slate's line shader so the edge is
    // antialiased even when a parent render transform places it between pixels.
    inline void SolidLine(const FGeometry& Geometry, FSlateWindowElementList& Elements,
        int32 Layer, FVector2D Start, FVector2D End, FLinearColor Color, float Thickness = 1.0f)
    {
        if ((End - Start).SizeSquared() <= UE_SMALL_NUMBER) return;
        TArray<FVector2f> Points;
        Points.Reserve(2);
        Points.Add(FVector2f(Start));
        Points.Add(FVector2f(End));
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
            MoveTemp(Points), ESlateDrawEffect::None, Color, true, Thickness);
    }
}
