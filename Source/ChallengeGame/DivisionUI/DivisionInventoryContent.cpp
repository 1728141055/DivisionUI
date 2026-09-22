#include "DivisionInventoryContent.h"
#include "DivisionInventoryIcons.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace DivisionInventoryContentPrivate
{
    constexpr int32 SlotCount = static_cast<int32>(EDivisionInventorySlot::Count);
    constexpr int32 VisibleRows = 5;
    constexpr int32 RowTarget = 100;
    constexpr int32 EquipTarget = 200;
    constexpr int32 BackTarget = 201;
    constexpr int32 OverviewTarget = 202;
    constexpr float ListTop = 290.0f;
    constexpr float RowPitch = 85.0f;
    constexpr float AnimationSeconds = 0.15f;
    constexpr float HoverInSpeed = 18.0f;
    constexpr float HoverOutSpeed = 14.0f;
    constexpr float ScrollSpeed = 18.0f;
    const FLinearColor Orange(1.0f, 0.30f, 0.035f, 1.0f);
    const FLinearColor White(0.94f, 0.95f, 0.94f, 1.0f);
    const FLinearColor Muted(0.84f, 0.87f, 0.86f, 1.0f);
    const FLinearColor Green(0.35f, 0.79f, 0.39f, 1.0f);
    const FLinearColor Red(0.95f, 0.32f, 0.22f, 1.0f);
    const TCHAR* const SlotNames[] = {
        TEXT("Primary weapon"), TEXT("Secondary weapon"), TEXT("Sidearm"),
        TEXT("Vest"), TEXT("Mask"), TEXT("Backpack"), TEXT("Kneepads"), TEXT("Gloves"), TEXT("Holster")
    };
    const TCHAR* const SlotIcons[] = {
        TEXT("Rifle"), TEXT("SMG"), TEXT("Pistol"), TEXT("Vest"), TEXT("Mask"),
        TEXT("Backpack"), TEXT("Kneepads"), TEXT("Gloves"), TEXT("Holster")
    };

    bool IsWeapon(EDivisionInventorySlot Slot)
    {
        return static_cast<int32>(Slot) <= static_cast<int32>(EDivisionInventorySlot::Sidearm);
    }

    FLinearColor QualityColor(EDivisionInventoryQuality Quality)
    {
        switch (Quality)
        {
        case EDivisionInventoryQuality::Standard: return Green;
        case EDivisionInventoryQuality::Specialized: return FLinearColor(0.23f, 0.58f, 0.94f, 1);
        case EDivisionInventoryQuality::Superior: return FLinearColor(0.70f, 0.36f, 0.88f, 1);
        default: return FLinearColor(0.96f, 0.72f, 0.22f, 1);
        }
    }

    FString QualityName(EDivisionInventoryQuality Quality)
    {
        switch (Quality)
        {
        case EDivisionInventoryQuality::Standard: return TEXT("Standard");
        case EDivisionInventoryQuality::Specialized: return TEXT("Specialized");
        case EDivisionInventoryQuality::Superior: return TEXT("Superior");
        default: return TEXT("High-end");
        }
    }

    FString Number(int32 Value)
    {
        // Fixed English grouping, independent of the editor/game's current locale.
        FString Result = FString::FromInt(FMath::Abs(Value));
        for (int32 Index = Result.Len() - 3; Index > 0; Index -= 3)
            Result.InsertAt(Index, TEXT(','));
        return Value < 0 ? TEXT("-") + Result : Result;
    }

    FSlateRect SlotRect(int32 Index)
    {
        if (Index < 3)
        {
            const float Top = 275.0f + Index * 149.0f;
            return FSlateRect(0, Top, 350, Top + 142);
        }
        const int32 GearIndex = Index - 3;
        const float Left = GearIndex < 3 ? 390.0f : 801.0f;
        const float Top = 275.0f + (GearIndex % 3) * 149.0f;
        return FSlateRect(Left, Top, Left + 399, Top + 142);
    }

    FSlateRect ListRowRect(int32 CandidateIndex, float ScrollOffset = 0.0f)
    {
        const float Top = ListTop + CandidateIndex * RowPitch - ScrollOffset;
        return FSlateRect(0, Top, 730, Top + 79);
    }

    const FSlateRect ListViewport(0, ListTop, 730, ListTop + VisibleRows * RowPitch - 6);

    FVector2D HoverOffset(float Amount)
    {
        return FVector2D(2.0f * Amount, -3.0f * Amount);
    }

    const FSlateRect EquipRect(760, 790, 1016, 836);
    const FSlateRect BackRect(0, 790, 140, 836);
    const FSlateRect OverviewRect(350, 15, 558, 61);

    bool Contains(const FSlateRect& Rect, FVector2D Point)
    {
        return Point.X >= Rect.Left && Point.X < Rect.Right && Point.Y >= Rect.Top && Point.Y < Rect.Bottom;
    }

    bool ContainsFloating(const FSlateRect& Rect, FVector2D Point, float Amount)
    {
        // Include the rendered edge, while keeping the original footprint stable so
        // a stationary pointer at the lower edge cannot repeatedly enter/leave as it lifts.
        return Contains(Rect, Point) || Contains(Rect, Point - HoverOffset(Amount));
    }

    FVector2D PageOffset(float Progress)
    {
        return FVector2D(8.0f * (1.0f - FMath::SmoothStep(0.0f, 1.0f, Progress)), 0);
    }

    // The animated body translation is also removed by HitTest below.
    struct FPainter
    {
        const FGeometry& Geometry;
        FSlateWindowElementList& Elements;
        int32 Layer;
        const FSlateFontInfo& Regular;
        const FSlateFontInfo& Medium;
        const FSlateFontInfo& Numeric;
        FLinearColor StyleTint;
        float Opacity = 1.0f;
        FVector2D Translation = FVector2D::ZeroVector;

        FLinearColor Tint(FLinearColor Color) const
        {
            Color *= StyleTint;
            Color.A *= Opacity;
            return Color;
        }

        void Box(float X, float Y, float W, float H, FLinearColor Color,
            int32 Offset = 0, bool bAntialiasEdge = true) const
        {
            const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry(FVector2D(W, H),
                FSlateLayoutTransform(FVector2D(X, Y) + Translation));
            const FLinearColor BoxTint = Tint(Color);
            FSlateDrawElement::MakeBox(Elements, Layer + Offset, PaintGeometry,
                FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, BoxTint);
            if (bAntialiasEdge && W >= 4.0f && H >= 4.0f)
            {
                // Filled Slate boxes have no AA switch. Keep their interior
                // opaque and add a low-alpha antialiased perimeter for the
                // projected inventory plane.
                FLinearColor EdgeTint = BoxTint;
                EdgeTint.A *= 0.55f;
                FSlateDrawElement::MakeGeometryOutline(Elements, Layer + Offset,
                    PaintGeometry, EdgeTint, ESlateDrawEffect::NoPixelSnapping,
                    true, 1.0f);
            }
        }

        void Card(const FSlateRect& Rect, bool bSelected = false, bool bHovered = false,
            float NeutralAlpha = 0.24f, float AnimatedEmphasis = -1.0f) const
        {
            const float Width = Rect.Right - Rect.Left;
            const float Height = Rect.Bottom - Rect.Top;
            const float Amount = AnimatedEmphasis >= 0 ? AnimatedEmphasis : (bSelected || bHovered ? 1.f : 0.f);
            if (Amount > 0)
                Box(Rect.Left + 1, Rect.Top + 5, Width, Height, FLinearColor(0, 0, 0, .07f * Amount));
            Box(Rect.Left, Rect.Top, Width, Height, FMath::Lerp(
                FLinearColor(0.17f, 0.19f, 0.20f, NeutralAlpha), FLinearColor(0.27f, 0.22f, 0.15f, 0.36f), Amount),
                0, true);
            FLinearColor Edge = Orange;
            Edge.A *= Amount;
            if (Amount > 0) Box(Rect.Left, Rect.Top, 2, Height * Amount, Edge, 1);
            const FLinearColor Corner = FMath::Lerp(FLinearColor(0.67f, 0.71f, 0.72f, 0.62f), Orange, Amount);
            Box(Rect.Left, Rect.Top, 3, 3, Corner, 1);
            Box(Rect.Right - 3, Rect.Bottom - 3, 3, 3, Corner, 1);
        }

        void RarityTile(const FSlateRect& Rect, FLinearColor Quality, float Amount) const
        {
            // The quality block is the background itself, not an overlay on a gray card.
            if (Amount > 0)
                Box(Rect.Left + 1, Rect.Top + 5, Rect.Right - Rect.Left, Rect.Bottom - Rect.Top,
                    FLinearColor(0, 0, 0, .07f * Amount));
            Quality.A = FMath::Lerp(.42f, .56f, Amount);
            Box(Rect.Left, Rect.Top, Rect.Right - Rect.Left, Rect.Bottom - Rect.Top,
                Quality, 0, true);
            const FLinearColor Accent = FMath::Lerp(White, Orange, Amount);
            FLinearColor Edge = Orange;
            Edge.A *= Amount;
            if (Amount > 0) Box(Rect.Left, Rect.Top, (Rect.Right - Rect.Left) * Amount, 2, Edge, 1);
            Box(Rect.Left, Rect.Top, 3, 3, Accent, 1);
            Box(Rect.Right - 3, Rect.Bottom - 3, 3, 3, Accent, 1);
        }

        void Text(const FString& Value, float X, float Y, float Width, int32 Size,
            FLinearColor Color = White, int32 Face = 0, bool bRight = false) const
        {
            X += Translation.X;
            Y += Translation.Y;
            // Small labels need the host's Borda Medium; numeric text keeps DemiBold.
            FSlateFontInfo Font = Face == 2 ? Numeric : (Face == 1 || Size <= 22) ? Medium : Regular;
            Font.Size = static_cast<float>(Size);
            const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FVector2D Extent = Measure->Measure(Value, Font);
            if (Extent.X > Width && Width > 0)
            {
                // Preserve a readable floor for auxiliary text within its existing clip.
                const float MinimumSize = Size <= 22 ? 20.0f : 12.0f;
                Font.Size = FMath::Max(MinimumSize, Font.Size * Width / static_cast<float>(Extent.X));
                Extent = Measure->Measure(Value, Font);
            }
            const float TextX = bRight ? X + FMath::Max(0.0f, Width - static_cast<float>(Extent.X)) : X;
            Elements.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(
                FVector2D(Width, FMath::Max(64.0, Extent.Y + 4.0)), FSlateLayoutTransform(FVector2D(X, Y)))));
            FSlateDrawElement::MakeText(Elements, Layer + 3,
                Geometry.ToPaintGeometry(FVector2D(Width, Extent.Y + 4.0), FSlateLayoutTransform(FVector2D(TextX, Y))),
                Value, Font, ESlateDrawEffect::None, Tint(Color));
            Elements.PopClip();
        }

        void Icon(FName Name, float X, float Y, float W, float H, FLinearColor Color) const
        {
            DivisionInventoryIcons::Draw(Name, Geometry, Elements, Layer + 2,
                FVector2D(X, Y) + Translation, FVector2D(W, H), Tint(Color));
        }

        void Bar(const FString& Label, const FString& Value, float Y, float Fraction) const
        {
            Text(Label, 780, Y, 140, 20, Muted);
            Text(Value, 1005, Y, 171, 20, White, 2, true);
            Box(924, Y + 13, 72, 5, FLinearColor(0.6f, 0.64f, 0.64f, 0.24f), 1);
            Box(924, Y + 13, 72 * FMath::Clamp(Fraction, 0.0f, 1.0f), 5, White, 2);
        }
    };
}

