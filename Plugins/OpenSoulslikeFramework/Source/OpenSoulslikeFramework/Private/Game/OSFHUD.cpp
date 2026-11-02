// Copyright Open Soulslike Framework contributors. MIT License.

#include "Game/OSFHUD.h"
#include "Game/OSFPlayerController.h"
#include "Characters/OSFPlayerCharacter.h"
#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFBossComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFLockOnComponent.h"
#include "Interaction/OSFInteractionComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "World/OSFCompassMarkerComponent.h"
#include "Progression/OSFQuestComponent.h"
#include "OSFGameplayTags.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "CanvasItem.h"

#define LOCTEXT_NAMESPACE "OSFHUD"

AOSFHUD::AOSFHUD()
{
	CurrencyLabel = LOCTEXT("Runes", "RUNES");
	ControlsHint = LOCTEXT("Controls", "LMB light  Q heavy  RMB guard  F skill/parry  Space roll (hold: sprint)  C jump  Tab lock-on  R item  E interact  1/wheel cycle weapon  2/3 cycle left/item  T two-hand  I menu");
	ControlsHintGamepad = LOCTEXT("ControlsPad", "RB light  RT heavy  LB guard  LT skill/parry  B roll (hold: sprint)  Y jump  R3 lock-on  X item  A interact  D-pad cycle  Menu: menu");
}

#undef LOCTEXT_NAMESPACE

UFont* AOSFHUD::GetHUDFont(bool bLarge)
{
	TObjectPtr<UFont>& Slot = bLarge ? LargeTextFont : TextFont;
	if (!Slot)
	{
		// Runtime composite font: TextFontFile / LargeTextFontFile or Roboto, with DroidSansFallback for missing glyphs (CJK...).
		const UFont* Base = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
		UFont* Font = NewObject<UFont>(this, bLarge ? TEXT("OSFHUDFontLarge") : TEXT("OSFHUDFont"));
		Font->FontCacheType = EFontCacheType::Runtime;
		Font->LegacyFontSize = Base ? Base->LegacyFontSize : (bLarge ? 24 : 16);
		const FString Fonts = FPaths::EngineContentDir() / TEXT("Slate/Fonts/");
		const FString& Custom = bLarge ? LargeTextFontFile : TextFontFile;
		const FString File = !Custom.IsEmpty() && FPaths::FileExists(Custom) ? Custom : Fonts / (bLarge ? TEXT("Roboto-Bold.ttf") : TEXT("Roboto-Regular.ttf"));
		Font->GetMutableInternalCompositeFont() = FCompositeFont(TEXT("Regular"), File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		Font->GetMutableInternalCompositeFont().FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), Fonts / TEXT("DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		Slot = Font;
	}
	return Slot;
}

void AOSFHUD::DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color, float Trail)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 0.9f), X, Y, Width, Height);
	if (Trail > Percent)
	{
		DrawRect(FLinearColor(0.9f, 0.8f, 0.5f, 0.8f), X, Y, Width * FMath::Clamp(Trail, 0.f, 1.f), Height);
	}
	DrawRect(Color, X, Y, Width * FMath::Clamp(Percent, 0.f, 1.f), Height);
}

