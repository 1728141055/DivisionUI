#include "DivisionMenuWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

bool UDivisionMenuCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    APawn* Pawn = CameraOwner && CameraOwner->PCOwner ? CameraOwner->PCOwner->GetPawn() : nullptr;
    if (PresentationAmount <= 0.0f || !Pawn || GetViewTarget() != Pawn)
        return false;

    // Fit the existing pawn; never move it or replace its animation/pose.
    float Radius = 0;
    float HalfHeight = 0;
    Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
    HalfHeight = FMath::Max(HalfHeight, 70.0f);
    float Aspect = FMath::Max(InOutPOV.AspectRatio, 1.0f);
    if (!InOutPOV.bConstrainAspectRatio)
    {
        int32 ViewWidth = 0;
        int32 ViewHeight = 0;
        CameraOwner->PCOwner->GetViewportSize(ViewWidth, ViewHeight);
        if (ViewHeight > 0) Aspect = static_cast<float>(ViewWidth) / ViewHeight;
    }
    const float HalfHorizontal = FMath::Tan(FMath::DegreesToRadians(InventoryFOV * .5f));
    const float HalfVertical = HalfHorizontal / Aspect;
    const float Distance = HalfHeight / (HalfVertical * .82f);
    const FRotator PortraitRotation(0, Pawn->GetActorRotation().Yaw + InventoryOrbitYaw, 0);
    const FRotationMatrix Basis(PortraitRotation);
    const FVector Forward = Basis.GetUnitAxis(EAxis::X);
    const FVector Right = Basis.GetUnitAxis(EAxis::Y);
    const FVector Up = Basis.GetUnitAxis(EAxis::Z);
    const FVector Center = Pawn->GetActorLocation();
    const float PortraitScreenX = .5f + (PlayerScreenX - .5f)
        * FMath::Min(1.0f, (16.0f / 9.0f) / Aspect);
    FVector PortraitLocation = Center - Forward * Distance
        - Right * ((PortraitScreenX * 2.0f - 1.0f) * Distance * HalfHorizontal)
        + Up * (.12f * Distance * HalfVertical);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DivisionInventoryCamera), false, Pawn);
    if (GetWorld()->SweepSingleByChannel(Hit, Center, PortraitLocation, FQuat::Identity,
        ECC_Camera, FCollisionShape::MakeSphere(12.0f), Query))
    {
        PortraitLocation = Hit.Location;
    }
    const float Blend = FMath::Clamp(PresentationAmount, 0.0f, 1.0f);
    InOutPOV.Location = FMath::Lerp(InOutPOV.Location, PortraitLocation, Blend);
    InOutPOV.Rotation = FQuat::Slerp(InOutPOV.Rotation.Quaternion(),
        PortraitRotation.Quaternion(), Blend).Rotator();
    InOutPOV.FOV = FMath::Lerp(InOutPOV.FOV, InventoryFOV, Blend);

    FPostProcessSettings Focus;
    Focus.bOverride_DepthOfFieldEnabled = true;
    Focus.DepthOfFieldEnabled = true;
    Focus.bOverride_DepthOfFieldFstop = true;
    Focus.DepthOfFieldFstop = InventoryFStop;
    Focus.bOverride_DepthOfFieldSensorWidth = true;
    Focus.DepthOfFieldSensorWidth = InventorySensorWidth;
    Focus.bOverride_DepthOfFieldFocalDistance = true;
    const FVector Torso = Center + FVector(0, 0, HalfHeight * .3f);
    Focus.DepthOfFieldFocalDistance = FMath::Max(10.0f, static_cast<float>(
        FVector::DotProduct(Torso - InOutPOV.Location, InOutPOV.Rotation.Vector())));
    // Reduce scene highlights without changing exposure metering or dimming Slate text.
    // These are menu-only color-grading contributions, not a fullscreen opaque UI plate.
    Focus.bOverride_ColorGain = true;
    Focus.ColorGain = FVector4(InventorySceneGain, InventorySceneGain, InventorySceneGain, 1.0f);
    Focus.bOverride_ColorGainHighlights = true;
    Focus.ColorGainHighlights = FVector4(InventoryHighlightGain, InventoryHighlightGain,
        InventoryHighlightGain, 1.0f);
    // LocalPlayer applies Base blends BEFORE the main camera's own PP. Apply the menu
    // after that camera so a camera-level DOF override cannot silently erase this focus.
    // Cached blends are cleared every camera update, so closing restores scene settings.
    CameraOwner->AddCachedPPBlend(Focus, Blend, VTBlendOrder_Override);
    return false;
}