void SDivisionInventoryContent::Construct(const FArguments& InArgs)
{
    // Font assets are provided by the host; no synchronous asset loading in Slate paint.
    RegularFont = InArgs._RegularFont ? FSlateFontInfo(InArgs._RegularFont, 18, FName(TEXT("Default")))
        : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18);
    MediumFont = InArgs._MediumFont ? FSlateFontInfo(InArgs._MediumFont, 22, FName(TEXT("Default"))) : RegularFont;
    NumberFont = InArgs._NumberFont ? FSlateFontInfo(InArgs._NumberFont, 42, FName(TEXT("Default"))) : MediumFont;
    SetClipping(EWidgetClipping::ClipToBounds);
    BuildDemo();
    Changed();
}

FVector2D SDivisionInventoryContent::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(1200, 900);
}

void SDivisionInventoryContent::BuildDemo()
{
    using namespace DivisionInventoryContentPrivate;
    Items.Reset();
    EquippedIndices.Init(INDEX_NONE, SlotCount);
    const TCHAR* const WeaponNames[] = { TEXT("Military P416"), TEXT("Vector 45 ACP"), TEXT("Super 90"),
        TEXT("Classic M60"), TEXT("Police M4"), TEXT("MP5 ST") };
    const TCHAR* const WeaponCategories[] = { TEXT("Assault rifle"), TEXT("Submachine gun"), TEXT("Shotgun"),
        TEXT("Light machine gun"), TEXT("Assault rifle"), TEXT("Submachine gun") };
    const TCHAR* const WeaponIcons[] = { TEXT("Rifle"), TEXT("SMG"), TEXT("Shotgun"), TEXT("LMG"), TEXT("Rifle"), TEXT("SMG") };
    const TCHAR* const PistolNames[] = { TEXT("First Wave X-45"), TEXT("Military M9"), TEXT("M1911"),
        TEXT("Police 686 Magnum"), TEXT("PX4 Storm"), TEXT("Officer's M9 A1") };
    const int32 Damages[] = { 10840, 8920, 42800, 12960, 9850, 9370 };
    const int32 Rates[] = { 750, 900, 160, 500, 850, 800 };
    const int32 Magazines[] = { 30, 20, 8, 100, 30, 32 };
    const TCHAR* const GearPrefixes[] = { TEXT("Tactical"), TEXT("Specialist"), TEXT("Operator") };

    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        const auto Slot = static_cast<EDivisionInventorySlot>(SlotIndex);
        const bool bWeapon = IsWeapon(Slot);
        const int32 CandidateCount = bWeapon ? 6 : 3;
        const int32 FirstItem = Items.Num();
        for (int32 Variant = 0; Variant < CandidateCount; ++Variant)
        {
            FDivisionInventoryItem Item;
            Item.Id = FName(*FString::Printf(TEXT("DemoInventory_%s_%02d"), SlotIcons[SlotIndex], Variant));
            Item.Slot = Slot;
            Item.Quality = static_cast<EDivisionInventoryQuality>((SlotIndex + Variant + 3) % 4);
            if (bWeapon)
            {
                const bool bPistol = Slot == EDivisionInventorySlot::Sidearm;
                const int32 Profile = (Variant + (SlotIndex == 1 ? 1 : 0)) % 6;
                Item.Name = bPistol ? PistolNames[Variant] : WeaponNames[Profile];
                Item.Category = bPistol ? TEXT("Sidearm") : WeaponCategories[Profile];
                Item.Icon = FName(bPistol ? TEXT("Pistol") : WeaponIcons[Profile]);
                Item.Damage = bPistol ? 7200 + Variant * 810 : Damages[Profile] + SlotIndex * 135;
                Item.RPM = bPistol ? 450 - Variant * 25 : Rates[Profile];
                Item.Magazine = bPistol ? (Variant == 3 ? 6 : 15 + Variant) : Magazines[Profile];
                Item.Accuracy = 0.54f + ((Variant + SlotIndex) % 5) * 0.085f;
                Item.Stability = 0.47f + ((Variant * 2 + SlotIndex) % 6) * 0.08f;
                Item.ReloadSeconds = bPistol ? 1.7f + Variant * 0.12f : 2.1f + Profile * 0.29f;
                Item.RangeMeters = bPistol ? 16.0f + Variant : (Profile == 2 ? 12.0f : 25.0f + Profile * 3);
                Item.Talents.Add({ TEXT("Responsive"), TEXT("+10% damage within 10 m."), FName(TEXT("Target")) });
                Item.Talents.Add({ Variant % 2 == 0 ? TEXT("Stable") : TEXT("Prepared"),
                    Variant % 2 == 0 ? TEXT("+15% weapon stability.") : TEXT("+10% damage beyond 30 m."), FName(TEXT("Talent")) });
            }
            else
            {
                Item.Name = FString::Printf(TEXT("%s %s"), GearPrefixes[Variant], SlotNames[SlotIndex]);
                Item.Category = SlotNames[SlotIndex];
                Item.Icon = FName(SlotIcons[SlotIndex]);
                Item.Armor = 420 + (SlotIndex - 3) * 89 + Variant * 113;
                Item.Firearms = 90 + ((SlotIndex + Variant) % 3 == 0 ? 430 : Variant * 34);
                Item.Stamina = 85 + ((SlotIndex + Variant) % 3 == 1 ? 450 : Variant * 28);
                Item.Electronics = 80 + ((SlotIndex + Variant) % 3 == 2 ? 440 : Variant * 31);
                Item.Talents.Add({ TEXT("Reinforced"), TEXT("Armor included in item rating."), FName(TEXT("Shield")) });
                Item.Talents.Add({ TEXT("Specialized"), TEXT("Core bonuses in totals."), FName(TEXT("Biohazard")) });
            }
            Items.Add(MoveTemp(Item));
        }
        EquippedIndices[SlotIndex] = FirstItem;
    }
    Stats = CalculateStats();
}