void AOSFHUD::DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float TextScale, bool bCenter, UFont* Font)
{
	if (!Canvas)
	{
		return;
	}
	UFont* UseFont = Font ? Font : GetHUDFont(false);
	float W = 0.f, H = 0.f;
	Canvas->TextSize(UseFont, Text, W, H, TextScale * Scale, TextScale * Scale);
	FCanvasTextItem Item(FVector2D(bCenter ? X - W * 0.5f : X, Y), FText::FromString(Text), UseFont, Color);
	Item.Scale = FVector2D(TextScale * Scale);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

float AOSFHUD::DrawWrapped(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, float TextScale)
{
	UFont* Font = GetHUDFont(false);
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	float CursorY = Y;
	const float LineHeight = Font->GetMaxCharHeight() * TextScale * Scale * 1.15f;
	for (const FString& Paragraph : Paragraphs)
	{
		// Character based wrapping (works for CJK text without spaces); breaks at the last space when there is one.
		FString Line;
		for (int32 Index = 0; Index < Paragraph.Len(); ++Index)
		{
			const FString Candidate = Line + Paragraph[Index];
			float W = 0.f, H = 0.f;
			Canvas->TextSize(Font, Candidate, W, H, TextScale * Scale, TextScale * Scale);
			if (W > MaxWidth && !Line.IsEmpty())
			{
				int32 Space = INDEX_NONE;
				FString Rest;
				if (Line.FindLastChar(TEXT(' '), Space) && Space > 0 && Paragraph[Index] != TEXT(' '))
				{
					Rest = Line.Mid(Space + 1);
					Line = Line.Left(Space);
				}
				DrawTextAt(Line, X, CursorY, Color, TextScale, false, Font);
				CursorY += LineHeight;
				Line = (Rest + Paragraph[Index]).TrimStart();
			}
			else
			{
				Line = Candidate;
			}
		}
		DrawTextAt(Line, X, CursorY, Color, TextScale, false, Font);
		CursorY += LineHeight;
	}
	return CursorY;
}

void AOSFHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	Scale = Canvas->ClipY / 1080.f;
	AOSFPlayerController* PC = Cast<AOSFPlayerController>(GetOwningPlayerController());
	AOSFPlayerCharacter* Player = PC ? PC->GetOSFPlayer() : nullptr;
	if (StartTime < 0.0)
	{
		StartTime = GetWorld()->GetTimeSeconds();
	}
	if (!Player)
	{
		return;
	}
	// The title screen covers everything.
	if (PC->GetCurrentMenu() == EOSFMenu::Title)
	{
		DrawMenu(PC);
		return;
	}

	if (!PC->IsInMenu() || PC->GetCurrentMenu() == EOSFMenu::Dialogue)
	{
		if (bDrawEnemyBars)
		{
			DrawEnemyBars(Player);
		}
		if (bDrawLockOnReticle)
		{
			DrawLockOnReticle(Player);
		}
		if (bDrawCompass)
		{
			DrawCompass(Player);
		}
		if (bDrawBossBar)
		{
			DrawBossBar(Player);
		}
		if (bDrawPrompt)
		{
			DrawPrompt(Player);
		}
	}
	if (bDrawStatBars || bDrawStatusEffects)
	{
		DrawStats(Player);
	}
	if (bDrawEquipment)
	{
		DrawEquipment(Player);
	}
	if (bDrawCurrency)
	{
		DrawCurrency(Player);
	}
	if (bShowQuestTracker && !PC->IsInMenu())
	{
		DrawQuests(Player);
	}
	DrawMenu(PC);
	DrawNotifications(PC);
	if (bShowControlsHint && !PC->IsInMenu())
	{
		DrawControls();
	}
}

void AOSFHUD::DrawStats(AOSFPlayerCharacter* Player)
{
	const UOSFStatComponent* Stats = Player->Stats;
	const float X = S(48.f);
	float Y = S(44.f);
	const float PxPerPoint = 0.55f;

	if (bDrawStatBars)
	{
		const float HP = Stats->GetPercent(OSFTags::Stat_Health);
		HealthTrail = HP < HealthTrail ? FMath::FInterpConstantTo(HealthTrail, HP, GetWorld()->GetDeltaSeconds(), 0.35f) : HP;
		DrawBar(X, Y, S(FMath::Min(900.f, Stats->GetMax(OSFTags::Stat_Health) * PxPerPoint)), S(16.f), HP, FLinearColor(0.62f, 0.06f, 0.05f), HealthTrail);
		Y += S(24.f);
		DrawBar(X, Y, S(FMath::Min(600.f, Stats->GetMax(OSFTags::Stat_FP) * PxPerPoint * 2.2f)), S(10.f), Stats->GetPercent(OSFTags::Stat_FP), FLinearColor(0.1f, 0.25f, 0.75f));
		Y += S(18.f);
		DrawBar(X, Y, S(FMath::Min(600.f, Stats->GetMax(OSFTags::Stat_Stamina) * PxPerPoint * 2.6f)), S(10.f), Stats->GetPercent(OSFTags::Stat_Stamina), FLinearColor(0.15f, 0.55f, 0.15f));
		Y += S(22.f);
	}
	if (!bDrawStatusEffects)
	{
		return;
	}

	if (const UOSFStatusEffectComponent* Status = Player->StatusEffects)
	{
		for (const TPair<FGameplayTag, FOSFStatusState>& Pair : Status->GetStates())
		{
			const float Percent = Status->GetBuildupPercent(Pair.Key);
			if (Percent <= 0.f)
			{
				continue;
			}
			const UOSFStatusEffectData* Effect = Status->FindEffect(Pair.Key);
			const FLinearColor Color = Effect ? Effect->Color : FLinearColor::Red;
			DrawBar(X, Y, S(120.f), S(7.f), Percent, Color);
			DrawTextAt(Effect ? Effect->DisplayName.ToString() : Pair.Key.ToString(), X + S(130.f), Y - S(6.f), Color, 0.8f);
			Y += S(16.f);
		}
	}
	if (const UOSFBuffComponent* Buffs = Player->Buffs)
	{
		float BX = X;
		for (const FOSFActiveBuff& Active : Buffs->GetActiveBuffs())
		{
			if (!Active.Buff)
			{
				continue;
			}
			FString Label = Active.Buff->DisplayName.ToString();
			if (Active.Duration > 0.f)
			{
				Label += FString::Printf(TEXT(" %ds"), FMath::CeilToInt(Active.Remaining));
			}
			DrawTextAt(Label, BX, Y, Active.Buff->Color, 0.8f);
			BX += S(170.f);
		}
	}
}

