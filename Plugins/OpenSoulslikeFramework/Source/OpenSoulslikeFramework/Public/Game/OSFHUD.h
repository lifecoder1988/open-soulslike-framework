// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Game/OSFPlayerController.h"
#include "OSFHUD.generated.h"

class AOSFPlayerCharacter;
class AOSFPlayerController;
class UFont;

/**
 * Default canvas HUD: stat bars, equipment slots, currency, status buildup, buffs, compass, lock-on reticle,
 * enemy & boss health bars, prompts, notifications and every menu of AOSFPlayerController.
 * Everything it draws comes from public component / controller APIs, so it can be replaced by UMG.
 * Each section can be switched off (bDraw*, ExternalMenus, ExternalNotifications) when another layer draws it,
 * e.g. an embedded web HUD or UMG widgets; menu input stays with AOSFPlayerController either way.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API AOSFHUD : public AHUD
{
	GENERATED_BODY()

public:
	AOSFHUD();

	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	float UIScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	bool bShowControlsHint = true;

	/** Text font. When null, a runtime Roboto font with a CJK fallback (DroidSansFallback) is built. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	TObjectPtr<UFont> TextFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	TObjectPtr<UFont> LargeTextFont;

	/** TTF / OTF files for the runtime fonts when TextFont / LargeTextFont are unset (DroidSansFallback still covers missing glyphs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FString TextFontFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FString LargeTextFontFile;

	/** Key hints shown for the first seconds of play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FText ControlsHint;

	/** ControlsHint while the player uses a gamepad (AOSFPlayerController::IsUsingGamepad). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FText ControlsHintGamepad;

	/** Seconds of actual play (CanShowControlsHint, no menu open) the key hints stay up; the last 3 fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	float ControlsHintSeconds = 30.f;

	/** Text scale of the key hints (drawn on a dark band at the bottom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	float ControlsHintScale = 1.25f;

	/** False while the game has the screen for something else (intro, cutscene): the hint time doesn't run. */
	virtual bool CanShowControlsHint() const { return true; }

	/** Label next to the currency counter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FText CurrencyLabel;

	/** Draws the tracked quest objectives (UOSFQuestComponent) at the top right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	bool bShowQuestTracker = true;

	/** Health / FP / stamina bars (top left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawStatBars = true;

	/** Status buildup bars and active buffs under the stat bars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawStatusEffects = true;

	/** Weapon / ammo / quick item slots (bottom left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawEquipment = true;

	/** Currency counter (bottom right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawCurrency = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawCompass = true;

	/** Health bars over locked / recently hit enemies, with the riposte cue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawEnemyBars = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawLockOnReticle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawBossBar = true;

	/** Interaction prompt ("[E] Rest"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	bool bDrawPrompt = true;

	/** Menus another layer draws (e.g. Dialogue, Rest); this HUD skips them entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	TSet<EOSFMenu> ExternalMenus;

	/** Notification types another layer draws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD|Sections")
	TSet<EOSFNotifyType> ExternalNotifications;

	UFUNCTION(BlueprintPure, Category = "OSF|HUD")
	UFont* GetHUDFont(bool bLarge = false);

protected:
	void DrawStats(AOSFPlayerCharacter* Player);
	void DrawEquipment(AOSFPlayerCharacter* Player);
	void DrawCurrency(AOSFPlayerCharacter* Player);
	void DrawLockOnReticle(AOSFPlayerCharacter* Player);
	void DrawCompass(AOSFPlayerCharacter* Player);
	void DrawEnemyBars(AOSFPlayerCharacter* Player);
	virtual void DrawBossBar(AOSFPlayerCharacter* Player);
	void DrawPrompt(AOSFPlayerCharacter* Player);
	void DrawNotifications(AOSFPlayerController* PC);
	void DrawMenu(AOSFPlayerController* PC);
	void DrawControls();
	void DrawQuests(AOSFPlayerCharacter* Player);

	void DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color, float Trail = -1.f);
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCenter = false, UFont* Font = nullptr);
	float DrawWrapped(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, float Scale = 1.f);

	float S(float Value) const { return Value * UIScale * Scale; }

	float Scale = 1.f;
	float HealthTrail = 1.f;
	float BossTrail = 1.f;
	double StartTime = -1.0;
	/** Play time the key hints have been visible (see ControlsHintSeconds). */
	float ControlsHintShown = 0.f;
};
