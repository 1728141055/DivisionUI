#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "DivisionMenuWidget.generated.h"

// Inventory camera only. The former settings widget has been removed.
UCLASS()
class CHALLENGEGAME_API UDivisionMenuCameraModifier : public UCameraModifier
{
    GENERATED_BODY()
public:
    float PresentationAmount = 0.0f;
    float PlayerScreenX = 0.18f;
    float InventoryFOV = 45.0f;
    float InventoryOrbitYaw = 25.0f;
    float InventoryFStop = 1.4f;
    float InventorySensorWidth = 36.0f;
    float InventorySceneGain = 0.60f;
    float InventoryHighlightGain = 0.80f;
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};
