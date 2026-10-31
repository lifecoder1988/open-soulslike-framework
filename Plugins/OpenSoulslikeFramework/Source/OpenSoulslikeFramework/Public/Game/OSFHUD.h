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

protected:
	void DrawStats(AOSFPlayerCharacter* Player);
	void DrawEquipment(AOSFPlayerCharacter* Player);
	void DrawCompass(AOSFPlayerCharacter* Player);
	void DrawEnemyBars(AOSFPlayerCharacter* Player);
	void DrawBossBar(AOSFPlayerCharacter* Player);
	void DrawPrompt(AOSFPlayerCharacter* Player);
	void DrawNotifications(AOSFPlayerController* PC);
	void DrawMenu(AOSFPlayerController* PC);
	void DrawControls();

	void DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color, float Trail = -1.f);
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCenter = false, UFont* Font = nullptr);
	float DrawWrapped(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, float Scale = 1.f);

	float S(float Value) const { return Value * UIScale * Scale; }

	float Scale = 1.f;
	float HealthTrail = 1.f;
	float BossTrail = 1.f;
	double StartTime = -1.0;
};
