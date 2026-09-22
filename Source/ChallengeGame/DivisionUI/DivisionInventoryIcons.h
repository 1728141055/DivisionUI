#pragma once

#include "CoreMinimal.h"

struct FGeometry;
class FSlateWindowElementList;

namespace DivisionInventoryIcons
{
    // Position/Size are local to Geometry. Icons are centered and aspect-fitted.
    // Keys: Rifle, SMG, Shotgun, LMG, Pistol, Vest, Mask, Backpack, Kneepads,
    // Gloves, Holster, Talent, Target, Shield, Biohazard, Currency.
    // Unknown keys and empty rectangles draw nothing. White Tint gives a white silhouette.
    CHALLENGEGAME_API void Draw(FName Icon, const FGeometry& Geometry,
        FSlateWindowElementList& Elements, int32 Layer, FVector2D Position,
        FVector2D Size, FLinearColor Tint);
}
