// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "OSFProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnProgressChanged, FGameplayTag, ProgressTag, bool, bAdded);

/**
 * Tag based progression. Story flags, defeated bosses, discovered resting points, NPC quest steps...
 * are all gameplay tags (OSF.Progress.*). Dialogue, vendors, recipes and doors can require / block on them.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFProgressComponent();

	UPROPERTY(BlueprintAssignable, Category = "OSF|Progress")
	FOSFOnProgressChanged OnProgressChanged;

	UFUNCTION(BlueprintCallable, Category = "OSF|Progress")
	void AddProgress(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "OSF|Progress")
	void RemoveProgress(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	bool HasProgress(FGameplayTag Tag) const { return Tag.IsValid() && ProgressTags.HasTagExact(Tag); }

	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	bool HasAllProgress(const FGameplayTagContainer& Tags) const { return ProgressTags.HasAllExact(Tags); }

	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	bool HasAnyProgress(const FGameplayTagContainer& Tags) const { return ProgressTags.HasAnyExact(Tags); }

	/** Generic requirement check: all of Required, none of Blocked. */
	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	bool MeetsRequirements(const FGameplayTagContainer& Required, const FGameplayTagContainer& Blocked) const;

	/** Progress flags can also be arbitrary names (tags not declared in config), e.g. "Door.CastleGate". */
	UFUNCTION(BlueprintCallable, Category = "OSF|Progress")
	void AddFlag(FName Flag) { if (!Flag.IsNone()) { Flags.Add(Flag); } }

	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }

	UFUNCTION(BlueprintCallable, Category = "OSF|Progress")
	void RemoveFlag(FName Flag) { Flags.Remove(Flag); }

	/** Generic persistent counters (vendor stock bought, NPC visit counts...). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Progress")
	int32 AddCounter(FName Key, int32 Delta = 1) { return Counters.FindOrAdd(Key) += Delta; }

	UFUNCTION(BlueprintPure, Category = "OSF|Progress")
	int32 GetCounter(FName Key) const { const int32* Value = Counters.Find(Key); return Value ? *Value : 0; }

	const FGameplayTagContainer& GetProgressTags() const { return ProgressTags; }
	const TSet<FName>& GetFlags() const { return Flags; }
	const TMap<FName, int32>& GetCounters() const { return Counters; }
	void SetRaw(const FGameplayTagContainer& InTags, const TSet<FName>& InFlags, const TMap<FName, int32>& InCounters) { ProgressTags = InTags; Flags = InFlags; Counters = InCounters; }

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Progress")
	TMap<FName, int32> Counters;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Progress")
	FGameplayTagContainer ProgressTags;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Progress")
	TSet<FName> Flags;
};