FDivisionInventoryStats SDivisionInventoryContent::CalculateStats(const FDivisionInventoryItem* Replacement) const
{
    FDivisionInventoryStats Result;
    Result.Firearms = 1620;
    Result.Stamina = 1510;
    Result.Electronics = 1340;
    const FDivisionInventoryItem* Primary = nullptr;
    for (int32 SlotIndex = 0; SlotIndex < EquippedIndices.Num(); ++SlotIndex)
    {
        const auto Slot = static_cast<EDivisionInventorySlot>(SlotIndex);
        const FDivisionInventoryItem* Item = Replacement && Replacement->Slot == Slot ? Replacement : GetEquippedItem(Slot);
        if (!Item) continue;
        Result.Firearms += Item->Firearms;
        Result.Stamina += Item->Stamina;
        Result.Electronics += Item->Electronics;
        Result.Armor += Item->Armor;
        if (Slot == EDivisionInventorySlot::Primary) Primary = Item;
    }
    Result.Health = 12000 + Result.Stamina * 30;
    Result.SkillPower = Result.Electronics * 10;
    if (Primary)
        Result.PrimaryDPS = FMath::RoundToInt(Primary->Damage * (Primary->RPM / 60.0f) * (1.0f + Result.Firearms / 10000.0f));
    return Result;
}

