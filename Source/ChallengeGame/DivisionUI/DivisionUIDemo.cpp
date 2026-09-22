#include "DivisionUIDemo.h"
#include "DivisionMenuWidget.h"
#include "DivisionInventoryWidget.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/SViewport.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

namespace DivisionPaint
{
    const FLinearColor Orange = FLinearColor::FromSRGBColor(FColor(255, 139, 16));
    const FLinearColor White = FLinearColor::FromSRGBColor(FColor(243, 243, 239));
    const FLinearColor Dim(0.55f, 0.56f, 0.57f, 0.85f);
    const FLinearColor Back(0.025f, 0.03f, 0.035f, 0.44f);
    // Linear neutral grey; the common wash is lighter than each functional cell.
    // These are reconstruction values, not measured Snowdrop material parameters.
    const FLinearColor StatusGroupTint(0.12f, 0.12f, 0.12f, 0.10f);
    const FLinearColor StatusCellTint(0.12f, 0.12f, 0.12f, 0.18f);
    const FLinearColor StatusCornerTint(0.8f, 0.8f, 0.8f, 0.22f);
    const FVector2D StatusProjectionOffset(2.0f, 2.0f);
    const FLinearColor StatusProjectionTint(0.55f, 0.55f, 0.55f, 0.10f);
    // The original UI uses Borda. This project now supplies the Borda
    // DemiBold UFont asset; BoldCondensed remains the safe editor fallback.
    const FName HudNumericTypeface(TEXT("BordaNumeric"));
    const FName HudFallbackTypeface(TEXT("BoldCondensed"));
    const FName BordaDefaultTypeface(TEXT("Default"));

    struct FDraw
    {
        const FGeometry& Geometry;
        FSlateWindowElementList& Elements;
        int32 Layer;
        float Opacity = 1.0f;
        const UFont* BordaNumericFont = nullptr;
        // Opt-in: all duplicate glyphs sit behind the sharp foreground.
        int32 ProjectionLayer = INDEX_NONE;
        float EdgeReveal = 1.0f;

        FSlateFontInfo GetFont(FName Typeface, int32 Size) const
        {
            if (Typeface == HudNumericTypeface)
            {
                if (BordaNumericFont)
                {
                    return FSlateFontInfo(BordaNumericFont, Size, BordaDefaultTypeface);
                }
                return FCoreStyle::GetDefaultFontStyle(HudFallbackTypeface, Size);
            }
            return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
        }

        float GetBaselineOffset(const FString& Value, int32 Size, FName Typeface) const
        {
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const FSlateFontInfo Font = GetFont(Typeface, Size);
            // Slate's baseline is measured from the bottom of the font box and
            // is negative, so this converts a top-left Y into a baseline Y.
            return Measure->Measure(Value, Font).Y + Measure->GetBaseline(Font);
        }