void AOSFHUD::DrawEquipment(AOSFPlayerCharacter* Player)
{
	const UOSFEquipmentComponent* Equipment = Player->Equipment;
	const float BaseX = S(60.f);
	const float BaseY = Canvas->ClipY - S(250.f);
	const float Box = S(84.f);

	auto DrawSlot = [this, Box](float X, float Y, const FString& Label, const FString& Sub, const FLinearColor& Tint)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, Box, Box);
		DrawRect(FLinearColor(Tint.R, Tint.G, Tint.B, 0.25f), X + S(3.f), Y + S(3.f), Box - S(6.f), Box - S(6.f));
		DrawTextAt(Label.Left(14), X + Box * 0.5f, Y + Box * 0.32f, FLinearColor::White, 0.65f, true);
		if (!Sub.IsEmpty())
		{
			DrawTextAt(Sub, X + Box * 0.5f, Y + Box * 0.62f, FLinearColor(0.9f, 0.85f, 0.6f), 0.7f, true);
		}
	};

	const UOSFWeaponData* Right = Equipment->GetEquippedWeapon(true);
	const UOSFWeaponData* Left = Equipment->GetEquippedWeapon(false);
	const UOSFItemData* Quick = Equipment->GetActiveQuickItem();
	// Ranged weapon: the top slot shows the loaded ammo, the weapon slot magazine / reserve.
	const UOSFWeaponData* Ranged = Equipment->GetActiveRangedWeapon();
	const FOSFMagazine Magazine = Equipment->GetMagazine(Ranged);
	if (Ranged && Magazine.Ammo)
	{
		DrawSlot(BaseX + Box * 1.1f, BaseY, Magazine.Ammo->DisplayName.ToString(), FString::Printf(TEXT("%d"), Equipment->GetReserveRounds(Ranged)), Magazine.Ammo->TracerColor);
	}
	else
	{
		DrawSlot(BaseX + Box * 1.1f, BaseY, NSLOCTEXT("OSFHUD", "Spell", "Spell").ToString(), TEXT(""), FLinearColor(0.3f, 0.3f, 0.5f));
	}
	auto HandSub = [&](const UOSFWeaponData* Weapon, const TCHAR* Hand)
	{
		if (Weapon && Weapon == Ranged)
		{
			return FString::Printf(TEXT("%d / %d"), Magazine.Rounds, Weapon->Ranged.MagazineSize);
		}
		return FString(Hand);
	};
	const FString Unarmed = NSLOCTEXT("OSFHUD", "Unarmed", "Unarmed").ToString();
	DrawSlot(BaseX, BaseY + Box * 1.1f, Left ? Left->DisplayName.ToString() : Unarmed, HandSub(Left, TEXT("L")), Left ? Left->Tint : FLinearColor::Gray);
	DrawSlot(BaseX + Box * 2.2f, BaseY + Box * 1.1f, Right ? Right->DisplayName.ToString() : Unarmed, HandSub(Right, Equipment->IsTwoHanding() ? TEXT("R (2H)") : TEXT("R")), Right ? Right->Tint : FLinearColor::Gray);
	DrawSlot(BaseX + Box * 1.1f, BaseY + Box * 2.2f, Quick ? Quick->DisplayName.ToString() : TEXT("-"), Quick ? FString::Printf(TEXT("%d"), Player->Inventory->GetItemCount(Quick)) : TEXT(""), Quick ? Quick->Tint : FLinearColor::Gray);
}

