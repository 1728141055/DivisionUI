#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "DivisionInventoryProjection.h"
#include "DivisionInventoryWidget.generated.h"

class FWidgetRenderer;
class SDivisionInventoryContent;
class UFont;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

// Presentation only: the content stays in a 1200 x 900 logical coordinate system.
UCLASS()
class CHALLENGEGAME_API UDivisionInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    explicit UDivisionInventoryWidget(const FObjectInitializer& ObjectInitializer);
    void SetTransition(float Progress);
    void OpenOverview();
    bool Back();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
        const FWidgetStyle& Style, bool bParentEnabled) const override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    void UpdateProjection(const FGeometry& Geometry);
    bool MapPointer(const FGeometry& Geometry, const FPointerEvent& Event, FVector2D& Logical);
    void EnsureContent();
    UPROPERTY() TObjectPtr<UFont> RegularFont;
    UPROPERTY() TObjectPtr<UFont> MediumFont;
    UPROPERTY() TObjectPtr<UFont> NumberFont;
    UPROPERTY() TObjectPtr<UMaterialInterface> PerspectiveMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PresentationMaterial;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> ContentTarget;
    TSharedPtr<SDivisionInventoryContent> Content;
    FWidgetRenderer* Renderer = nullptr;
    FSlateBrush PresentationBrush;
    FDivisionInventoryProjection Projection;
    FVector2D LastViewSize = FVector2D::ZeroVector;
    uint64 DrawnRevision = MAX_uint64;
    float RevealProgress = 0;
    bool bProjectionDirty = true;
    bool bProjectionValid = false;
};