        void Box(FVector2D Position, FVector2D Size, FLinearColor Color,
            bool bAntialiasEdge = true) const
        {
            Color.A *= Opacity;
            if (ProjectionLayer != INDEX_NONE)
            {
                FLinearColor ProjectionColor = StatusProjectionTint;
                ProjectionColor.A *= Color.A;
                const FPaintGeometry ProjectionGeometry = Geometry.ToPaintGeometry(Size,
                    FSlateLayoutTransform(Position + StatusProjectionOffset));
                FSlateDrawElement::MakeBox(Elements, ProjectionLayer, ProjectionGeometry,
                    FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
                    ProjectionColor);
                if (bAntialiasEdge && Size.X >= 4.0f && Size.Y >= 4.0f)
                {
                    FLinearColor EdgeColor = ProjectionColor;
                    EdgeColor.A *= 0.55f;
                    FSlateDrawElement::MakeGeometryOutline(Elements, ProjectionLayer,
                        ProjectionGeometry, EdgeColor, ESlateDrawEffect::NoPixelSnapping,
                        true, 1.0f);
                }
            }
            const FPaintGeometry BoxGeometry = Geometry.ToPaintGeometry(Size,
                FSlateLayoutTransform(Position));
            FSlateDrawElement::MakeBox(Elements, Layer, BoxGeometry,
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
            if (bAntialiasEdge && Size.X >= 4.0f && Size.Y >= 4.0f)
            {
                // MakeBox has no antialias flag. An antialiased outline supplies
                // coverage at the perimeter while preserving the hard-edged fill.
                FLinearColor EdgeColor = Color;
                EdgeColor.A *= 0.55f;
                FSlateDrawElement::MakeGeometryOutline(Elements, Layer, BoxGeometry,
                    EdgeColor, ESlateDrawEffect::NoPixelSnapping, true, 1.0f);
            }
        }

        void Text(FVector2D Position, const FString& Value, int32 Size,
            FLinearColor Color = White, FName Typeface = TEXT("Regular")) const
        {
            Color.A *= Opacity;
            const FSlateFontInfo Font = GetFont(Typeface, Size);
            if (ProjectionLayer != INDEX_NONE)
            {
                FLinearColor ProjectionColor = StatusProjectionTint;
                ProjectionColor.A *= Color.A;
                FSlateDrawElement::MakeText(Elements, ProjectionLayer,
                    Geometry.ToPaintGeometry(FVector2D(320.0f, 42.0f),
                        FSlateLayoutTransform(Position + StatusProjectionOffset)),
                    Value, Font, ESlateDrawEffect::None, ProjectionColor);
            }
            // A small dark shadow preserves white glyphs over snow without enclosing the HUD.
            FSlateDrawElement::MakeText(Elements, Layer + 1,
                Geometry.ToPaintGeometry(FVector2D(320.0f, 42.0f), FSlateLayoutTransform(Position + FVector2D(1, 1))),
                Value, Font,
                ESlateDrawEffect::None, FLinearColor(0, 0, 0, Color.A * 0.45f));
            FSlateDrawElement::MakeText(Elements, Layer + 2,
                Geometry.ToPaintGeometry(FVector2D(320.0f, 42.0f), FSlateLayoutTransform(Position)),
                Value, Font,
                ESlateDrawEffect::None, Color);
        }

        void ProjectionText(FVector2D Position, const FString& Value, int32 Size,
            FLinearColor Color = StatusProjectionTint,
            FName Typeface = TEXT("Regular")) const
        {
            Color.A *= Opacity;
            FSlateDrawElement::MakeText(Elements, Layer,
                Geometry.ToPaintGeometry(FVector2D(320.0f, 42.0f),
                    FSlateLayoutTransform(Position + StatusProjectionOffset)),
                Value, GetFont(Typeface, Size), ESlateDrawEffect::None, Color);
        }

        void Number(FVector2D Position, float Width, const FString& Value, int32 Size,
            FLinearColor Color = White, float HorizontalAlignment = 1.0f) const
        {
            NumberAtBaseline(Position.X, Width, Value, Size,
                Position.Y + GetBaselineOffset(Value, Size, HudNumericTypeface), Color,
                HorizontalAlignment);
        }

        void NumberAtBaseline(float X, float Width, const FString& Value, int32 Size,
            float BaselineY, FLinearColor Color = White,
            float HorizontalAlignment = 1.0f) const
        {
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FSlateFontInfo Font = GetFont(HudNumericTypeface, Size);
            float TextWidth = Measure->Measure(Value, Font).X;
            while (TextWidth > Width && Size > 10)
            {
                Font.Size = --Size;
                TextWidth = Measure->Measure(Value, Font).X;
            }
            Text({X + (Width - TextWidth) * HorizontalAlignment,
                    BaselineY - GetBaselineOffset(Value, Size, HudNumericTypeface)},
                Value, Size, Color,
                HudNumericTypeface);
        }

        void NumberCentered(FVector2D Position, FVector2D Area, const FString& Value,
            int32 Size, FLinearColor Color = White) const
        {
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FSlateFontInfo Font = GetFont(HudNumericTypeface, Size);
            float TextWidth = Measure->Measure(Value, Font).X;
            while (TextWidth > Area.X && Size > 10)
            {
                Font.Size = --Size;
                TextWidth = Measure->Measure(Value, Font).X;
            }
            const float TextHeight = Measure->Measure(Value, Font).Y;
            Text({Position.X + (Area.X - TextWidth) * 0.5f,
                    Position.Y + (Area.Y - TextHeight) * 0.5f},
                Value, Size, Color, HudNumericTypeface);
        }

        void TextAtBaseline(float X, float BaselineY, const FString& Value, int32 Size,
            FLinearColor Color = White, FName Typeface = TEXT("Regular")) const
        {
            Text({X, BaselineY - GetBaselineOffset(Value, Size, Typeface)}, Value, Size,
                Color, Typeface);
        }

        void TextCenteredAtBaseline(float CenterX, float BaselineY, const FString& Value,
            int32 Size, FLinearColor Color = White, FName Typeface = TEXT("Regular")) const
        {
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const float TextWidth = Measure->Measure(Value, GetFont(Typeface, Size)).X;
            TextAtBaseline(CenterX - TextWidth * 0.5f, BaselineY, Value, Size,
                Color, Typeface);
        }

        void Lines(const TArray<FVector2D>& Points, FLinearColor Color = White,
            float Width = 1.5f) const
        {
            Color.A *= Opacity;
            TArray<FVector2f> SlatePoints;
            SlatePoints.Reserve(Points.Num());
            for (const FVector2D& Point : Points)
            {
                SlatePoints.Add(FVector2f(Point));
            }
            if (ProjectionLayer != INDEX_NONE)
            {
                TArray<FVector2f> ProjectionPoints;
                ProjectionPoints.Reserve(Points.Num());
                for (const FVector2D& Point : Points)
                    ProjectionPoints.Add(FVector2f(Point + StatusProjectionOffset));
                FLinearColor ProjectionColor = StatusProjectionTint;
                ProjectionColor.A *= Color.A;
                FSlateDrawElement::MakeLines(Elements, ProjectionLayer, Geometry.ToPaintGeometry(),
                    MoveTemp(ProjectionPoints), ESlateDrawEffect::None, ProjectionColor, true, Width);
            }
            FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(),
                MoveTemp(SlatePoints), ESlateDrawEffect::None, Color, true, Width);
        }