void AOSFHUD::DrawCurrency(AOSFPlayerCharacter* Player)
{
	const FString Runes = FString::Printf(TEXT("%d"), Player->Inventory->GetCurrency());
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), Canvas->ClipX - S(260.f), Canvas->ClipY - S(80.f), S(220.f), S(42.f));
	DrawTextAt(Runes, Canvas->ClipX - S(60.f) - S(10.f) * Runes.Len(), Canvas->ClipY - S(74.f), FLinearColor(1.f, 0.9f, 0.6f), 1.3f);
	DrawTextAt(CurrencyLabel.ToString(), Canvas->ClipX - S(250.f), Canvas->ClipY - S(70.f), FLinearColor(0.7f, 0.65f, 0.5f), 0.8f);
}

void AOSFHUD::DrawQuests(AOSFPlayerCharacter* Player)
{
	const UOSFQuestComponent* Quests = Player->FindComponentByClass<UOSFQuestComponent>();
	if (!Quests)
	{
		return;
	}
	const float X = Canvas->ClipX - S(440.f);
	float Y = S(96.f);
	for (const UOSFQuestData* Quest : Quests->GetActiveQuests())
	{
		DrawTextAt(Quest->Title.ToString(), X, Y, FLinearColor(1.f, 0.85f, 0.5f), 0.95f);
		Y += S(24.f);
		for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
		{
			const FOSFQuestObjective& Objective = Quest->Objectives[Index];
			const int32 Done = Quests->GetObjectiveProgress(Quest, Index);
			const bool bComplete = Done >= Objective.Count;
			if (Quest->bSequential && !bComplete && Index != Quests->GetCurrentObjective(Quest))
			{
				continue;
			}
			FString Line = (bComplete ? TEXT("+ ") : TEXT("- ")) + Objective.Description.ToString();
			if (Objective.Count > 1)
			{
				Line += FString::Printf(TEXT("  %d/%d"), Done, Objective.Count);
			}
			DrawTextAt(Line, X + S(10.f), Y, bComplete ? FLinearColor(0.55f, 0.55f, 0.5f) : FLinearColor::White, 0.8f);
			Y += S(20.f);
		}
		Y += S(10.f);
	}
}

void AOSFHUD::DrawCompass(AOSFPlayerCharacter* Player)
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	const float Width = S(680.f);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = S(24.f);
	const float Yaw = PC->GetControlRotation().Yaw;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), CX - Width * 0.5f, Y, Width, S(36.f));

	auto ToScreenX = [Yaw, CX, Width](float BearingYaw) -> TOptional<float>
	{
		const float Delta = FRotator::NormalizeAxis(BearingYaw - Yaw);
		if (FMath::Abs(Delta) > 90.f)
		{
			return TOptional<float>();
		}
		return CX + Delta / 90.f * Width * 0.5f;
	};

	const TCHAR* Cardinals[] = { TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		// UE: +X = north, +Y = east.
		if (TOptional<float> X = ToScreenX(Index * 90.f))
		{
			DrawTextAt(Cardinals[Index], X.GetValue(), Y + S(3.f), FLinearColor::White, 1.35f, true);
		}
	}
	for (const TWeakObjectPtr<UOSFCompassMarkerComponent>& Weak : UOSFCompassMarkerComponent::GetAllMarkers())
	{
		const UOSFCompassMarkerComponent* Marker = Weak.Get();
		if (!Marker || !Marker->bShowOnCompass || Marker->GetWorld() != GetWorld())
		{
			continue;
		}
		const FVector To = Marker->GetComponentLocation() - Player->GetActorLocation();
		if (Marker->MaxDistance > 0.f && To.Size() > Marker->MaxDistance)
		{
			continue;
		}
		if (TOptional<float> X = ToScreenX(To.Rotation().Yaw))
		{
			// Marker pip on the strip, its label and distance below on a dark plate so it reads over bright skies.
			DrawRect(Marker->Color, X.GetValue() - S(5.f), Y + S(38.f), S(10.f), S(10.f));
			const FString Text = FString::Printf(TEXT("%s %dm"), *Marker->Label.ToString(), FMath::RoundToInt(To.Size() / 100.f));
			float W = 0.f, H = 0.f;
			Canvas->TextSize(GetHUDFont(false), Text, W, H, 1.05f * Scale, 1.05f * Scale);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X.GetValue() - W * 0.5f - S(6.f), Y + S(50.f), W + S(12.f), H + S(4.f));
			DrawTextAt(Text, X.GetValue(), Y + S(52.f), Marker->Color, 1.05f, true);
		}
	}
}

