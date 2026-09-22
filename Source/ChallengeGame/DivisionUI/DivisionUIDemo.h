#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "DivisionUIDemo.generated.h"

class APawn;
class APlayerController;
class SDivisionStatusContent;
class SBackgroundBlur;
class UFont;
class UDivisionInventoryWidget;
class UDivisionMenuCameraModifier;
class IInputProcessor;

// Presentation data only. The independent demo supplies simulated gameplay values.
USTRUCT(BlueprintType)
struct FDivisionUIStatus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Magazine = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Reserve = 780;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Grenades = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Medkits = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PulseRemaining = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TurretRemaining = 0.0f;
};

UCLASS()
class CHALLENGEGAME_API UDivisionStatusWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    explicit UDivisionStatusWidget(const FObjectInitializer& ObjectInitializer);

    void SetStatus(const FDivisionUIStatus& InStatus);
    void UpdatePlacement(APlayerController* PC, float DeltaSeconds);
    void ResetFollow() { bHasPosition = false; }
    void SetMenuVisibility(float Amount) { MenuVisibility = Amount; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Division UI")
    FVector2D ScreenOffset = FVector2D(140.0f, -10.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Division UI")
    float FollowSpeed = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Division UI")
    float PlaneAngle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Division UI", meta=(ClampMin="0.0", ClampMax="20.0"))
    float BlurStrength = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category="Division UI|Typography")
    TObjectPtr<UFont> BordaNumericFont;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    friend class SDivisionStatusContent;
    int32 PaintStatus(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
        int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const;

    FDivisionUIStatus Status;
    TWeakObjectPtr<APawn> LastPawn;
    FVector2D DisplayPosition = FVector2D::ZeroVector;
    bool bHasPosition = false;
    float DisplayHealth = 1.0f;
    float DamageTrail = 1.0f;
    float DamageHold = 0.0f;
    float AmmoFlash = 0.0f;
    float ConsumableFlash[2] = {0.0f, 0.0f};
    float DamageFlash = 0.0f;
    float ReadyFlash[2] = {0.0f, 0.0f};
    float Reveal = 0.0f;
    float MenuVisibility = 1.0f;
    TSharedPtr<SDivisionStatusContent> StatusContent;
    TSharedPtr<SBackgroundBlur> BackgroundBlur;
};

UCLASS()
class CHALLENGEGAME_API UDivisionScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    explicit UDivisionScreenWidget(const FObjectInitializer& ObjectInitializer);

    void UpdateNavigation(APlayerController* PC, bool bHelp, float DeltaSeconds);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
        int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
    UPROPERTY(EditDefaultsOnly, Category="Division UI|Typography")
    TObjectPtr<UFont> BordaNumericFont;

    float Heading = 0.0f;
    float ExperiencePreviewTime = 0.0f;
    bool bShowHelp = true;
};

UCLASS()
class CHALLENGEGAME_API ADivisionDemoHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostRender() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UFUNCTION(BlueprintCallable, Category="Division UI")
    void PushStatus(const FDivisionUIStatus& InStatus);

    UFUNCTION(BlueprintCallable, Category="Division UI")
    void ResetStatusFollow();

    UFUNCTION(BlueprintCallable, Category="Division UI")
    void ToggleInventory();

    void HandleMenuBack();
    UUserWidget* GetActiveMenu() const;

    // Disable this when wiring real gameplay state into PushStatus.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Division UI")
    bool bDemoControls = true;

private:
    void OpenMenu();
    void UpdateDemo(APlayerController* PC, float DeltaSeconds);
    UPROPERTY(Transient) TObjectPtr<UDivisionStatusWidget> StatusWidget;
    UPROPERTY(Transient) TObjectPtr<UDivisionScreenWidget> ScreenWidget;
    UPROPERTY(Transient) TObjectPtr<UDivisionInventoryWidget> InventoryWidget;
    UPROPERTY(Transient) TObjectPtr<UDivisionMenuCameraModifier> MenuCamera;
    TSharedPtr<IInputProcessor> MenuInput;
    float MenuProgress = 0.0f;
    bool bMenuOpen = false;
    bool bMenuInputLocked = false;
    bool bPreviousMouseCursor = false;
    FDivisionUIStatus Status;
    bool bHelp = true;
};

// Applied only through the independent map's World Settings override.
UCLASS()
class CHALLENGEGAME_API ADivisionUIDemoGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADivisionUIDemoGameMode();
};