        void Ring(FVector2D Center, float Radius, FLinearColor Color,
            float Start = 0.0f, float Arc = 2.0f * PI) const
        {
            TArray<FVector2D> Points;
            const int32 Steps = 40;
            for (int32 Index = 0; Index <= Steps; ++Index)
            {
                const float Angle = Start + Arc * Index / Steps;
                Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
            }
            Lines(Points, Color, 1.2f);
        }

        void StatusCorners(FVector2D Position, FVector2D Size) const
        {
            const FVector2D TopLeft = Position + FVector2D(0.5f, 0.5f);
            const FVector2D BottomRight = Position + Size - FVector2D(0.5f, 0.5f);
            const float Extent = 3.0f * EdgeReveal;
            Lines({TopLeft + FVector2D(0, Extent), TopLeft,
                TopLeft + FVector2D(Extent, 0)}, StatusCornerTint, 1.0f);
            Lines({BottomRight - FVector2D(Extent, 0), BottomRight,
                BottomRight - FVector2D(0, Extent)}, StatusCornerTint, 1.0f);
        }

        void StatusCell(FVector2D Position, FVector2D Size) const
        {
            Box(Position, Size, StatusCellTint, true);
            StatusCorners(Position, Size);
        }

        void Cross(FVector2D P) const
        {
            Box(P + FVector2D(6, 0), FVector2D(7, 20), White);
            // Non-overlapping arms avoid doubling the translucent projection at the center.
            Box(P + FVector2D(0, 6), FVector2D(6, 7), White);
            Box(P + FVector2D(13, 6), FVector2D(6, 7), White);
        }

        void Grenade(FVector2D P) const
        {
            Ring(P + FVector2D(8, 12), 7, White);
            Lines({P + FVector2D(5, 4), P + FVector2D(5, 0),
                P + FVector2D(11, 0), P + FVector2D(15, 7)}, White, 2);
            Lines({P + FVector2D(2, 10), P + FVector2D(14, 14)}, White, 2);
        }

        void Pulse(FVector2D P) const
        {
            Ring(P + FVector2D(15, 17), 11, White, -0.7f, 4.8f);
            Ring(P + FVector2D(15, 17), 6, White, -0.7f, 4.8f);
            Lines({P + FVector2D(5, 28), P + FVector2D(24, 8)}, White, 2);
            Lines({P + FVector2D(24, 8), P + FVector2D(24, 1),
                P + FVector2D(29, 1), P + FVector2D(29, 9)}, White, 2);
            Lines({P + FVector2D(17, 1), P + FVector2D(14, 1),
                P + FVector2D(14, 8), P + FVector2D(18, 12),
                P + FVector2D(20, 7)}, White, 2);
        }

        void Turret(FVector2D P) const
        {
            Lines({P + FVector2D(1, 9), P + FVector2D(9, 9),
                P + FVector2D(9, 3), P + FVector2D(23, 3),
                P + FVector2D(23, 13), P + FVector2D(7, 13)}, White, 2);
            Lines({P + FVector2D(16, 13), P + FVector2D(16, 20),
                P + FVector2D(5, 29), P + FVector2D(2, 29)}, White, 2);
            Lines({P + FVector2D(16, 20), P + FVector2D(28, 29),
                P + FVector2D(31, 29)}, White, 2);
            Lines({P + FVector2D(16, 20), P + FVector2D(16, 30)}, White, 2);
        }
};
}

// Foreground is a child of the blur: Slate guarantees it is painted after the background pass.
class SDivisionStatusContent : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDivisionStatusContent) {}
        SLATE_ARGUMENT(UDivisionStatusWidget*, Owner)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs) { Owner = InArgs._Owner; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(238, 108); }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
        const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        if (const UDivisionStatusWidget* Widget = Owner.Get())
            return Widget->PaintStatus(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
        return LayerId;
    }
private:
    TWeakObjectPtr<UDivisionStatusWidget> Owner;
};

UDivisionStatusWidget::UDivisionStatusWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> BordaFontFinder(
        TEXT("/Game/DivisionUIDemo/UI/Fonts/Borda_DemiBold_Font.Borda_DemiBold_Font"));
    if (BordaFontFinder.Succeeded())
    {
        BordaNumericFont = BordaFontFinder.Object;
    }
}

UDivisionScreenWidget::UDivisionScreenWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> BordaFontFinder(
        TEXT("/Game/DivisionUIDemo/UI/Fonts/Borda_DemiBold_Font.Borda_DemiBold_Font"));
    if (BordaFontFinder.Succeeded())
    {
        BordaNumericFont = BordaFontFinder.Object;
    }
}