void AOSFHUD::DrawEnemyBars(AOSFPlayerCharacter* Player)
{
	const AActor* LockTarget = Player->LockOn->GetTarget();
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	for (TActorIterator<AOSFEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AOSFEnemyCharacter* Enemy = *It;
		if (Enemy->IsDead() || Enemy->IsHidden() || Enemy->IsBossFightActive())
		{
			continue;
		}
		const bool bLocked = Enemy == LockTarget;
		const bool bRecentlyHit = Now - Enemy->Combat->GetLastDamageTime() < 4.f;
		if (!bLocked && !bRecentlyHit)
		{
			continue;
		}
		const FVector Screen = Project(Enemy->GetActorLocation() + FVector(0.f, 0.f, 130.f * Enemy->VisualScale), true);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawBar(Screen.X - S(50.f), Screen.Y, S(100.f), S(7.f), Enemy->Stats->GetPercent(OSFTags::Stat_Health), FLinearColor(0.6f, 0.05f, 0.05f));
		if (Enemy->HasStateTag(OSFTags::State_PoiseBroken))
		{
			DrawTextAt(NSLOCTEXT("OSFHUD", "Riposte", "RIPOSTE!").ToString(), Screen.X, Screen.Y - S(26.f), FLinearColor(1.f, 0.85f, 0.2f), 0.8f, true);
		}
	}
}

void AOSFHUD::DrawLockOnReticle(AOSFPlayerCharacter* Player)
{
	if (Player->LockOn->GetTarget())
	{
		const FVector Screen = Project(Player->LockOn->GetTargetLocation(), true);
		if (Screen.Z > 0.f)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.9f), Screen.X - S(5.f), Screen.Y - S(5.f), S(10.f), S(10.f));
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.9f), Screen.X - S(2.f), Screen.Y - S(2.f), S(4.f), S(4.f));
		}
	}
}

void AOSFHUD::DrawBossBar(AOSFPlayerCharacter* Player)
{
	const AOSFEnemyCharacter* Boss = Player->GetActiveBoss();
	if (!Boss || Boss->IsDead() || !Boss->IsBossFightActive())
	{
		BossTrail = 1.f;
		return;
	}
	const float Percent = Boss->Stats->GetPercent(OSFTags::Stat_Health);
	BossTrail = Percent < BossTrail ? FMath::FInterpConstantTo(BossTrail, Percent, GetWorld()->GetDeltaSeconds(), 0.3f) : Percent;
	const float Width = S(1000.f);
	const float X = Canvas->ClipX * 0.5f - Width * 0.5f;
	const float Y = Canvas->ClipY - S(150.f);
	DrawTextAt(Boss->Boss->BossName.ToString(), X, Y - S(30.f), FLinearColor(0.95f, 0.9f, 0.8f), 1.1f);
	DrawBar(X, Y, Width, S(14.f), Percent, FLinearColor(0.6f, 0.04f, 0.04f), BossTrail);
}