const FDivisionInventoryItem* SDivisionInventoryContent::GetEquippedItem(EDivisionInventorySlot Slot) const
{
    const int32 SlotIndex = static_cast<int32>(Slot);
    return EquippedIndices.IsValidIndex(SlotIndex) && Items.IsValidIndex(EquippedIndices[SlotIndex])
        ? &Items[EquippedIndices[SlotIndex]] : nullptr;
}

const FDivisionInventoryItem* SDivisionInventoryContent::GetSelectedItem() const
{
    return bInList && Candidates.IsValidIndex(SelectedCandidate) ? &Items[Candidates[SelectedCandidate]] : nullptr;
}

void SDivisionInventoryContent::Changed()
{
    ++Revision;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SDivisionInventoryContent::StartPageAnimation()
{
    PageAnimation = 0.0f;
    HoverTarget = INDEX_NONE;
    bPointerActive = false;
    Emphasis.Reset();
    Changed();
}

bool SDivisionInventoryContent::Advance(float DeltaTime)
{
    using namespace DivisionInventoryContentPrivate;
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) return false;
    bool bAnimating = false;
    if (PageAnimation < 1.0f)
    {
        PageAnimation = FMath::Min(1.0f, PageAnimation + DeltaTime / AnimationSeconds);
        bAnimating = true;
    }
    const float TargetScroll = FirstVisible * RowPitch;
    if (ScrollOffset != TargetScroll)
    {
        ScrollOffset = FMath::Lerp(ScrollOffset, TargetScroll, 1.0f - FMath::Exp(-ScrollSpeed * DeltaTime));
        if (FMath::Abs(ScrollOffset - TargetScroll) < .05f) ScrollOffset = TargetScroll;
        bAnimating = true;
    }
    // Rows can move under a stationary cursor. Hover follows the displayed offset,
    // but never calls KeepSelectionVisible (which would fight wheel scrolling).
    if (bPointerActive) UpdatePointerFocus();
    const int32 FocusTarget = bInList ? RowTarget + SelectedCandidate : FocusedSlot;
    Emphasis.FindOrAdd(FocusTarget);
    if (HoverTarget != INDEX_NONE) Emphasis.FindOrAdd(HoverTarget);
    for (auto& Pair : Emphasis)
    {
        const float Target = Pair.Key == FocusTarget || Pair.Key == HoverTarget ? 1.0f : 0.0f;
        if (Pair.Value == Target) continue;
        const float Speed = Target > Pair.Value ? HoverInSpeed : HoverOutSpeed;
        Pair.Value = FMath::Lerp(Pair.Value, Target, 1.0f - FMath::Exp(-Speed * DeltaTime));
        if (FMath::Abs(Pair.Value - Target) < .001f) Pair.Value = Target;
        bAnimating = true;
    }
    if (bAnimating) Changed();
    return bAnimating;
}

float SDivisionInventoryContent::GetEmphasis(int32 Target) const
{
    const float* Value = Emphasis.Find(Target);
    return Value ? *Value : 0.0f;
}

bool SDivisionInventoryContent::OpenSlot(EDivisionInventorySlot Slot)
{
    using namespace DivisionInventoryContentPrivate;
    const int32 SlotIndex = static_cast<int32>(Slot);
    if (SlotIndex >= SlotCount) return false;
    Candidates.Reset();
    for (int32 Index = 0; Index < Items.Num(); ++Index)
        if (Items[Index].Slot == Slot) Candidates.Add(Index);
    if (Candidates.IsEmpty()) return false;
    ActiveSlot = Slot;
    FocusedSlot = SlotIndex;
    SelectedCandidate = FMath::Max(0, Candidates.IndexOfByKey(EquippedIndices[SlotIndex]));
    FirstVisible = 0;
    KeepSelectionVisible();
    ScrollOffset = FirstVisible * RowPitch;
    bInList = true;
    StartPageAnimation();
    return true;
}

bool SDivisionInventoryContent::Back()
{
    if (!bInList) return false;
    bInList = false;
    Candidates.Reset();
    FirstVisible = 0;
    ScrollOffset = 0;
    StartPageAnimation();
    return true;
}

void SDivisionInventoryContent::ResetPage()
{
    const bool bChanged = bInList || FocusedSlot != 0 || HoverTarget != INDEX_NONE || PageAnimation != 1.0f;
    bInList = false;
    Candidates.Reset();
    SelectedCandidate = FirstVisible = FocusedSlot = 0;
    ActiveSlot = EDivisionInventorySlot::Primary;
    HoverTarget = INDEX_NONE;
    bPointerActive = false;
    ScrollOffset = 0;
    Emphasis.Reset();
    PageAnimation = 1.0f;
    if (bChanged) Changed();
}