TSharedRef<SWidget> UDivisionStatusWidget::RebuildWidget()
{
    return SNew(SBox).WidthOverride(254).HeightOverride(124)
    [
        SAssignNew(BackgroundBlur, SBackgroundBlur)
        .Padding(0).BlurStrength(BlurStrength).bApplyAlphaToBlur(true)
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(DivisionPaint::StatusGroupTint)
            .Padding(FMargin(8))
            [ SAssignNew(StatusContent, SDivisionStatusContent).Owner(this) ]
        ]
    ];
}

void UDivisionStatusWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    StatusContent.Reset();
    BackgroundBlur.Reset();
}

void UDivisionStatusWidget::SetStatus(const FDivisionUIStatus& InStatus)
{
    if (InStatus.Health < Status.Health)
    {
        DamageTrail = FMath::Max(DamageTrail, DisplayHealth);
        DamageHold = 0.18f;
        DamageFlash = 1.0f;
    }
    if (InStatus.Magazine != Status.Magazine) AmmoFlash = 1.0f;
    if (InStatus.Grenades != Status.Grenades) ConsumableFlash[0] = 1.0f;
    if (InStatus.Medkits != Status.Medkits) ConsumableFlash[1] = 1.0f;
    if (Status.PulseRemaining > 0 && InStatus.PulseRemaining <= 0) ReadyFlash[0] = 1.0f;
    if (Status.TurretRemaining > 0 && InStatus.TurretRemaining <= 0) ReadyFlash[1] = 1.0f;
    Status = InStatus;
    if (StatusContent) StatusContent->Invalidate(EInvalidateWidgetReason::Paint);
    InvalidateLayoutAndVolatility();
}

void UDivisionStatusWidget::UpdatePlacement(APlayerController* PC, float DeltaSeconds)
{
    const float HealthAlpha = 1.0f - FMath::Exp(-22.0f * DeltaSeconds);
    DisplayHealth = FMath::Lerp(DisplayHealth, FMath::Clamp(Status.Health, 0.0f, 1.0f), HealthAlpha);
    DamageHold = FMath::Max(0.0f, DamageHold - DeltaSeconds);
    if (DamageHold <= 0) DamageTrail = FMath::Lerp(DamageTrail, DisplayHealth,
        1.0f - FMath::Exp(-6.0f * DeltaSeconds));
    AmmoFlash = FMath::Max(0.0f, AmmoFlash - DeltaSeconds / 0.16f);
    for (float& Flash : ConsumableFlash)
        Flash = FMath::Max(0.0f, Flash - DeltaSeconds / 0.16f);
    DamageFlash = FMath::Max(0.0f, DamageFlash - DeltaSeconds / 0.28f);
    for (float& Flash : ReadyFlash) Flash = FMath::Max(0.0f, Flash - DeltaSeconds / 0.45f);
    Reveal = FMath::Min(1.0f, Reveal + DeltaSeconds / 0.18f);
    SetRenderOpacity(Reveal * MenuVisibility);
    SetRenderScale(FVector2D(1.0f, FMath::Max(MenuVisibility, 0.001f)));
    if (BackgroundBlur) BackgroundBlur->SetBlurStrength(FMath::Max(0.0f, BlurStrength));
    if (StatusContent) StatusContent->Invalidate(EInvalidateWidgetReason::Paint);
    InvalidateLayoutAndVolatility();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    FVector2D Projected;
    if (!Pawn || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        PC, Pawn->GetActorLocation(), Projected, true))
    {
        SetVisibility(ESlateVisibility::Hidden);
        ResetFollow();
        return;
    }

    // The pawn origin is a stable capsule-center reference, not an animated hand socket.
    const FVector2D Target = Projected + ScreenOffset;
    if (!bHasPosition || LastPawn.Get() != Pawn)
    {
        DisplayPosition = Target;
        bHasPosition = true;
    }
    else
    {
        const float FollowAlpha = 1.0f - FMath::Exp(-FMath::Max(FollowSpeed, 0.0f) * DeltaSeconds);
        DisplayPosition = FMath::Lerp(DisplayPosition, Target, FollowAlpha);
    }
    LastPawn = Pawn;
    SetPositionInViewport(DisplayPosition, false); // Projector already removed DPI scaling.
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    SetRenderTransformAngle(PlaneAngle);
    SetRenderShear(FVector2D::ZeroVector);
    SetVisibility(MenuVisibility > 0.0f ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Hidden);
}