void AOSFHUD::DrawPrompt(AOSFPlayerCharacter* Player)
{
	const FText Prompt = Player->Interaction->GetFocusedPrompt();
	if (Prompt.IsEmpty())
	{
		return;
	}
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - S(260.f), Y - S(8.f), S(520.f), S(44.f));
	const AOSFPlayerController* PC = Cast<AOSFPlayerController>(GetOwningPlayerController());
	DrawTextAt(FString::Printf(TEXT("%s  %s"), PC && PC->IsUsingGamepad() ? TEXT("[A]") : TEXT("[E]"), *Prompt.ToString()), CX, Y, FLinearColor(1.f, 0.95f, 0.85f), 1.1f, true);
}

void AOSFHUD::DrawNotifications(AOSFPlayerController* PC)
{
	float ToastY = Canvas->ClipY * 0.35f;
	for (const FOSFNotification& Notification : PC->GetNotifications())
	{
		const float Elapsed = Notification.Duration - Notification.Remaining;
		if (ExternalNotifications.Contains(Notification.Type))
		{
			continue;
		}
		const float Alpha = FMath::Clamp(FMath::Min(Elapsed * 3.f, Notification.Remaining * 1.5f), 0.f, 1.f);
		switch (Notification.Type)
		{
		case EOSFNotifyType::Banner:
		{
			const FString Text = Notification.Text.ToString();
			const bool bDeath = Text == NSLOCTEXT("OSFPlayer", "YouDied", "YOU DIED").ToString();
			const FLinearColor Color = bDeath ? FLinearColor(0.75f, 0.05f, 0.05f, Alpha) : FLinearColor(1.f, 0.85f, 0.45f, Alpha);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha), 0.f, Canvas->ClipY * 0.42f, Canvas->ClipX, S(120.f));
			DrawTextAt(Text, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f + S(28.f), Color, 3.2f, true, GetHUDFont(true));
			break;
		}
		case EOSFNotifyType::Hint:
			DrawTextAt(Notification.Text.ToString(), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.62f, FLinearColor(1.f, 1.f, 1.f, Alpha), 1.f, true);
			break;
		case EOSFNotifyType::Toast:
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), Canvas->ClipX - S(380.f), ToastY - S(4.f), S(340.f), S(30.f));
			DrawTextAt(Notification.Text.ToString(), Canvas->ClipX - S(370.f), ToastY, FLinearColor(1.f, 0.95f, 0.8f, Alpha), 0.9f);
			ToastY += S(36.f);
			break;
		}
	}
}

