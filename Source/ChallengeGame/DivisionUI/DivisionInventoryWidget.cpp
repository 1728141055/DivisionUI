#include "DivisionInventoryWidget.h"
#include "DivisionInventoryContent.h"

#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "RenderDeferredCleanup.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SSpacer.h"

UDivisionInventoryWidget::UDivisionInventoryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Regular(
        TEXT("/Game/DivisionUIDemo/UI/Fonts/Borda_Regular_Font"));
    static ConstructorHelpers::FObjectFinder<UFont> Medium(
        TEXT("/Game/DivisionUIDemo/UI/Fonts/Borda_Medium_Font"));
    static ConstructorHelpers::FObjectFinder<UFont> Numbers(
        TEXT("/Game/DivisionUIDemo/UI/Fonts/Borda_DemiBold_Font"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
        TEXT("/Game/DivisionUIDemo/UI/M_DivisionInventoryPerspective"));
    RegularFont = Regular.Object;
    MediumFont = Medium.Object;
    NumberFont = Numbers.Object;
    PerspectiveMaterial = Material.Object;
}

void UDivisionInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
}

void UDivisionInventoryWidget::EnsureContent()
{
    if (!Content)
        Content = SNew(SDivisionInventoryContent).RegularFont(RegularFont.Get())
            .MediumFont(MediumFont.Get()).NumberFont(NumberFont.Get());
    if (!Renderer)
    {
        // Match SRetainerWidget: gamma-space premultiplied color, transparent clear.
        Renderer = new FWidgetRenderer(true, true);
        Renderer->SetApplyColorDeficiencyCorrection(false);
    }
    if (!PresentationMaterial && PerspectiveMaterial)
    {
        PresentationMaterial = UMaterialInstanceDynamic::Create(PerspectiveMaterial, this);
        PresentationBrush.SetResourceObject(PresentationMaterial);
        PresentationBrush.DrawAs = ESlateBrushDrawType::Image;
        bProjectionDirty = true;
    }
}

TSharedRef<SWidget> UDivisionInventoryWidget::RebuildWidget()
{
    EnsureContent();
    return SNew(SSpacer);
}