int32 UDivisionStatusWidget::PaintStatus(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    using namespace DivisionPaint;
    const int32 Base = LayerId;
    const float ContentOpacity = Style.GetColorAndOpacityTint().A;
    const FDraw Plates{Geometry, Elements, Base, ContentOpacity, BordaNumericFont.Get(),
        INDEX_NONE, Reveal * MenuVisibility};
    Plates.StatusCorners({0, 0}, {238, 108});
    Plates.StatusCell({0, 0}, {50, 64});
    for (int32 Index = 0; Index < 3; ++Index)
        Plates.StatusCell({58.0f + Index * 61.0f, 0}, {58, 18});
    Plates.StatusCell({58, 29}, {38, 40});
    Plates.StatusCell({200, 29}, {38, 40});
    Plates.StatusCell({58, 76}, {72, 32});
    Plates.StatusCell({166, 76}, {72, 32});

    // Both rows share X=25, even when their digit counts or fitted sizes differ.
    const FDraw D{Geometry, Elements, Base + 3, ContentOpacity,
        BordaNumericFont.Get(), Base + 2};
    D.Number({2, -4}, 46, FString::Printf(TEXT("%02d"), Status.Magazine), 28,
        FMath::Lerp(White, Orange, AmmoFlash), 0.5f);
    D.Number({2, 32}, 46, FString::FromInt(Status.Reserve), 19, White, 0.5f);

    // Project only current health; the damage trail and width markers stay single-layer.
    const FDraw HealthProjectionDraw{Geometry, Elements, Base + 2, ContentOpacity,
        BordaNumericFont.Get()};
    FLinearColor HealthProjectionColor = FMath::Lerp(Orange, White, 0.45f);
    HealthProjectionColor.A = 0.14f;
    const FDraw HealthDraw{Geometry, Elements, Base + 3, ContentOpacity, BordaNumericFont.Get()};
    const float Segments = DisplayHealth * 3.0f;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FVector2D P(58 + Index * 61, 0);
        const float Trail = FMath::Clamp(DamageTrail * 3.0f - Index, 0.0f, 1.0f);
        if (Trail > 0)
        {
            HealthDraw.Box(P, {58 * Trail, 18},
                FLinearColor::FromSRGBColor(FColor(246, 42, 28, 160)));
        }
        const float Fill = FMath::Clamp(Segments - Index, 0.0f, 1.0f);
        if (Fill > 0.0f)
        {
            HealthProjectionDraw.Box(P + StatusProjectionOffset, {58 * Fill, 18},
                HealthProjectionColor);
            HealthDraw.Box(P, {58 * Fill, 18}, FMath::Lerp(Orange, White, DamageFlash * 0.45f));
        }
    }
    // Fixed markers bracket the full three-segment capacity, even when health is empty.
    HealthDraw.Box({56, -1}, {2, 20}, White);
    HealthDraw.Box({238, -1}, {2, 20}, White);

    // During cooldown the number replaces the glyph, rather than overlapping it.
    if (Status.PulseRemaining <= 0) D.Pulse({61, 32});
    if (Status.TurretRemaining <= 0) D.Turret({203, 33});
    // Cooldowns belong to skill cells, never to the consumables row.
    const float Cooldowns[] = {Status.PulseRemaining, Status.TurretRemaining};
    const float Durations[] = {8.0f, 12.0f};
    const float Positions[] = {58.0f, 200.0f};
    // Fill stays inside the plate and below the number's projection.
    const FDraw CooldownFillDraw{Geometry, Elements, Base + 1, ContentOpacity,
        BordaNumericFont.Get()};
    const FDraw CooldownDraw{Geometry, Elements, Base + 6, ContentOpacity,
        BordaNumericFont.Get(), Base + 2};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (ReadyFlash[Index] > 0)
        {
            CooldownDraw.Box({Positions[Index], 29}, {38, 2},
                FLinearColor(Orange.R, Orange.G, Orange.B, ReadyFlash[Index]));
            CooldownDraw.Box({Positions[Index], 65}, {38, 2},
                FLinearColor(Orange.R, Orange.G, Orange.B, ReadyFlash[Index]));
        }
        if (Cooldowns[Index] > 0.0f)
        {
            const float Fraction = FMath::Clamp(Cooldowns[Index] / Durations[Index], 0.0f, 1.0f);
            constexpr float InnerTop = 30.0f;
            constexpr float InnerHeight = 38.0f;
            constexpr float InnerBottom = InnerTop + InnerHeight;
            constexpr float InnerWidth = 36.0f;
            const float InnerX = Positions[Index] + 1.0f;
            const float FillHeight = InnerHeight * (1.0f - Fraction);
            if (FillHeight > 0.0f)
            {
                CooldownFillDraw.Box({InnerX, InnerBottom - FillHeight},
                    {InnerWidth, FillHeight}, FLinearColor(Orange.R, Orange.G, Orange.B, 0.22f));
            }

            const int32 Seconds = FMath::CeilToInt(Cooldowns[Index]);
            CooldownDraw.NumberCentered({Positions[Index], 29}, {38, 36},
                FString::FromInt(Seconds), 22);
        }
    }

    // Reference layout: count/key/grenade, then medical cross/key/count.
    // Paired slot centers are mirrored about the vertical line X=148.
    const float ConsumableBaseline = 79.0f
        + D.GetBaselineOffset(TEXT("3"), 18, HudNumericTypeface);
    const float ConsumableIconTop = ConsumableBaseline - 16.0f;
    constexpr float ConsumableAxisX = 148.0f;
    constexpr float ConsumableNumberWidth = 18.0f;
    constexpr float LeftNumberX = 58.0f;
    constexpr float LeftKeyCenterX = 88.0f;
    constexpr float LeftIconX = 108.0f;
    const float RightNumberX = ConsumableAxisX * 2.0f
        - LeftNumberX - ConsumableNumberWidth;
    const float RightKeyCenterX = ConsumableAxisX * 2.0f - LeftKeyCenterX;
    const float RightIconX = ConsumableAxisX * 2.0f - LeftIconX - 20.0f;

    D.NumberAtBaseline(LeftNumberX, ConsumableNumberWidth,
        FString::FromInt(Status.Grenades), 18,
        ConsumableBaseline, FMath::Lerp(White, Orange, ConsumableFlash[0]), 0.5f);
    D.TextCenteredAtBaseline(LeftKeyCenterX, ConsumableBaseline, TEXT("G"), 18, Orange,
        HudNumericTypeface);
    D.Grenade({LeftIconX, ConsumableIconTop});
    D.Cross({RightIconX, ConsumableIconTop - 3.0f});
    D.TextCenteredAtBaseline(RightKeyCenterX, ConsumableBaseline, TEXT("V"), 18, Orange,
        HudNumericTypeface);
    D.NumberAtBaseline(RightNumberX, ConsumableNumberWidth,
        FString::FromInt(Status.Medkits), 18,
        ConsumableBaseline, FMath::Lerp(White, Orange, ConsumableFlash[1]), 0.5f);
    return Base + 9;
}

