// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "OSFItemRegistry.generated.h"

class UOSFItemData;

/**
 * Resolves items by ItemId. Items can be data assets (found through the asset registry) or
 * created at runtime (e.g. procedurally generated loot) and registered explicitly.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFItemRegistry : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UOSFItemRegistry* Get();

	UFUNCTION(BlueprintCallable, Category = "OSF|Items")
	void RegisterItem(UOSFItemData* Item);

	UFUNCTION(BlueprintCallable, Category = "OSF|Items")
	UOSFItemData* FindItem(FName ItemId);

	/** Convenience static wrapper usable from anywhere (null-safe). */
	static UOSFItemData* Find(FName ItemId);

private:
	UPROPERTY()
	TMap<FName, TObjectPtr<UOSFItemData>> Items;
};
