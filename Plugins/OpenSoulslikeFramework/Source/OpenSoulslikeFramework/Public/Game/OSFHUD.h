// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OSFHUD.generated.h"

class AOSFPlayerCharacter;
class AOSFPlayerController;
class UFont;

/**
 * Default canvas HUD: stat bars, equipment slots, currency, status buildup, buffs, compass, lock-on reticle,
 * enemy & boss health bars, prompts, notifications and every menu of AOSFPlayerController.
 * Everything it draws comes from public component / controller APIs, so it can be replaced by UMG.
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

	/** Key hints shown for the first seconds of play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FText ControlsHint;

	/** Label next to the currency counter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	FText CurrencyLabel;

	/** Draws the tracked quest objectives (UOSFQuestComponent) at the top right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HUD")
	bool bShowQuestTracker = true;

	UFUNCTION(BlueprintPure, Category = "OSF|HUD")
	UFont* GetHUDFont(bool bLarge = false);

protected:
	void DrawStats(AOSFPlayerCharacter* Player);
	void DrawEquipment(AOSFPlayerCharacter* Player);
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
};