TSharedRef<SWidget> UDivisionScreenWidget::RebuildWidget()
{
    return SNew(SSpacer);
}

void UDivisionScreenWidget::UpdateNavigation(APlayerController* PC, bool bHelp,
    float DeltaSeconds)
{
    bShowHelp = bHelp;
    ExperiencePreviewTime = FMath::Fmod(
        ExperiencePreviewTime + FMath::Max(DeltaSeconds, 0.0f), 2.4f);
    if (PC)
    {
        Heading = FMath::DegreesToRadians(PC->PlayerCameraManager
            ? PC->PlayerCameraManager->GetCameraRotation().Yaw : PC->GetControlRotation().Yaw);
    }
    InvalidateLayoutAndVolatility();
}

int32 UDivisionScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    using namespace DivisionPaint;
    const int32 Base = Super::NativePaint(Args, Geometry, CullingRect, Elements,
        LayerId, Style, bParentEnabled);
    const FDraw D{Geometry, Elements, Base + 1, Style.GetColorAndOpacityTint().A, BordaNumericFont.Get()};
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D Center(112, 113);
    D.Ring(Center, 78, Dim);
    D.Lines({Center + FVector2D(-4, 4), Center + FVector2D(0, -5),
        Center + FVector2D(4, 4)}, White, 1.5f);
    // World +X is north in this self-contained demonstration map.
    const FVector2D North(-FMath::Sin(Heading), -FMath::Cos(Heading));
    D.Text(Center + North * 87 - FVector2D(5, 9), TEXT("N"), 12, White,
        HudNumericTypeface);
    D.Text({33, 24}, TEXT("1 : 100"), 9, Dim, HudNumericTypeface);
    const FVector2D ExperiencePanelPosition(Size.X - 99, 34);
    const FVector2D ExperiencePanelSize(43, 43);
    D.Box(ExperiencePanelPosition, ExperiencePanelSize,
        FLinearColor(Orange.R, Orange.G, Orange.B, 0.25f), true);
    // Preview-only partial XP fill: a solid white bar grows left-to-right above the orange track.
    const float ExperiencePhase = ExperiencePreviewTime / 2.4f;
    const float ExperienceFill = FMath::Lerp(0.18f, 0.68f,
        0.5f - 0.5f * FMath::Cos(ExperiencePhase * 2.0f * PI));
    const FVector2D ExperienceTextPosition(Size.X - 95, 37);
    const float ExperienceLineY = ExperienceTextPosition.Y
        + D.GetBaselineOffset(TEXT("09"), 25, HudNumericTypeface) - 1.0f;
    const FVector2D ExperienceLinePosition(Size.X - 420, ExperienceLineY);
    constexpr float ExperienceLineWidth = 290.0f;
    D.Box(ExperienceLinePosition, {ExperienceLineWidth, 2.0f},
        FLinearColor(Orange.R, Orange.G, Orange.B, 0.3f));
    D.Box(ExperienceLinePosition - FVector2D(0.0f, 6.0f),
        {ExperienceLineWidth * ExperienceFill, 7.0f}, White);
    const FDraw ExperienceProjection{Geometry, Elements, Base + 2, Style.GetColorAndOpacityTint().A,
        BordaNumericFont.Get()};
    D.Text(ExperienceTextPosition, TEXT("09"), 25, White, HudNumericTypeface);
    ExperienceProjection.ProjectionText(ExperienceTextPosition, TEXT("09"), 25,
        StatusProjectionTint, HudNumericTypeface);

    if (bShowHelp)
    {
        D.Box({24, Size.Y - 73}, {800, 48}, Back, true);
        D.Text({35, Size.Y - 70}, TEXT("UI 独立测试关卡 · 模拟数据"), 12, Orange);
        D.Text({35, Size.Y - 49}, TEXT("H 受伤  J 恢复  F 开火  R 换弹  1/2 技能  G 手雷  V 治疗  I 背包  Esc 返回  F1 隐藏说明"), 12);
    }
    return Base + 4;
}