void SDivisionInventoryContent::KeepSelectionVisible()
{
    using namespace DivisionInventoryContentPrivate;
    if (SelectedCandidate < FirstVisible) FirstVisible = SelectedCandidate;
    if (SelectedCandidate >= FirstVisible + VisibleRows) FirstVisible = SelectedCandidate - VisibleRows + 1;
    FirstVisible = FMath::Clamp(FirstVisible, 0, FMath::Max(0, Candidates.Num() - VisibleRows));
}

bool SDivisionInventoryContent::SelectItem(FName ItemId)
{
    if (!bInList) return false;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        if (Items[Candidates[Index]].Id != ItemId) continue;
        const bool bSelectionChanged = SelectedCandidate != Index;
        const int32 PreviousFirstVisible = FirstVisible;
        SelectedCandidate = Index;
        // Home/End can target an already-selected item that wheel scrolling hid.
        // Pointer hover deliberately bypasses this method to avoid fighting the wheel.
        KeepSelectionVisible();
        if (bSelectionChanged)
        {
            HoverTarget = INDEX_NONE;
        }
        if (bSelectionChanged || FirstVisible != PreviousFirstVisible) Changed();
        return true;
    }
    return false;
}

bool SDivisionInventoryContent::MoveSelection(int32 Offset)
{
    if (!bInList || Candidates.IsEmpty()) return false;
    const int32 Next = FMath::Clamp(SelectedCandidate + Offset, 0, Candidates.Num() - 1);
    return SelectItem(Items[Candidates[Next]].Id);
}

bool SDivisionInventoryContent::EquipSelected()
{
    const FDivisionInventoryItem* Item = GetSelectedItem();
    if (!Item) return false;
    const int32 SlotIndex = static_cast<int32>(ActiveSlot);
    const int32 ItemIndex = Candidates[SelectedCandidate];
    if (EquippedIndices[SlotIndex] == ItemIndex) return false;
    EquippedIndices[SlotIndex] = ItemIndex;
    Stats = CalculateStats();
    Changed();
    // Snapshot callback arguments so an integration callback may safely change the page.
    const FName EquippedId = Item->Id;
    const EDivisionInventorySlot EquippedSlot = ActiveSlot;
    const FDivisionInventoryStats EquippedStats = Stats;
    FOnDemoEquipped Callback = OnDemoEquipped;
    if (Callback) Callback(EquippedSlot, EquippedId, EquippedStats);
    return true;
}

int32 SDivisionInventoryContent::HitTest(FVector2D Logical) const
{
    using namespace DivisionInventoryContentPrivate;
    if (!Contains(FSlateRect(0, 0, 1200, 900), Logical)) return INDEX_NONE;
    if (Logical.Y >= 250 && Logical.Y < 725) Logical -= PageOffset(PageAnimation);
    if (bInList)
    {
        if (ContainsFloating(BackRect, Logical, GetEmphasis(BackTarget))) return BackTarget;
        if (ContainsFloating(OverviewRect, Logical, GetEmphasis(OverviewTarget))) return OverviewTarget;
        const auto* Selected = GetSelectedItem();
        const auto* Equipped = GetEquippedItem(ActiveSlot);
        if (Selected && Equipped && Selected->Id != Equipped->Id &&
            ContainsFloating(EquipRect, Logical, GetEmphasis(EquipTarget))) return EquipTarget;
        if (Contains(ListViewport, Logical))
            for (int32 Index = 0; Index < Candidates.Num(); ++Index)
                if (ContainsFloating(ListRowRect(Index, ScrollOffset), Logical, GetEmphasis(RowTarget + Index)))
                    return RowTarget + Index;
    }
    else
    {
        for (int32 Index = 0; Index < SlotCount; ++Index)
            if (ContainsFloating(SlotRect(Index), Logical, GetEmphasis(Index))) return Index;
    }
    return INDEX_NONE;
}

bool SDivisionInventoryContent::PointerMove(FVector2D Logical)
{
    PointerPosition = Logical;
    bPointerActive = true;
    UpdatePointerFocus();
    return HoverTarget != INDEX_NONE;
}

void SDivisionInventoryContent::UpdatePointerFocus()
{
    using namespace DivisionInventoryContentPrivate;
    const int32 Target = HitTest(PointerPosition);
    bool bChanged = false;
    if (HoverTarget != Target)
    {
        HoverTarget = Target;
        bChanged = true;
    }
    if (!bInList && Target >= 0 && Target < SlotCount && FocusedSlot != Target)
    {
        FocusedSlot = Target;
        bChanged = true;
    }
    else if (bInList && Target >= RowTarget && Target < RowTarget + Candidates.Num())
    {
        const int32 Index = Target - RowTarget;
        if (SelectedCandidate != Index)
        {
            SelectedCandidate = Index;
            bChanged = true;
        }
    }
    if (bChanged) Changed();
}

void SDivisionInventoryContent::PointerLeave()
{
    bPointerActive = false;
    if (HoverTarget == INDEX_NONE) return;
    HoverTarget = INDEX_NONE;
    Changed();
}

bool SDivisionInventoryContent::PointerDown(FVector2D Logical)
{
    using namespace DivisionInventoryContentPrivate;
    PointerMove(Logical);
    const int32 Target = HitTest(Logical);
    if (Target == INDEX_NONE) return false;
    if (!bInList) return OpenSlot(static_cast<EDivisionInventorySlot>(Target));
    if (Target == BackTarget || Target == OverviewTarget) return Back();
    if (Target == EquipTarget) return EquipSelected();
    const int32 CandidateIndex = Target - RowTarget;
    return Candidates.IsValidIndex(CandidateIndex) && SelectItem(Items[Candidates[CandidateIndex]].Id);
}

bool SDivisionInventoryContent::Scroll(float Delta, FVector2D Logical)
{
    using namespace DivisionInventoryContentPrivate;
    if (!bInList || !Contains(ListViewport, Logical - PageOffset(PageAnimation)) ||
        !FMath::IsFinite(Delta) || FMath::IsNearlyZero(Delta)) return false;
    PointerMove(Logical);
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(FMath::Min(FMath::Abs(Delta), 5.0f)), 1, VisibleRows);
    const int32 Next = FMath::Clamp(FirstVisible + (Delta > 0 ? -Steps : Steps),
        0, FMath::Max(0, Candidates.Num() - VisibleRows));
    if (Next != FirstVisible)
    {
        FirstVisible = Next;
        Changed();
    }
    return true;
}

