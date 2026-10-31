// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFInventoryComponent.generated.h"

class UOSFItemData;
class UOSFCraftingRecipe;

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF")
	int32 Count = 0;

	/** Flasks only: charges restored when resting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF")
	int32 MaxCharges = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnInventoryChanged, UOSFInventoryComponent*, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSFOnItemAdded, UOSFInventoryComponent*, Inventory, UOSFItemData*, Item, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSFOnCurrencyChanged, UOSFInventoryComponent*, Inventory, int32, NewAmount, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnItemUsed, UOSFInventoryComponent*, Inventory, UOSFItemData*, Item);

/** Inventory manager: carried items, storage chest, currency, flasks and crafting. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFInventoryComponent();

	virtual void BeginPlay() override;

	/** Items granted at BeginPlay (first spawn only - save games override). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Inventory")
	TArray<FOSFItemStack> StartingItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Inventory")
	int32 StartingCurrency = 0;

	/** Recipes this character knows (subject to their RequiredProgress). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Inventory")
	TArray<TObjectPtr<UOSFCraftingRecipe>> KnownRecipes;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Inventory")
	FOSFOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Inventory")
	FOSFOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Inventory")
	FOSFOnCurrencyChanged OnCurrencyChanged;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Inventory")
	FOSFOnItemUsed OnItemUsed;

	// --- Items ---

	/** Adds items; overflow beyond MaxStack goes to storage. Returns how many were added to the carried inventory. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	int32 AddItem(UOSFItemData* Item, int32 Count = 1, bool bNotify = true);

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool RemoveItem(UOSFItemData* Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	int32 GetItemCount(const UOSFItemData* Item) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	bool HasItem(const UOSFItemData* Item, int32 Count = 1) const { return GetItemCount(Item) >= Count; }

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	bool HasItemWithId(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	const TArray<FOSFInventoryEntry>& GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	TArray<FOSFInventoryEntry> GetItemsByCategory(EOSFItemCategory Category) const;

	/** Applies the item's use effects to the owner and consumes it. Prefer the UseItem action for animated use. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool ConsumeItem(UOSFItemData* Item);

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	bool CanUseItem(const UOSFItemData* Item) const;

	/** Applies the effects of Item to Target (no consumption). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	static void ApplyItemEffects(const UOSFItemData* Item, AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	void RefillFlasks();

	/** Increases a flask's max charges (flask upgrades). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	void AddFlaskCharge(UOSFItemData* Flask, int32 Amount = 1);

	// --- Storage ---

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool DepositToStorage(UOSFItemData* Item, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool WithdrawFromStorage(UOSFItemData* Item, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	const TArray<FOSFInventoryEntry>& GetStorage() const { return Storage; }

	// --- Currency ---

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	int32 GetCurrency() const { return Currency; }

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	void AddCurrency(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool SpendCurrency(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	void SetCurrency(int32 Amount);

	// --- Crafting ---

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	bool IsRecipeKnown(const UOSFCraftingRecipe* Recipe) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Inventory")
	bool CanCraft(const UOSFCraftingRecipe* Recipe, int32 Times = 1) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Inventory")
	bool Craft(UOSFCraftingRecipe* Recipe, int32 Times = 1);

	// --- Save support ---
	void SetRawContents(const TArray<FOSFInventoryEntry>& InItems, const TArray<FOSFInventoryEntry>& InStorage, int32 InCurrency);
	void MarkStartingItemsGranted() { bStartingItemsGranted = true; }

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Inventory")
	TArray<FOSFInventoryEntry> Items;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Inventory")
	TArray<FOSFInventoryEntry> Storage;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Inventory")
	int32 Currency = 0;

	bool bStartingItemsGranted = false;

	static FOSFInventoryEntry* FindEntry(TArray<FOSFInventoryEntry>& List, const UOSFItemData* Item);
	static const FOSFInventoryEntry* FindEntry(const TArray<FOSFInventoryEntry>& List, const UOSFItemData* Item);
	void Changed();
};