void UDivisionInventoryWidget::SetTransition(float Progress)
{
    Progress = FMath::Clamp(Progress, 0.f, 1.f);
    if (RevealProgress != Progress)
    {
        RevealProgress = Progress;
        bProjectionDirty = true;
        // NativeTick skips zero reveal; invalidate here so the final hidden frame is painted too.
        if (const TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
            SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void UDivisionInventoryWidget::OpenOverview()
{
    EnsureContent();
    Content->ResetPage();
    DrawnRevision = MAX_uint64;
}

bool UDivisionInventoryWidget::Back()
{
    return Content && Content->Back();
}

void UDivisionInventoryWidget::UpdateProjection(const FGeometry& Geometry)
{
    const FVector2D ViewSize = Geometry.GetLocalSize();
    if (ViewSize.X <= 0 || ViewSize.Y <= 0 || !PresentationMaterial) return;
    if (!bProjectionDirty && LastViewSize.Equals(ViewSize, .01)) return;
    LastViewSize = ViewSize;
    bProjectionDirty = false;
    // Fit the reference composition; ultra-wide displays keep typography proportions.
    const double Scale = FMath::Min(ViewSize.X / 1920.0, ViewSize.Y / 1080.0);
    const FVector2D ReferenceSize = FVector2D(1920, 1080) * Scale;
    const FVector2D Origin = (ViewSize - ReferenceSize) * .5;
    const auto Corner = [&](double X, double Y)
    {
        // A short unfolding transition affects the whole plane, including its hit mapping.
        Y = .4875 + (Y - .4875) * FMath::Max(.01f, RevealProgress);
        X += .014 * (1.0 - RevealProgress);
        return (Origin + FVector2D(X, Y) * ReferenceSize) / ViewSize;
    };
    bProjectionValid = Projection.SetQuad(Corner(.288,.100), Corner(.918,.070),
        Corner(.890,.905), Corner(.303,.805));
    if (bProjectionValid)
    {
        PresentationMaterial->SetVectorParameterValue(TEXT("InverseRow0"), Projection.MaterialRow(0));
        PresentationMaterial->SetVectorParameterValue(TEXT("InverseRow1"), Projection.MaterialRow(1));
        PresentationMaterial->SetVectorParameterValue(TEXT("InverseRow2"), Projection.MaterialRow(2));
        PresentationMaterial->SetScalarParameterValue(TEXT("Reveal"), RevealProgress);
    }
    if (const TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
        SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
}

void UDivisionInventoryWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if (RevealProgress <= 0) return;
    EnsureContent();
    UpdateProjection(Geometry);
    if (!PresentationMaterial || !bProjectionValid) return;

    const FVector2D PixelSize = Geometry.GetLocalSize() * Geometry.GetAccumulatedLayoutTransform().GetScale();
    // Custom Slate vertices do not expose a per-element AA flag. Keep the
    // offscreen surface supersampled so icon silhouettes and box edges receive
    // a filtered coverage sample when the perspective material is composed.
    const float ResolutionScale = FMath::Clamp(static_cast<float>(
        FMath::Min(PixelSize.X / 1920.0, PixelSize.Y / 1080.0) * 1.5), 1.5f, 2.56f);
    const int32 TargetWidth = FMath::RoundToInt(1200 * ResolutionScale);
    const int32 TargetHeight = FMath::RoundToInt(900 * ResolutionScale);
    if (!ContentTarget)
    {
        ContentTarget = FWidgetRenderer::CreateTargetFor(FVector2D(TargetWidth, TargetHeight), TF_Bilinear, true);
        PresentationMaterial->SetTextureParameterValue(TEXT("InventoryTexture"), ContentTarget);
        DrawnRevision = MAX_uint64;
    }
    else if (ContentTarget->SizeX != TargetWidth || ContentTarget->SizeY != TargetHeight)
    {
        ContentTarget->ResizeTarget(TargetWidth, TargetHeight);
        DrawnRevision = MAX_uint64;
    }
    const bool bAnimating = Content->Advance(DeltaSeconds);
    if (ContentTarget && (DrawnRevision != Content->GetRevision() || bAnimating))
    {
        Renderer->DrawWidget(ContentTarget, Content.ToSharedRef(), ResolutionScale,
            FVector2D(TargetWidth, TargetHeight), DeltaSeconds);
        DrawnRevision = Content->GetRevision();
    }
}

int32 UDivisionInventoryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 Base = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
    if (RevealProgress <= 0 || !ContentTarget || !PresentationMaterial || !bProjectionValid) return Base;
    // Material is AlphaComposite; do not multiply the RT's alpha a second time.
    FLinearColor Tint = Style.GetColorAndOpacityTint();
    Tint.R *= Tint.A;
    Tint.G *= Tint.A;
    Tint.B *= Tint.A;
    FSlateDrawElement::MakeBox(Elements, Base + 1, Geometry.ToPaintGeometry(), &PresentationBrush,
        ESlateDrawEffect::PreMultipliedAlpha | ESlateDrawEffect::NoGamma, Tint);
    return Base + 1;
}

bool UDivisionInventoryWidget::MapPointer(const FGeometry& Geometry, const FPointerEvent& Event, FVector2D& Logical)
{
    UpdateProjection(Geometry);
    const FVector2D Size = Geometry.GetLocalSize();
    if (!bProjectionValid || RevealProgress < .95f || Size.X <= 0 || Size.Y <= 0) return false;
    return Projection.ScreenToLogical(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) / Size, Logical);
}

FReply UDivisionInventoryWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    // Slate also sends stationary synthetic moves after keyboard navigation/layout work.
    // Those must not return focus to the item underneath an unmoved mouse.
    const FVector2f CursorDelta(Event.GetCursorDelta());
    if (CursorDelta.IsNearlyZero()) return FReply::Handled();
    FVector2D Logical;
    if (Content)
    {
        if (MapPointer(Geometry, Event, Logical)) Content->PointerMove(Logical);
        else Content->PointerLeave();
    }
    return FReply::Handled();
}

FReply UDivisionInventoryWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    FVector2D Logical;
    if (Content && Event.GetEffectingButton() == EKeys::LeftMouseButton && MapPointer(Geometry, Event, Logical))
        Content->PointerDown(Logical);
    return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UDivisionInventoryWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    FVector2D Logical;
    if (Content && MapPointer(Geometry, Event, Logical)) Content->Scroll(Event.GetWheelDelta(), Logical);
    return FReply::Handled();
}

void UDivisionInventoryWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
    if (Content) Content->PointerLeave();
    Super::NativeOnMouseLeave(Event);
}

FReply UDivisionInventoryWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Content && RevealProgress >= .95f && Content->Key(Event.GetKey())) return FReply::Handled();
    return Super::NativeOnKeyDown(Geometry, Event);
}

void UDivisionInventoryWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Content.Reset();
    // DrawWidget enqueues render-thread work: immediate deletion here is unsafe.
    if (Renderer) BeginCleanup(Renderer);
    Renderer = nullptr;
    PresentationBrush.SetResourceObject(nullptr);
    PresentationMaterial = nullptr;
    ContentTarget = nullptr;
    DrawnRevision = MAX_uint64;
    bProjectionDirty = true;
}