bool SDivisionInventoryContent::Key(FKey InKey)
{
    using namespace DivisionInventoryContentPrivate;
    // Keyboard navigation owns focus until the pointer actually moves again.
    bPointerActive = false;
    if (HoverTarget != INDEX_NONE)
    {
        HoverTarget = INDEX_NONE;
        Changed();
    }
    if (InKey == EKeys::Escape || InKey == EKeys::BackSpace) return Back();
    if (InKey == EKeys::Enter || InKey == EKeys::SpaceBar)
    {
        if (!bInList) return OpenSlot(static_cast<EDivisionInventorySlot>(FocusedSlot));
        EquipSelected();
        return true;
    }
    if (bInList)
    {
        if (InKey == EKeys::Up || InKey == EKeys::Left) return MoveSelection(-1);
        if (InKey == EKeys::Down || InKey == EKeys::Right) return MoveSelection(1);
        if (InKey == EKeys::PageUp) return MoveSelection(-VisibleRows);
        if (InKey == EKeys::PageDown) return MoveSelection(VisibleRows);
        if (InKey == EKeys::Home) return MoveSelection(-Candidates.Num());
        if (InKey == EKeys::End) return MoveSelection(Candidates.Num());
        return false;
    }
    int32 Next = FocusedSlot;
    const int32 Row = FocusedSlot % 3;
    if (InKey == EKeys::Up) Next = FocusedSlot - (Row > 0 ? 1 : 0);
    else if (InKey == EKeys::Down) Next = FocusedSlot + (Row < 2 ? 1 : 0);
    else if (InKey == EKeys::Left) Next = FMath::Max(Row, FocusedSlot - 3);
    else if (InKey == EKeys::Right) Next = FMath::Min(6 + Row, FocusedSlot + 3);
    else return false;
    if (Next != FocusedSlot)
    {
        FocusedSlot = Next;
        HoverTarget = INDEX_NONE;
        Changed();
    }
    return true;
}

