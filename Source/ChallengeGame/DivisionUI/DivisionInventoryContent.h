#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

enum class EDivisionInventorySlot : uint8
{
    Primary, Secondary, Sidearm, Vest, Mask, Backpack, Kneepads, Gloves, Holster, Count
};

enum class EDivisionInventoryQuality : uint8
{
    Standard, Specialized, Superior, HighEnd
};

struct FDivisionInventoryTalent
{
    FString Name;
    FString Description;
    FName Icon = FName(TEXT("Talent"));
};

// Demo values are deliberately independent of actors, assets and gameplay equipment.
struct FDivisionInventoryItem
{
    FName Id;
    EDivisionInventorySlot Slot = EDivisionInventorySlot::Primary;
    EDivisionInventoryQuality Quality = EDivisionInventoryQuality::Standard;
    FString Name;
    FString Category;
    FName Icon;
    int32 Level = 30;
    int32 Damage = 0;
    int32 RPM = 0;
    int32 Magazine = 0;
    int32 Armor = 0;
    int32 Firearms = 0;
    int32 Stamina = 0;
    int32 Electronics = 0;
    float Accuracy = 0.0f;
    float ReloadSeconds = 0.0f;
    float RangeMeters = 0.0f;
    float Stability = 0.0f;
    TArray<FDivisionInventoryTalent> Talents;
};

struct FDivisionInventoryStats
{
    int32 PrimaryDPS = 0;
    int32 Health = 0;
    int32 SkillPower = 0;
    int32 Firearms = 0;
    int32 Stamina = 0;
    int32 Electronics = 0;
    int32 Armor = 0;
};

class SDivisionInventoryContent : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDivisionInventoryContent)
        : _RegularFont(nullptr), _MediumFont(nullptr), _NumberFont(nullptr) {}
        SLATE_ARGUMENT(UObject*, RegularFont)
        SLATE_ARGUMENT(UObject*, MediumFont)
        SLATE_ARGUMENT(UObject*, NumberFont)
    SLATE_END_ARGS()

    // The owning UUserWidget must retain these Borda UObjects (e.g. UPROPERTY fonts).
    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

    // Coordinates are logical 1200x900 coordinates after the host's inverse projection.
    // Input methods return handled; use GetRevision() to detect a visual change.
    bool Back(); // True only when a slot list was closed.
    void ResetPage(); // Return to overview; preserve demo equipment.
    bool PointerMove(FVector2D Logical);
    bool PointerDown(FVector2D Logical);
    void PointerLeave();
    bool Scroll(float Delta, FVector2D Logical);
    bool Key(FKey InKey);
    bool Advance(float DeltaTime); // True on every animation step, including its final frame.
    uint64 GetRevision() const { return Revision; }

    using FOnDemoEquipped = TFunction<void(EDivisionInventorySlot, FName, const FDivisionInventoryStats&)>;
    void SetOnDemoEquipped(FOnDemoEquipped Callback) { OnDemoEquipped = MoveTemp(Callback); }
    const TArray<FDivisionInventoryItem>& GetItems() const { return Items; }
    const FDivisionInventoryStats& GetStats() const { return Stats; }
    const FDivisionInventoryItem* GetEquippedItem(EDivisionInventorySlot Slot) const;
    const FDivisionInventoryItem* GetSelectedItem() const;
    bool IsInList() const { return bInList; }
    EDivisionInventorySlot GetActiveSlot() const { return ActiveSlot; }
    bool OpenSlot(EDivisionInventorySlot Slot);
    bool SelectItem(FName ItemId);
    bool EquipSelected();

protected:
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    void BuildDemo();
    void Changed();
    void StartPageAnimation();
    void KeepSelectionVisible();
    bool MoveSelection(int32 Offset);
    FDivisionInventoryStats CalculateStats(const FDivisionInventoryItem* Replacement = nullptr) const;
    int32 HitTest(FVector2D Logical) const;
    void UpdatePointerFocus();
    float GetEmphasis(int32 Target) const;

    FSlateFontInfo RegularFont;
    FSlateFontInfo MediumFont;
    FSlateFontInfo NumberFont;
    TArray<FDivisionInventoryItem> Items;
    TArray<int32> EquippedIndices;
    TArray<int32> Candidates;
    FDivisionInventoryStats Stats;
    FOnDemoEquipped OnDemoEquipped;
    EDivisionInventorySlot ActiveSlot = EDivisionInventorySlot::Primary;
    int32 SelectedCandidate = 0;
    int32 FirstVisible = 0;
    int32 FocusedSlot = 0;
    int32 HoverTarget = INDEX_NONE;
    TMap<int32, float> Emphasis;
    FVector2D PointerPosition = FVector2D::ZeroVector;
    float ScrollOffset = 0.0f;
    bool bPointerActive = false;
    float PageAnimation = 1.0f;
    uint64 Revision = 0;
    bool bInList = false;
};