void AOSFHUD::DrawMenu(AOSFPlayerController* PC)
{
	if (!PC->IsInMenu() || ExternalMenus.Contains(PC->GetCurrentMenu()))
	{
		return;
	}
	const EOSFMenu Menu = PC->GetCurrentMenu();
	const TArray<FOSFMenuEntry> Entries = PC->GetMenuEntries();
	const int32 Selection = PC->GetMenuSelection();

	if (Menu == EOSFMenu::Dialogue)
	{
		const float Width = S(1100.f);
		const float X = Canvas->ClipX * 0.5f - Width * 0.5f;
		const float Y = Canvas->ClipY - S(330.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), X, Y, Width, S(230.f));
		DrawTextAt(PC->GetMenuTitle().ToString(), X + S(30.f), Y + S(16.f), FLinearColor(1.f, 0.85f, 0.5f), 1.1f);
		DrawWrapped(PC->GetMenuDetails().ToString(), X + S(30.f), Y + S(60.f), Width - S(60.f), FLinearColor::White, 1.f);
		float OX = X + S(30.f);
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const bool bSelected = Index == Selection;
			DrawTextAt(FString::Printf(TEXT("%s%s"), bSelected ? TEXT("> ") : TEXT("  "), *Entries[Index].Label.ToString()), OX, Y + S(185.f), bSelected ? FLinearColor(1.f, 0.85f, 0.4f) : FLinearColor(0.8f, 0.8f, 0.8f), 1.f);
			OX += S(200.f);
		}
		return;
	}

	// Darken the world and draw a list panel + a details panel.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const float PanelX = S(120.f);
	const float PanelY = S(160.f);
	const float PanelW = S(620.f);
	const float RowH = S(38.f);
	const int32 MaxRows = 16;
	DrawRect(FLinearColor(0.03f, 0.03f, 0.03f, 0.85f), PanelX, PanelY, PanelW, S(110.f) + RowH * FMath::Min(MaxRows, Entries.Num()));
	DrawTextAt(PC->GetMenuTitle().ToString(), PanelX + S(24.f), PanelY + S(18.f), FLinearColor(1.f, 0.85f, 0.5f), 1.4f);
	const FText Tab = PC->GetMenuTabLabel();
	if (!Tab.IsEmpty())
	{
		DrawTextAt(Tab.ToString(), PanelX + PanelW - S(24.f) - S(9.f) * Tab.ToString().Len(), PanelY + S(24.f), FLinearColor(0.8f, 0.8f, 0.8f), 0.9f);
	}

	const int32 First = FMath::Clamp(Selection - MaxRows / 2, 0, FMath::Max(0, Entries.Num() - MaxRows));
	float Y = PanelY + S(80.f);
	for (int32 Index = First; Index < FMath::Min(Entries.Num(), First + MaxRows); ++Index)
	{
		const FOSFMenuEntry& Entry = Entries[Index];
		if (Index == Selection)
		{
			DrawRect(FLinearColor(0.6f, 0.45f, 0.2f, 0.35f), PanelX + S(10.f), Y - S(4.f), PanelW - S(20.f), RowH - S(4.f));
		}
		DrawTextAt(Entry.Label.ToString(), PanelX + S(26.f), Y, Entry.Color, 1.f);
		const FString Value = Entry.Value.ToString();
		if (!Value.IsEmpty())
		{
			float W = 0.f, H = 0.f;
			Canvas->TextSize(GetHUDFont(false), Value, W, H, Scale, Scale);
			DrawTextAt(Value, PanelX + PanelW - S(26.f) - W, Y, Entry.bEnabled ? FLinearColor(0.9f, 0.85f, 0.7f) : FLinearColor(0.45f, 0.45f, 0.45f), 1.f);
		}
		Y += RowH;
	}

	const FString Details = PC->GetMenuDetails().ToString();
	if (!Details.IsEmpty())
	{
		const float DX = PanelX + PanelW + S(40.f);
		const float DW = S(560.f);
		DrawRect(FLinearColor(0.03f, 0.03f, 0.03f, 0.85f), DX, PanelY, DW, S(480.f));
		DrawWrapped(Details, DX + S(24.f), PanelY + S(24.f), DW - S(48.f), FLinearColor(0.9f, 0.9f, 0.9f), 0.95f);
	}
	const FText Keys = PC->IsUsingGamepad()
		? NSLOCTEXT("OSFHUD", "MenuKeysPad", "[D-pad] select / adjust   [A] confirm   [B] back")
		: NSLOCTEXT("OSFHUD", "MenuKeys", "[W/S] select   [A/D] adjust   [Enter/E] confirm   [Backspace] back");
	DrawTextAt(Keys.ToString(), PanelX, Canvas->ClipY - S(60.f), FLinearColor(0.7f, 0.7f, 0.7f), 0.8f);
}

void AOSFHUD::DrawControls()
{
	// Counts only time the player can actually read it (no title, intro or cutscene in the way).
	if (!CanShowControlsHint() || ControlsHintShown > ControlsHintSeconds)
	{
		return;
	}
	ControlsHintShown += GetWorld()->GetDeltaSeconds();
	const float Alpha = FMath::Clamp((ControlsHintSeconds - ControlsHintShown) / 3.f, 0.f, 1.f);
	const AOSFPlayerController* PC = Cast<AOSFPlayerController>(GetOwningPlayerController());
	const FString Text = (PC && PC->IsUsingGamepad() && !ControlsHintGamepad.IsEmpty() ? ControlsHintGamepad : ControlsHint).ToString();
	const float Height = S(30.f) * ControlsHintScale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), 0.f, Canvas->ClipY - S(16.f) - Height, Canvas->ClipX, Height + S(8.f));
	DrawTextAt(Text, Canvas->ClipX * 0.5f, Canvas->ClipY - S(14.f) - Height, FLinearColor(0.92f, 0.9f, 0.85f, Alpha), ControlsHintScale, true);
}
