// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "OSFSaveGame.generated.h"

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFSavedItem
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FName ItemId;
	UPROPERTY(SaveGame) int32 Count = 0;
	UPROPERTY(SaveGame) int32 MaxCharges = 0;
};

/** Everything persisted for one save slot. */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) FString MapName;
	UPROPERTY(SaveGame) FDateTime Timestamp;
	UPROPERTY(SaveGame) float PlayTime = 0.f;

	// Player
	UPROPERTY(SaveGame) FTransform PlayerTransform;
	UPROPERTY(SaveGame) FTransform RespawnTransform;
	UPROPERTY(SaveGame) FName LastRestingPoint;
	UPROPERTY(SaveGame) TMap<FGameplayTag, int32> Attributes;
	UPROPERTY(SaveGame) int32 Level = 1;
	UPROPERTY(SaveGame) int32 Currency = 0;

	// Items
	UPROPERTY(SaveGame) TArray<FOSFSavedItem> Inventory;
	UPROPERTY(SaveGame) TArray<FOSFSavedItem> Storage;
	UPROPERTY(SaveGame) TArray<FName> RightHand;
	UPROPERTY(SaveGame) TArray<FName> LeftHand;
	UPROPERTY(SaveGame) TArray<FName> Armor;
	UPROPERTY(SaveGame) TArray<FName> Talismans;
	UPROPERTY(SaveGame) TArray<FName> QuickItems;
	UPROPERTY(SaveGame) int32 ActiveRight = 0;
	UPROPERTY(SaveGame) int32 ActiveLeft = 0;
	UPROPERTY(SaveGame) int32 ActiveQuick = 0;
	UPROPERTY(SaveGame) bool bTwoHanding = false;

	// Progression / world
	UPROPERTY(SaveGame) FGameplayTagContainer ProgressTags;
	UPROPERTY(SaveGame) TSet<FName> ProgressFlags;
	UPROPERTY(SaveGame) TMap<FName, int32> ProgressCounters;
	UPROPERTY(SaveGame) bool bHasBloodstain = false;
	UPROPERTY(SaveGame) FVector BloodstainLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame) int32 BloodstainCurrency = 0;
	UPROPERTY(SaveGame) float TimeOfDay = 12.f;
};
