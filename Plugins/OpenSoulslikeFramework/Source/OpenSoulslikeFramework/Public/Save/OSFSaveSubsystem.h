// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OSFSaveSubsystem.generated.h"

class UOSFSaveGame;
class AOSFPlayerCharacter;

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFSaveSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") bool bExists = false;
	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") int32 Level = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") int32 Currency = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") FString MapName;
	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") FDateTime Timestamp;
	UPROPERTY(BlueprintReadOnly, Category = "OSF|Save") float PlayTime = 0.f;
};

/** Slot based saving & loading. Autosaves to the active slot when resting. */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UOSFSaveSubsystem* Get(const UObject* WorldContext);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Save")
	int32 NumSlots = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Save")
	FString SlotPrefix = TEXT("OSF_Slot");

	UPROPERTY(BlueprintReadWrite, Category = "OSF|Save")
	int32 ActiveSlot = 0;

	UFUNCTION(BlueprintCallable, Category = "OSF|Save")
	bool SaveToSlot(AOSFPlayerCharacter* Player, int32 Slot);

	/** Loads a slot: applied immediately if the map matches, otherwise after travelling to the saved map. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Save")
	bool LoadFromSlot(UObject* WorldContext, int32 Slot);

	UFUNCTION(BlueprintCallable, Category = "OSF|Save")
	bool DeleteSlot(int32 Slot);

	UFUNCTION(BlueprintPure, Category = "OSF|Save")
	FOSFSaveSummary GetSlotSummary(int32 Slot) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Save")
	void AutoSave(AOSFPlayerCharacter* Player);

	/** Called by the game mode when the player pawn spawns. */
	void ApplyPendingLoad(AOSFPlayerCharacter* Player);

	bool HasPendingLoad() const { return PendingLoad != nullptr; }

	/** Serialization helpers (public for tests / custom flows). */
	static void CapturePlayer(AOSFPlayerCharacter* Player, UOSFSaveGame* Save);
	static void ApplyToPlayer(AOSFPlayerCharacter* Player, const UOSFSaveGame* Save, bool bTeleport = true);

	FString GetSlotName(int32 Slot) const { return FString::Printf(TEXT("%s%d"), *SlotPrefix, Slot); }

protected:
	UPROPERTY(Transient)
	TObjectPtr<UOSFSaveGame> PendingLoad;

	/** Summaries are cached because the menu queries them every frame. */
	mutable TMap<int32, FOSFSaveSummary> SummaryCache;
};