// Route menu keys before PIE's Stop shortcut, only while this game's UI has focus.
class FDivisionMenuInputProcessor : public IInputProcessor
{
public:
    explicit FDivisionMenuInputProcessor(ADivisionDemoHUD* InHUD) : HUD(InHUD) {}
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        ADivisionDemoHUD* Owner = HUD.Get();
        if (!Owner || (Event.GetKey() != EKeys::Escape && Event.GetKey() != EKeys::I))
            return false;
        UGameViewportClient* ViewportClient = Owner->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Viewport = ViewportClient
            ? ViewportClient->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWidget> Menu = Owner->GetActiveMenu()
            ? Owner->GetActiveMenu()->GetCachedWidget() : nullptr;
        const bool bGameFocused = Viewport && Viewport->HasAnyUserFocusOrFocusedDescendants();
        const bool bMenuFocused = Menu && Menu->HasAnyUserFocusOrFocusedDescendants();
        if (!bGameFocused && !bMenuFocused)
            return false;
        if (!Event.IsRepeat())
        {
            if (Event.GetKey() == EKeys::I) Owner->ToggleInventory();
            else Owner->HandleMenuBack();
        }
        return true;
    }
private:
    TWeakObjectPtr<ADivisionDemoHUD> HUD;
};

UUserWidget* ADivisionDemoHUD::GetActiveMenu() const
{
    return InventoryWidget.Get();
}

void ADivisionDemoHUD::ToggleInventory()
{
    if (bMenuOpen) bMenuOpen = false;
    else OpenMenu();
}

void ADivisionDemoHUD::HandleMenuBack()
{
    if (!bMenuInputLocked) return;
    if (bMenuOpen && InventoryWidget && InventoryWidget->Back()) return;
    bMenuOpen = false;
}

void ADivisionDemoHUD::OpenMenu()
{
    APlayerController* PC = GetOwningPlayerController();
    if (!InventoryWidget || !PC) return;
    bMenuOpen = true;
    if (!bMenuInputLocked)
    {
        bPreviousMouseCursor = PC->bShowMouseCursor;
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        bMenuInputLocked = true;
        PC->bShowMouseCursor = true;
    }
    InventoryWidget->SetVisibility(ESlateVisibility::Visible);
    InventoryWidget->OpenOverview();
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(GetActiveMenu()->TakeWidget());
    PC->SetInputMode(InputMode);
}

void ADivisionDemoHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!InventoryWidget) return;
    MenuProgress = FMath::Clamp(MenuProgress + (bMenuOpen ? 1.0f : -1.0f)
        * DeltaSeconds / 0.4f, 0.0f, 1.0f);
    const auto Ease = [](float Value) { return Value * Value * (3.0f - 2.0f * Value); };
    const float Transition = Ease(MenuProgress);
    // The status rectangle folds vertically first; the large panel then unfolds.
    StatusWidget->SetMenuVisibility(1.0f - Ease(FMath::Clamp(MenuProgress / 0.5f, 0.0f, 1.0f)));
    ScreenWidget->SetRenderOpacity(1.0f - Transition);
    const float PageTransition = Ease(FMath::Clamp((MenuProgress - 0.2f) / 0.8f, 0.0f, 1.0f));
    InventoryWidget->SetTransition(PageTransition);
    if (MenuCamera) MenuCamera->PresentationAmount = Transition;
    if (!bMenuOpen && MenuProgress <= 0.0f && bMenuInputLocked)
    {
        if (APlayerController* PC = GetOwningPlayerController())
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
            PC->bShowMouseCursor = bPreviousMouseCursor;
            PC->SetInputMode(FInputModeGameOnly());
        }
        bMenuInputLocked = false;
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ADivisionDemoHUD::BeginPlay()
{
    Super::BeginPlay();
    APlayerController* PC = GetOwningPlayerController();
    if (!PC || !PC->IsLocalController())
    {
        return;
    }
    StatusWidget = CreateWidget<UDivisionStatusWidget>(PC);
    ScreenWidget = CreateWidget<UDivisionScreenWidget>(PC);
    StatusWidget->AddToPlayerScreen(10);
    StatusWidget->SetDesiredSizeInViewport({254, 124});
    StatusWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
    StatusWidget->SetVisibility(ESlateVisibility::Hidden);
    ScreenWidget->AddToPlayerScreen(5);
    ScreenWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    StatusWidget->SetStatus(Status);
    InventoryWidget = CreateWidget<UDivisionInventoryWidget>(PC);
    InventoryWidget->AddToPlayerScreen(30);
    InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (PC->PlayerCameraManager)
        MenuCamera = Cast<UDivisionMenuCameraModifier>(PC->PlayerCameraManager->AddNewCameraModifier(
            UDivisionMenuCameraModifier::StaticClass()));
    MenuInput = MakeShared<FDivisionMenuInputProcessor>(this);
    FSlateApplication::Get().RegisterInputPreProcessor(MenuInput, 0);
    SetActorTickEnabled(true);
}

