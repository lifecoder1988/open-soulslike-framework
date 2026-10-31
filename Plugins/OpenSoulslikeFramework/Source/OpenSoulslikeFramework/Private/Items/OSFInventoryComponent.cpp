// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFItemRegistry.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Progression/OSFProgressComponent.h"
#include "OSFGameplayTags.h"

UOSFInventoryComponent::UOSFInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSFInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bStartingItemsGranted)
	{
		bStartingItemsGranted = true;
		for (const FOSFItemStack& Stack : StartingItems)
		{
			AddItem(Stack.Item, Stack.Count, false);
		}
		Currency += StartingCurrency;
	}
	for (const FOSFInventoryEntry& Entry : Items)
	{
		if (UOSFItemRegistry* Registry = UOSFItemRegistry::Get())
		{
			Registry->RegisterItem(Entry.Item);
		}
	}
}

FOSFInventoryEntry* UOSFInventoryComponent::FindEntry(TArray<FOSFInventoryEntry>& List, const UOSFItemData* Item)
{
	return List.FindByPredicate([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
}

const FOSFInventoryEntry* UOSFInventoryComponent::FindEntry(const TArray<FOSFInventoryEntry>& List, const UOSFItemData* Item)
{
	return List.FindByPredicate([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
}

void UOSFInventoryComponent::Changed()
{
	OnInventoryChanged.Broadcast(this);
}

int32 UOSFInventoryComponent::AddItem(UOSFItemData* Item, int32 Count, bool bNotify)
{
	if (!Item || Count <= 0)
	{
		return 0;
	}
	if (UOSFItemRegistry* Registry = UOSFItemRegistry::Get())
	{
		Registry->RegisterItem(Item);
	}

	FOSFInventoryEntry* Entry = FindEntry(Items, Item);
	if (!Entry)
	{
		Entry = &Items.AddDefaulted_GetRef();
		Entry->Item = Item;
		Entry->Count = 0;
	}

	if (Item->bIsFlask)
	{
		// Picking up a flask grants a charge slot.
		Entry->MaxCharges += Count;
		Entry->Count += Count;
		if (bNotify) { OnItemAdded.Broadcast(this, Item, Count); }
		Changed();
		return Count;
	}

	const int32 Space = FMath::Max(0, Item->MaxStack - Entry->Count);
	const int32 ToCarry = FMath::Min(Space, Count);
	Entry->Count += ToCarry;
	if (Entry->Count <= 0)
	{
		Items.RemoveAll([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
	}

	const int32 Overflow = Count - ToCarry;
	if (Overflow > 0)
	{
		FOSFInventoryEntry* StorageEntry = FindEntry(Storage, Item);
		if (!StorageEntry)
		{
			StorageEntry = &Storage.AddDefaulted_GetRef();
			StorageEntry->Item = Item;
		}
		StorageEntry->Count += Overflow;
	}

	if (bNotify) { OnItemAdded.Broadcast(this, Item, Count); }
	Changed();
	return ToCarry;
}

bool UOSFInventoryComponent::RemoveItem(UOSFItemData* Item, int32 Count)
{
	FOSFInventoryEntry* Entry = FindEntry(Items, Item);
	if (!Entry || Entry->Count < Count || Count <= 0)
	{
		return false;
	}
	Entry->Count -= Count;
	if (Entry->Count <= 0 && !(Item && Item->bIsFlask))
	{
		Items.RemoveAll([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
	}
	Changed();
	return true;
}

int32 UOSFInventoryComponent::GetItemCount(const UOSFItemData* Item) const
{
	const FOSFInventoryEntry* Entry = FindEntry(Items, Item);
	return Entry ? Entry->Count : 0;
}

bool UOSFInventoryComponent::HasItemWithId(FName ItemId) const
{
	return Items.ContainsByPredicate([ItemId](const FOSFInventoryEntry& E) { return E.Item && E.Item->ItemId == ItemId && E.Count > 0; });
}

TArray<FOSFInventoryEntry> UOSFInventoryComponent::GetItemsByCategory(EOSFItemCategory Category) const
{
	return Items.FilterByPredicate([Category](const FOSFInventoryEntry& E) { return E.Item && E.Item->Category == Category; });
}

bool UOSFInventoryComponent::CanUseItem(const UOSFItemData* Item) const
{
	return Item && Item->IsUsable() && GetItemCount(Item) > 0;
}

bool UOSFInventoryComponent::ConsumeItem(UOSFItemData* Item)
{
	if (!CanUseItem(Item))
	{
		return false;
	}
	ApplyItemEffects(Item, GetOwner());
	if (Item->bConsumeOnUse)
	{
		RemoveItem(Item, 1);
	}
	OnItemUsed.Broadcast(this, Item);
	return true;
}

void UOSFInventoryComponent::ApplyItemEffects(const UOSFItemData* Item, AActor* Target)
{
	if (!Item || !Target)
	{
		return;
	}
	UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(Target);
	for (const FOSFItemEffect& Effect : Item->UseEffects)
	{
		switch (Effect.Type)
		{
		case EOSFItemEffectType::RestoreStat:
			if (Stats)
			{
				const float Amount = Effect.bPercent ? Stats->GetMax(Effect.Tag) * Effect.Magnitude : Effect.Magnitude;
				Stats->ModifyCurrent(Effect.Tag, Amount);
			}
			break;
		case EOSFItemEffectType::ApplyBuff:
			if (UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(Target))
			{
				Buffs->ApplyBuff(Effect.Buff, Effect.Magnitude);
			}
			break;
		case EOSFItemEffectType::CureStatus:
			if (UOSFStatusEffectComponent* Status = OSF::Get<UOSFStatusEffectComponent>(Target))
			{
				if (Effect.Tag.IsValid()) { Status->Cure(Effect.Tag); }
				else { Status->CureAll(); }
			}
			break;
		case EOSFItemEffectType::GrantCurrency:
			if (UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(Target))
			{
				Inventory->AddCurrency(FMath::RoundToInt(Effect.Magnitude));
			}
			break;
		case EOSFItemEffectType::AddBuildup:
			if (UOSFStatusEffectComponent* Status = OSF::Get<UOSFStatusEffectComponent>(Target))
			{
				Status->AddBuildup(Effect.Tag, Effect.Magnitude);
			}
			break;
		case EOSFItemEffectType::GrantProgress:
			if (UOSFProgressComponent* Progress = OSF::Get<UOSFProgressComponent>(Target))
			{
				Progress->AddProgress(Effect.Tag);
			}
			break;
		}
	}
}

void UOSFInventoryComponent::RefillFlasks()
{
	for (FOSFInventoryEntry& Entry : Items)
	{
		if (Entry.Item && Entry.Item->bIsFlask)
		{
			Entry.Count = Entry.MaxCharges;
		}
	}
	Changed();
}

void UOSFInventoryComponent::AddFlaskCharge(UOSFItemData* Flask, int32 Amount)
{
	if (FOSFInventoryEntry* Entry = FindEntry(Items, Flask))
	{
		Entry->MaxCharges += Amount;
		Entry->Count += Amount;
		Changed();
	}
}

bool UOSFInventoryComponent::DepositToStorage(UOSFItemData* Item, int32 Count)
{
	if (!Item || Item->bIsFlask || Item->Category == EOSFItemCategory::KeyItem || !RemoveItem(Item, Count))
	{
		return false;
	}
	FOSFInventoryEntry* Entry = FindEntry(Storage, Item);
	if (!Entry)
	{
		Entry = &Storage.AddDefaulted_GetRef();
		Entry->Item = Item;
	}
	Entry->Count += Count;
	Changed();
	return true;
}

bool UOSFInventoryComponent::WithdrawFromStorage(UOSFItemData* Item, int32 Count)
{
	FOSFInventoryEntry* Entry = FindEntry(Storage, Item);
	if (!Entry || Entry->Count < Count || Count <= 0)
	{
		return false;
	}
	const int32 Carried = GetItemCount(Item);
	const int32 Space = Item->MaxStack - Carried;
	const int32 ToMove = FMath::Min(Space, Count);
	if (ToMove <= 0)
	{
		return false;
	}
	Entry->Count -= ToMove;
	if (Entry->Count <= 0)
	{
		Storage.RemoveAll([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
	}
	AddItem(Item, ToMove, false);
	return true;
}

void UOSFInventoryComponent::AddCurrency(int32 Amount)
{
	if (Amount == 0)
	{
		return;
	}
	const int32 Old = Currency;
	Currency = FMath::Clamp<int64>(static_cast<int64>(Currency) + Amount, 0, 999999999);
	OnCurrencyChanged.Broadcast(this, Currency, Currency - Old);
}

bool UOSFInventoryComponent::SpendCurrency(int32 Amount)
{
	if (Amount < 0 || Currency < Amount)
	{
		return false;
	}
	AddCurrency(-Amount);
	return true;
}

void UOSFInventoryComponent::SetCurrency(int32 Amount)
{
	AddCurrency(Amount - Currency);
}

bool UOSFInventoryComponent::IsRecipeKnown(const UOSFCraftingRecipe* Recipe) const
{
	if (!Recipe)
	{
		return false;
	}
	if (Recipe->RequiredProgress.IsEmpty())
	{
		return true;
	}
	const UOSFProgressComponent* Progress = OSF::Get<UOSFProgressComponent>(GetOwner());
	return Progress && Progress->HasAllProgress(Recipe->RequiredProgress);
}

bool UOSFInventoryComponent::CanCraft(const UOSFCraftingRecipe* Recipe, int32 Times) const
{
	if (!IsRecipeKnown(Recipe) || !Recipe->Result.Item || Times <= 0)
	{
		return false;
	}
	for (const FOSFItemStack& Ingredient : Recipe->Ingredients)
	{
		if (GetItemCount(Ingredient.Item) < Ingredient.Count * Times)
		{
			return false;
		}
	}
	return true;
}

bool UOSFInventoryComponent::Craft(UOSFCraftingRecipe* Recipe, int32 Times)
{
	if (!CanCraft(Recipe, Times))
	{
		return false;
	}
	for (const FOSFItemStack& Ingredient : Recipe->Ingredients)
	{
		RemoveItem(Ingredient.Item, Ingredient.Count * Times);
	}
	AddItem(Recipe->Result.Item, Recipe->Result.Count * Times);
	return true;
}

void UOSFInventoryComponent::SetRawContents(const TArray<FOSFInventoryEntry>& InItems, const TArray<FOSFInventoryEntry>& InStorage, int32 InCurrency)
{
	bStartingItemsGranted = true;
	Items = InItems;
	Storage = InStorage;
	const int32 Old = Currency;
	Currency = InCurrency;
	OnCurrencyChanged.Broadcast(this, Currency, Currency - Old);
	Changed();
}