int32 SDivisionInventoryContent::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    using namespace DivisionInventoryContentPrivate;
    FPainter P{ AllottedGeometry, OutDrawElements, LayerId, RegularFont, MediumFont, NumberFont,
        InWidgetStyle.GetColorAndOpacityTint(), bParentEnabled ? 1.0f : 0.5f };

    P.Text(TEXT("Inventory"), 0, 0, 320, 32, Orange, 1);
    const float OverviewEmphasis = GetEmphasis(OverviewTarget);
    P.Translation = HoverOffset(OverviewEmphasis);
    P.Text(TEXT("Overview"), 368, 15, 172, 22, Orange, 1);
    P.Box(350, 61, 208, 3, Orange, 1);
    if (OverviewEmphasis > 0)
        P.Box(350, 64, 208 * OverviewEmphasis, 1, FLinearColor(1, 1, 1, OverviewEmphasis), 1);
    P.Translation = FVector2D::ZeroVector;
    P.Text(TEXT("Character"), 590, 15, 190, 22, FLinearColor(0.64f, 0.67f, 0.67f, 0.50f));
    P.Text(TEXT("Pouches"), 812, 15, 155, 22, FLinearColor(0.64f, 0.67f, 0.67f, 0.50f));
    P.Text(TEXT("Appearance"), 992, 15, 208, 22, FLinearColor(0.64f, 0.67f, 0.67f, 0.50f));

    const TCHAR* const StatNames[] = { TEXT("Primary DPS"), TEXT("Health"), TEXT("Skill Power") };
    const TCHAR* const CoreNames[] = { TEXT("Firearms"), TEXT("Stamina"), TEXT("Electronics") };
    const TCHAR* const StatIcons[] = { TEXT("Target"), TEXT("Shield"), TEXT("Biohazard") };
    const int32 Values[] = { Stats.PrimaryDPS, Stats.Health, Stats.SkillPower };
    const int32 Cores[] = { Stats.Firearms, Stats.Stamina, Stats.Electronics };
    const FDivisionInventoryStats Preview = CalculateStats(GetSelectedItem());
    const int32 Deltas[] = { Preview.PrimaryDPS - Stats.PrimaryDPS, Preview.Health - Stats.Health, Preview.SkillPower - Stats.SkillPower };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float X = Index * 404.0f;
        P.Card(FSlateRect(X, 95, X + 392, 240));
        P.Icon(FName(StatIcons[Index]), X + 16, 110, 24, 24, Muted);
        P.Text(StatNames[Index], X + 52, 102, 324, 22, White, 1);
        P.Text(Number(Values[Index]), X + 16, 139, Deltas[Index] ? 255 : 360, 42, White, 2);
        if (bInList && Deltas[Index])
            P.Text((Deltas[Index] > 0 ? TEXT("+") : TEXT("")) + Number(Deltas[Index]),
                X + 266, 158, 110, 20, Deltas[Index] > 0 ? Green : Red, 2, true);
        P.Box(X + 16, 207, 360, 1, FLinearColor(0.65f, 0.7f, 0.7f, 0.20f), 1);
        P.Text(CoreNames[Index], X + 16, 211, 220, 20, Muted);
        P.Text(Number(Cores[Index]), X + 246, 211, 130, 20, White, 2, true);
    }

    const float BaseOpacity = P.Opacity;
    P.Opacity *= FMath::SmoothStep(0.0f, 1.0f, PageAnimation);
    P.Translation = PageOffset(PageAnimation);
    if (!bInList)
    {
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            const FDivisionInventoryItem* Item = GetEquippedItem(static_cast<EDivisionInventorySlot>(SlotIndex));
            if (!Item) continue;
            const FSlateRect Rect = SlotRect(SlotIndex);
            const float X = Rect.Left;
            const float Y = Rect.Top;
            const FLinearColor Quality = QualityColor(Item->Quality);
            const float Amount = GetEmphasis(SlotIndex);
            P.Translation = PageOffset(PageAnimation) + HoverOffset(Amount);
            if (SlotIndex < 3)
            {
                // 225 px weapon tile, 13 px open gap, then two separate talent squares.
                P.RarityTile(FSlateRect(X, Y, X + 225, Rect.Bottom), Quality, Amount);
                P.Text(SlotNames[SlotIndex], X + 7, Y + 9, 211, 20, White);
                P.Icon(Item->Icon, X + 14, Y + 42, 197, 86, FLinearColor::White);
                P.Text(TEXT("Talents"), X + 238, Y + 9, 112, 20, Muted);
                for (int32 TalentIndex = 0; TalentIndex < Item->Talents.Num() && TalentIndex < 2; ++TalentIndex)
                {
                    const float TalentX = X + 238 + TalentIndex * 60.0f;
                    P.Card(FSlateRect(TalentX, Y + 48, TalentX + 52, Y + 100), false, false, .24f, Amount);
                    P.Icon(Item->Talents[TalentIndex].Icon, TalentX + 10, Y + 58, 32, 32, White);
                }
            }
            else
            {
                // Keep the icon and armor panels separate so the scene shows through the gap.
                P.RarityTile(FSlateRect(X, Y, X + 130, Rect.Bottom), Quality, Amount);
                P.Icon(Item->Icon, X + 16, Y + 24, 98, 94, FLinearColor::White);
                P.Card(FSlateRect(X + 142, Y, Rect.Right, Rect.Bottom), false, false, .24f, Amount);
                P.Text(SlotNames[SlotIndex], X + 158, Y + 9, 225, 20, FMath::Lerp(Muted, Orange, Amount));
                P.Text(Number(Item->Armor), X + 158, Y + 40, 225, 36, White, 2, true);
                P.Text(TEXT("ARMOR"), X + 158, Y + 100, 225, 20, Muted, 0, true);
            }
        }
    }
    else
    {
        P.Text(SlotNames[static_cast<int32>(ActiveSlot)], 0, 250, 500, 26, White, 1);
        P.Text(FString::Printf(TEXT("%d ITEMS"), Candidates.Num()), 520, 254, 208, 20, Muted, 0, true);
        const FDivisionInventoryItem* Equipped = GetEquippedItem(ActiveSlot);
        // Clip moving rows as one viewport, including partially entering/leaving rows.
        OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.ToPaintGeometry(
            FVector2D(ListViewport.Right - ListViewport.Left, ListViewport.Bottom - ListViewport.Top),
            FSlateLayoutTransform(FVector2D(ListViewport.Left, ListViewport.Top) + PageOffset(PageAnimation)))));
        const int32 FirstDrawn = FMath::Max(0, FMath::FloorToInt(ScrollOffset / RowPitch));
        for (int32 CandidateIndex = FirstDrawn; CandidateIndex < Candidates.Num() && CandidateIndex <= FirstDrawn + VisibleRows; ++CandidateIndex)
        {
            const FDivisionInventoryItem& Item = Items[Candidates[CandidateIndex]];
            const FSlateRect Rect = ListRowRect(CandidateIndex, ScrollOffset);
            const float Amount = GetEmphasis(RowTarget + CandidateIndex);
            P.Translation = PageOffset(PageAnimation) + HoverOffset(Amount);
            const FLinearColor Quality = QualityColor(Item.Quality);
            P.RarityTile(FSlateRect(0, Rect.Top, 112, Rect.Bottom), Quality, Amount);
            P.Icon(Item.Icon, 12, Rect.Top + 10, 88, 59, FLinearColor::White);
            P.Card(FSlateRect(124, Rect.Top, Rect.Right, Rect.Bottom), false, false, .24f, Amount);
            FLinearColor NameWash = Orange;
            NameWash.A *= Amount;
            if (Amount > 0) P.Box(124, Rect.Top, 606, 37, NameWash, 1);
            const FLinearColor NameColor = FMath::Lerp(White, FLinearColor(0.055f, 0.065f, 0.07f, 1), Amount);
            P.Text(Item.Name, 136, Rect.Top + 3, 369, 22, NameColor, 1);
            P.Text(QualityName(Item.Quality) + TEXT(" / ") + Item.Category, 136, Rect.Top + 45, 369, 20, Quality);
            P.Text(Number(IsWeapon(Item.Slot) ? Item.Damage : Item.Armor) + (IsWeapon(Item.Slot) ? TEXT(" DMG") : TEXT(" ARM")),
                515, Rect.Top + 3, 191, 22, NameColor, 2, true);
            P.Text(Equipped && Equipped->Id == Item.Id ? TEXT("EQUIPPED") : TEXT("LVL 30"),
                533, Rect.Top + 46, 173, 20, Equipped && Equipped->Id == Item.Id ? Orange : Muted, 0, true);
        }
        OutDrawElements.PopClip();
        P.Translation = PageOffset(PageAnimation);
        if (Candidates.Num() > VisibleRows)
        {
            const float TrackHeight = VisibleRows * RowPitch - 6;
            const float ThumbHeight = TrackHeight * VisibleRows / Candidates.Num();
            const float ThumbY = ListTop + (TrackHeight - ThumbHeight) * ScrollOffset / (RowPitch * (Candidates.Num() - VisibleRows));
            P.Box(738, ListTop, 3, TrackHeight, FLinearColor(0.65f, 0.7f, 0.7f, 0.2f), 1);
            P.Box(738, ThumbY, 3, ThumbHeight, Orange, 2);
        }

        if (const FDivisionInventoryItem* Item = GetSelectedItem())
        {
            const FLinearColor Quality = QualityColor(Item->Quality);
            P.Card(FSlateRect(760, 275, 1200, 426), false, false, 0.28f);
            P.Text(Item->Name, 780, 281, 400, 26, Quality, 1);
            P.Text(QualityName(Item->Quality) + TEXT(" / ") + Item->Category, 780, 320, 400, 20, Muted);
            const bool bWeapon = IsWeapon(Item->Slot);
            const TCHAR* const WeaponLabels[] = { TEXT("DMG"), TEXT("RPM"), TEXT("MAG") };
            const TCHAR* const GearLabels[] = { TEXT("ARMOR"), TEXT("LEVEL"), TEXT("CORE") };
            const int32 WeaponValues[] = { Item->Damage, Item->RPM, Item->Magazine };
            const int32 GearValues[] = { Item->Armor, Item->Level, FMath::Max3(Item->Firearms, Item->Stamina, Item->Electronics) };
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const float X = 780 + Index * 136.0f;
                P.Text(bWeapon ? WeaponLabels[Index] : GearLabels[Index], X, 351, 127, 20, Muted);
                P.Text(Number(bWeapon ? WeaponValues[Index] : GearValues[Index]), X, 376, 127, 26, White, 2);
            }
            P.Card(FSlateRect(760, 437, 1200, 582), false, false, 0.28f);
            if (bWeapon)
            {
                P.Bar(TEXT("Accuracy"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Item->Accuracy * 100)), 443, Item->Accuracy);
                P.Bar(TEXT("Reload"), FString::Printf(TEXT("%.1f s"), Item->ReloadSeconds), 475, 1.0f - Item->ReloadSeconds / 6.0f);
                P.Bar(TEXT("Range"), FString::Printf(TEXT("%.0f m"), Item->RangeMeters), 507, Item->RangeMeters / 60.0f);
                P.Bar(TEXT("Stability"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Item->Stability * 100)), 539, Item->Stability);
            }
            else
            {
                P.Bar(TEXT("Firearms"), TEXT("+") + Number(Item->Firearms), 443, Item->Firearms / 650.0f);
                P.Bar(TEXT("Stamina"), TEXT("+") + Number(Item->Stamina), 475, Item->Stamina / 650.0f);
                P.Bar(TEXT("Electronics"), TEXT("+") + Number(Item->Electronics), 507, Item->Electronics / 650.0f);
                const int32 ArmorDelta = Equipped ? Item->Armor - Equipped->Armor : 0;
                P.Text(TEXT("Armor change"), 780, 539, 210, 20, Muted);
                P.Text((ArmorDelta > 0 ? TEXT("+") : TEXT("")) + Number(ArmorDelta), 1000, 539, 176, 20,
                    ArmorDelta == 0 ? Muted : ArmorDelta > 0 ? Green : Red, 2, true);
            }
            P.Card(FSlateRect(760, 593, 1200, 715), false, false, 0.28f);
            for (int32 Index = 0; Index < Item->Talents.Num() && Index < 2; ++Index)
            {
                const float Y = 601 + Index * 55.0f;
                P.Icon(Item->Talents[Index].Icon, 780, Y + 9, 28, 28, Quality);
                P.Text(Item->Talents[Index].Name, 823, Y, 353, 20, Quality, 1);
                P.Text(Item->Talents[Index].Description, 823, Y + 27, 353, 20, White);
            }
        }
    }
    P.Opacity = BaseOpacity;
    P.Translation = FVector2D::ZeroVector;

    // Footer groups deliberately do not share a continuous backing plate.
    P.Card(FSlateRect(0, 735, 350, 777));
    P.Text(TEXT("Capacity"), 14, 741, 145, 20, Muted);
    P.Text(FString::Printf(TEXT("%d / 80"), Items.Num()), 183, 738, 151, 22, White, 2, true);
    P.Card(FSlateRect(390, 735, 789, 777));
    P.Icon(FName(TEXT("Currency")), 405, 746, 23, 23, Muted);
    P.Text(TEXT("148,250"), 440, 738, 180, 22, White, 2);
    P.Text(TEXT("CREDITS"), 634, 745, 139, 20, Muted, 0, true);

    if (bInList)
    {
        const float BackEmphasis = GetEmphasis(BackTarget);
        P.Translation = HoverOffset(BackEmphasis);
        P.Card(BackRect, false, false, .24f, BackEmphasis);
        P.Text(TEXT("Esc  Back"), 14, 798, 112, 20, White, 1);
        P.Translation = FVector2D::ZeroVector;
        P.Text(TEXT("Arrows  Select     Wheel  Scroll"), 165, 799, 563, 20, Muted);
        const FDivisionInventoryItem* Selected = GetSelectedItem();
        const FDivisionInventoryItem* Equipped = GetEquippedItem(ActiveSlot);
        const bool bCanEquip = Selected && Equipped && Selected->Id != Equipped->Id;
        const float EquipEmphasis = GetEmphasis(EquipTarget);
        P.Translation = HoverOffset(EquipEmphasis);
        P.Card(EquipRect, false, false, .24f, EquipEmphasis);
        P.Text(bCanEquip ? TEXT("Enter  Equip") : TEXT("Equipped"), 780, 797, 216, 22, bCanEquip ? Orange : Muted, 1);
        P.Translation = FVector2D::ZeroVector;
    }
    else
    {
        P.Text(TEXT("Enter / Click  Inspect"), 0, 795, 310, 20, White);
        P.Text(TEXT("Arrows  Navigate"), 330, 795, 255, 20, Muted);
        P.Text(TEXT("Esc  Close inventory"), 612, 795, 335, 20, Muted);
    }
    P.Text(TEXT("Demo loadout / talents are descriptive"), 0, 861, 710, 20, Muted);
    P.Text(TEXT("LEVEL"), 916, 842, 100, 20, Muted);
    P.Text(TEXT("30"), 1024, 828, 70, 32, Orange, 2, true);
    P.Text(TEXT("XP MAX"), 1100, 842, 100, 20, Muted, 0, true);
    P.Box(916, 881, 284, 4, FLinearColor(0.6f, 0.65f, 0.65f, 0.23f), 1);
    P.Box(916, 881, 284, 4, Orange, 2);
    return LayerId + 4;
}