void ADivisionDemoHUD::UpdateDemo(APlayerController* PC, float DeltaSeconds)
{
    bool bChanged = Status.PulseRemaining > 0 || Status.TurretRemaining > 0;
    Status.PulseRemaining = FMath::Max(0.0f, Status.PulseRemaining - DeltaSeconds);
    Status.TurretRemaining = FMath::Max(0.0f, Status.TurretRemaining - DeltaSeconds);
    if (bMenuInputLocked)
    {
        if (bChanged) StatusWidget->SetStatus(Status);
        return;
    }
    if (PC->WasInputKeyJustPressed(EKeys::H))
    {
        Status.Health = FMath::Max(0.0f, Status.Health - 0.2f);
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::J))
    {
        Status.Health = 1.0f;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::F) && Status.Magazine > 0)
    {
        --Status.Magazine;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::R))
    {
        const int32 Transfer = FMath::Min(FMath::Max(30 - Status.Magazine, 0), Status.Reserve);
        Status.Magazine += Transfer;
        Status.Reserve -= Transfer;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::One) && Status.PulseRemaining <= 0)
    {
        Status.PulseRemaining = 8.0f;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::Two) && Status.TurretRemaining <= 0)
    {
        Status.TurretRemaining = 12.0f;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::G) && Status.Grenades > 0)
    {
        --Status.Grenades;
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::V) && Status.Medkits > 0 && Status.Health < 1)
    {
        --Status.Medkits;
        Status.Health = FMath::Min(1.0f, Status.Health + 0.5f);
        bChanged = true;
    }
    if (PC->WasInputKeyJustPressed(EKeys::F1))
    {
        bHelp = !bHelp;
    }
    if (bChanged)
    {
        StatusWidget->SetStatus(Status);
    }
}

void ADivisionDemoHUD::PostRender()
{
    Super::PostRender();
    APlayerController* PC = GetOwningPlayerController();
    if (!StatusWidget || !PC)
    {
        return;
    }
    if (!bShowHUD)
    {
        StatusWidget->SetVisibility(ESlateVisibility::Hidden);
        ScreenWidget->SetVisibility(ESlateVisibility::Hidden);
        StatusWidget->ResetFollow();
        return;
    }
    const float Dt = GetWorld()->GetDeltaSeconds();
    if (bDemoControls)
    {
        UpdateDemo(PC, Dt);
    }
    StatusWidget->UpdatePlacement(PC, Dt);
    ScreenWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    ScreenWidget->UpdateNavigation(PC, bHelp, Dt);
}

void ADivisionDemoHUD::PushStatus(const FDivisionUIStatus& InStatus)
{
    Status = InStatus;
    if (StatusWidget)
    {
        StatusWidget->SetStatus(Status);
    }
}

void ADivisionDemoHUD::ResetStatusFollow()
{
    if (StatusWidget)
    {
        StatusWidget->ResetFollow();
    }
}

void ADivisionDemoHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MenuInput && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(MenuInput);
    MenuInput.Reset();
    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (bMenuInputLocked)
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
            PC->bShowMouseCursor = bPreviousMouseCursor;
            PC->SetInputMode(FInputModeGameOnly());
        }
        if (MenuCamera && PC->PlayerCameraManager)
            PC->PlayerCameraManager->RemoveCameraModifier(MenuCamera);
    }
    if (InventoryWidget) InventoryWidget->RemoveFromParent();
    InventoryWidget = nullptr;
    MenuCamera = nullptr;
    if (StatusWidget)
    {
        StatusWidget->RemoveFromParent();
        StatusWidget = nullptr;
    }
    if (ScreenWidget)
    {
        ScreenWidget->RemoveFromParent();
        ScreenWidget = nullptr;
    }
    Super::EndPlay(Reason);
}

ADivisionUIDemoGameMode::ADivisionUIDemoGameMode()
{
    // Preserve the project's existing pawn and controller without editing their assets.
    static ConstructorHelpers::FClassFinder<AGameModeBase> ExistingMode(
        TEXT("/Game/Code/Character/BP_ChallengeMode"));
    if (ExistingMode.Succeeded())
    {
        const AGameModeBase* Defaults = ExistingMode.Class->GetDefaultObject<AGameModeBase>();
        DefaultPawnClass = Defaults->DefaultPawnClass;
        PlayerControllerClass = Defaults->PlayerControllerClass;
    }
    HUDClass = ADivisionDemoHUD::StaticClass();
}
