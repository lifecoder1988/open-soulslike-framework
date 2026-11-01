// Copyright Open Soulslike Framework contributors. MIT License.

#include "Game/OSFPlayerController.h"
#include "Game/OSFGameMode.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFCombatLibrary.h"
#include "Combat/OSFLockOnComponent.h"
#include "Combat/OSFWeaponTraceComponent.h"
#include "Interaction/OSFNPC.h"
#include "Interaction/OSFInteractionComponent.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFItemRegistry.h"
#include "Progression/OSFProgressComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Save/OSFSaveSubsystem.h"
#include "World/OSFTimeOfDay.h"
#include "AI/OSFEnemyCharacter.h"
#include "OSFGameplayTags.h"
#include "OpenSoulslikeFramework.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "OSFUI"

namespace
{
	struct FEquipRow
	{
		EOSFEquipSlot Slot;
		int32 Index;
	};

	TArray<FEquipRow> GetEquipRows()
	{
		TArray<FEquipRow> Rows;
		for (int32 I = 0; I < UOSFEquipmentComponent::NumHandSlots; ++I) { Rows.Add({ EOSFEquipSlot::RightHand, I }); }
		for (int32 I = 0; I < UOSFEquipmentComponent::NumHandSlots; ++I) { Rows.Add({ EOSFEquipSlot::LeftHand, I }); }
		Rows.Add({ EOSFEquipSlot::Head, 0 });
		Rows.Add({ EOSFEquipSlot::Chest, 0 });
		Rows.Add({ EOSFEquipSlot::Arms, 0 });
		Rows.Add({ EOSFEquipSlot::Legs, 0 });
		for (int32 I = 0; I < UOSFEquipmentComponent::NumTalismanSlots; ++I) { Rows.Add({ EOSFEquipSlot::Talisman, I }); }
		for (int32 I = 0; I < UOSFEquipmentComponent::NumQuickSlots; ++I) { Rows.Add({ EOSFEquipSlot::QuickItem, I }); }
		return Rows;
	}

	FText SlotName(const FEquipRow& Row)
	{
		switch (Row.Slot)
		{
		case EOSFEquipSlot::RightHand: return FText::Format(LOCTEXT("RightHand", "Right Hand {0}"), FText::AsNumber(Row.Index + 1));
		case EOSFEquipSlot::LeftHand: return FText::Format(LOCTEXT("LeftHand", "Left Hand {0}"), FText::AsNumber(Row.Index + 1));
		case EOSFEquipSlot::Head: return LOCTEXT("Head", "Head");
		case EOSFEquipSlot::Chest: return LOCTEXT("Chest", "Chest");
		case EOSFEquipSlot::Arms: return LOCTEXT("Arms", "Arms");
		case EOSFEquipSlot::Legs: return LOCTEXT("Legs", "Legs");
		case EOSFEquipSlot::Talisman: return FText::Format(LOCTEXT("Talisman", "Talisman {0}"), FText::AsNumber(Row.Index + 1));
		case EOSFEquipSlot::QuickItem: return FText::Format(LOCTEXT("Quick", "Quick Item {0}"), FText::AsNumber(Row.Index + 1));
		default: return FText::GetEmpty();
		}
	}

	UOSFItemData* GetSlotItem(const UOSFEquipmentComponent* Equipment, const FEquipRow& Row)
	{
		switch (Row.Slot)
		{
		case EOSFEquipSlot::RightHand: return Equipment->RightHand.IsValidIndex(Row.Index) ? Equipment->RightHand[Row.Index].Get() : nullptr;
		case EOSFEquipSlot::LeftHand: return Equipment->LeftHand.IsValidIndex(Row.Index) ? Equipment->LeftHand[Row.Index].Get() : nullptr;
		case EOSFEquipSlot::Talisman: return Equipment->Talismans.IsValidIndex(Row.Index) ? Equipment->Talismans[Row.Index].Get() : nullptr;
		case EOSFEquipSlot::QuickItem: return Equipment->QuickItems.IsValidIndex(Row.Index) ? Equipment->QuickItems[Row.Index].Get() : nullptr;
		default:
		{
			const TObjectPtr<UOSFArmorData>* Piece = Equipment->Armor.Find(Row.Slot);
			return Piece ? Piece->Get() : nullptr;
		}
		}
	}

	FText AttributeName(const FGameplayTag& Tag)
	{
		FString Name = Tag.GetTagName().ToString();
		Name.RemoveFromStart(TEXT("OSF.Attribute."));
		return FText::FromString(Name);
	}
}

AOSFPlayerController::AOSFPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AOSFPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AOSFPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 Index = Notifications.Num() - 1; Index >= 0; --Index)
	{
		Notifications[Index].Remaining -= DeltaSeconds;
		if (Notifications[Index].Remaining <= 0.f)
		{
			Notifications.RemoveAt(Index);
		}
	}
	// Leave dialogue / vendor menus when walking away or dying.
	if (IsInMenu() && GetOSFPlayer() && GetOSFPlayer()->IsDead())
	{
		CloseAllMenus();
	}
}

void AOSFPlayerController::PushNotification(const FText& Text, EOSFNotifyType Type)
{
	FOSFNotification& Notification = Notifications.AddDefaulted_GetRef();
	Notification.Text = Text;
	Notification.Type = Type;
	Notification.Duration = Type == EOSFNotifyType::Banner ? 4.f : (Type == EOSFNotifyType::Hint ? 2.f : 3.5f);
	Notification.Remaining = Notification.Duration;
	if (Type == EOSFNotifyType::Banner)
	{
		Notifications.RemoveAll([&Notification](const FOSFNotification& Other) { return Other.Type == EOSFNotifyType::Banner && &Other != &Notification; });
	}
	UE_LOG(LogOSF, Log, TEXT("[Notify] %s"), *Text.ToString());
}

// --- Menu stack -----------------------------------------------------------------------

void AOSFPlayerController::OnMenusChanged()
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer())
	{
		if (IsInMenu())
		{
			if (!Hero->HasStateTag(OSFTags::State_InMenu))
			{
				Hero->ActionComponent->AddStateTag(OSFTags::State_InMenu);
			}
			Hero->SetSprinting(false);
			Hero->ActionComponent->StopAction(OSFTags::Action_Guard);
		}
		else
		{
			Hero->ActionComponent->ClearStateTag(OSFTags::State_InMenu);
			Hero->StopResting();
		}
	}
	OnMenuChanged.Broadcast(GetCurrentMenu());
}

void AOSFPlayerController::OpenMenu(EOSFMenu Menu)
{
	if (Menu == EOSFMenu::None)
	{
		return;
	}
	FOSFMenuFrame& Frame = MenuStack.AddDefaulted_GetRef();
	Frame.Menu = Menu;
	if (Menu == EOSFMenu::LevelUp)
	{
		PendingLevelUp.Reset();
	}
	OnMenusChanged();
}

void AOSFPlayerController::CloseMenu()
{
	if (MenuStack.Num() > 0)
	{
		if (MenuStack.Last().Menu == EOSFMenu::Dialogue && !bDialogueFinished && DialogueNPC.IsValid())
		{
			// Leaving mid-conversation still counts as having heard it.
			const FOSFDialogEntry* Entry = DialogueNPC->GetCurrentDialog(GetOSFPlayer() ? GetOSFPlayer()->Progress : nullptr);
			if (Entry) { DialogueNPC->CompleteDialog(GetOSFPlayer(), *Entry); }
			bDialogueFinished = true;
		}
		MenuStack.Pop();
	}
	OnMenusChanged();
}

void AOSFPlayerController::CloseAllMenus()
{
	while (MenuStack.Num() > 0)
	{
		CloseMenu();
	}
}

void AOSFPlayerController::ToggleSystemMenu()
{
	if (IsInMenu())
	{
		MenuBack();
	}
	else if (GetOSFPlayer() && !GetOSFPlayer()->IsDead())
	{
		OpenMenu(EOSFMenu::System);
	}
}

void AOSFPlayerController::OpenRestMenu(AOSFRestingPoint* RestingPoint)
{
	CurrentRestingPoint = RestingPoint;
	CloseAllMenus();
	OpenMenu(EOSFMenu::Rest);
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer())
	{
		Hero->ActionComponent->AddStateTag(OSFTags::State_Resting);
	}
}

void AOSFPlayerController::OpenDialogue(AOSFNPC* NPC)
{
	DialogueNPC = NPC;
	DialogueLine = 0;
	bDialogueFinished = false;
	const FOSFDialogEntry* Entry = NPC ? NPC->GetCurrentDialog(GetOSFPlayer() ? GetOSFPlayer()->Progress : nullptr) : nullptr;
	DialogueEntryIndex = Entry && NPC->Dialog ? NPC->Dialog->Entries.IndexOfByPredicate([Entry](const FOSFDialogEntry& E) { return &E == Entry; }) : INDEX_NONE;
	if (!Entry || Entry->Lines.Num() == 0)
	{
		bDialogueFinished = true;
	}
	CloseAllMenus();
	OpenMenu(EOSFMenu::Dialogue);
}

int32 AOSFPlayerController::GetEntryCount() const
{
	return GetMenuEntries().Num();
}

void AOSFPlayerController::MenuNavigate(int32 DeltaX, int32 DeltaY)
{
	if (MenuStack.Num() == 0)
	{
		return;
	}
	FOSFMenuFrame& Frame = MenuStack.Last();
	const int32 Count = GetEntryCount();
	if (DeltaY != 0 && Count > 0)
	{
		Frame.Selection = (Frame.Selection + DeltaY + Count) % Count;
	}
	if (DeltaX == 0)
	{
		return;
	}

	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	switch (Frame.Menu)
	{
	case EOSFMenu::LevelUp:
	{
		const TArray<FGameplayTag> Attributes = GetLevelAttributes();
		if (Hero && Attributes.IsValidIndex(Frame.Selection))
		{
			int32& Pending = PendingLevelUp.FindOrAdd(Attributes[Frame.Selection]);
			int32 Total = 0;
			for (const TPair<FGameplayTag, int32>& Pair : PendingLevelUp) { Total += Pair.Value; }
			if (DeltaX > 0 && Hero->Stats->GetCostForPoints(Total + 1) <= Hero->Inventory->GetCurrency()
				&& Hero->Stats->GetBaseAttribute(Attributes[Frame.Selection]) + Pending < 99)
			{
				++Pending;
			}
			else if (DeltaX < 0 && Pending > 0)
			{
				--Pending;
			}
		}
		break;
	}
	case EOSFMenu::Storage:
	case EOSFMenu::Vendor:
		Frame.Tab = 1 - Frame.Tab;
		Frame.Selection = 0;
		break;
	case EOSFMenu::Inventory:
	{
		// Right on a consumable: register it as a quick item.
		TArray<UOSFItemData*> Items;
		GetInventoryList(Items);
		if (Hero && DeltaX > 0 && Items.IsValidIndex(Frame.Selection) && Items[Frame.Selection]->IsUsable())
		{
			Hero->Equipment->EquipItemAuto(Items[Frame.Selection]);
			PushNotification(FText::Format(LOCTEXT("QuickSet", "{0} registered to quick items"), Items[Frame.Selection]->DisplayName), EOSFNotifyType::Hint);
		}
		break;
	}
	default:
		break;
	}
}

void AOSFPlayerController::MenuBack()
{
	if (MenuStack.Num() == 0)
	{
		return;
	}
	CloseMenu();
}

void AOSFPlayerController::MenuConfirm()
{
	if (MenuStack.Num() == 0)
	{
		return;
	}
	FOSFMenuFrame& Frame = MenuStack.Last();
	const TArray<FOSFMenuEntry> Entries = GetMenuEntries();
	if (!Entries.IsValidIndex(Frame.Selection) || !Entries[Frame.Selection].bEnabled)
	{
		return;
	}
	ConfirmEntry(Frame, Frame.Selection);
}

TArray<FGameplayTag> AOSFPlayerController::GetLevelAttributes()
{
	return {
		OSFTags::Attribute_Vigor, OSFTags::Attribute_Mind, OSFTags::Attribute_Endurance, OSFTags::Attribute_Strength,
		OSFTags::Attribute_Dexterity, OSFTags::Attribute_Intelligence, OSFTags::Attribute_Faith, OSFTags::Attribute_Arcane
	};
}

void AOSFPlayerController::GetInventoryList(TArray<UOSFItemData*>& Out, bool bStorage) const
{
	Out.Reset();
	const AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (!Hero)
	{
		return;
	}
	const TArray<FOSFInventoryEntry>& List = bStorage ? Hero->Inventory->GetStorage() : Hero->Inventory->GetItems();
	for (const FOSFInventoryEntry& Entry : List)
	{
		if (Entry.Item && (Entry.Count > 0 || Entry.Item->bIsFlask))
		{
			Out.Add(Entry.Item);
		}
	}
	Out.Sort([](const UOSFItemData& A, const UOSFItemData& B)
	{
		return A.Category != B.Category ? A.Category < B.Category : A.DisplayName.CompareTo(B.DisplayName) < 0;
	});
}

void AOSFPlayerController::GetEquipCandidates(EOSFEquipSlot Slot, TArray<UOSFItemData*>& Out) const
{
	TArray<UOSFItemData*> Items;
	GetInventoryList(Items);
	for (UOSFItemData* Item : Items)
	{
		bool bMatch = false;
		switch (Slot)
		{
		case EOSFEquipSlot::RightHand:
		case EOSFEquipSlot::LeftHand:
			bMatch = Item->IsA<UOSFWeaponData>();
			break;
		case EOSFEquipSlot::Talisman:
			bMatch = Item->IsA<UOSFTalismanData>();
			break;
		case EOSFEquipSlot::QuickItem:
			bMatch = Item->IsUsable();
			break;
		default:
			bMatch = Item->IsA<UOSFArmorData>() && Cast<UOSFArmorData>(Item)->Slot == Slot;
			break;
		}
		if (bMatch)
		{
			Out.Add(Item);
		}
	}
}

FText AOSFPlayerController::DescribeItem(const UOSFItemData* Item) const
{
	if (!Item)
	{
		return FText::GetEmpty();
	}
	FString Text = Item->Description.ToString();
	const AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (const UOSFWeaponData* Weapon = Cast<UOSFWeaponData>(Item))
	{
		Text += TEXT("\n");
		for (const TPair<FGameplayTag, float>& Pair : UOSFCombatLibrary::GetAttackRating(Hero, Weapon, false))
		{
			FString Type = Pair.Key.GetTagName().ToString();
			Type.RemoveFromStart(TEXT("OSF.Damage."));
			Text += FString::Printf(TEXT("\n%s attack: %d"), *Type, FMath::RoundToInt(Pair.Value));
		}
		FString Scaling;
		for (const TPair<FGameplayTag, float>& Pair : Weapon->Scaling)
		{
			FString Attr = Pair.Key.GetTagName().ToString();
			Attr.RemoveFromStart(TEXT("OSF.Attribute."));
			Scaling += FString::Printf(TEXT("%s %s  "), *Attr.Left(3), *UOSFCombatLibrary::GetScalingGrade(Pair.Value));
		}
		Text += FString::Printf(TEXT("\nScaling: %s\nWeight: %.1f"), *Scaling, Weapon->Weight);
		if (!Weapon->SkillName.IsEmpty())
		{
			Text += FString::Printf(TEXT("\nSkill: %s"), *Weapon->SkillName.ToString());
		}
	}
	else if (const UOSFArmorData* Armor = Cast<UOSFArmorData>(Item))
	{
		const float* Physical = Armor->Negation.Find(OSFTags::Damage_Physical);
		Text += FString::Printf(TEXT("\n\nPhysical negation: %.1f%%\nPoise: %.0f\nWeight: %.1f"), Physical ? *Physical : 0.f, Armor->Poise, Armor->Weight);
	}
	return FText::FromString(Text);
}

// --- Entries ----------------------------------------------------------------------------

FText AOSFPlayerController::GetMenuTitle() const
{
	switch (GetCurrentMenu())
	{
	case EOSFMenu::System: return LOCTEXT("TitleSystem", "MENU");
	case EOSFMenu::Rest: return CurrentRestingPoint.IsValid() ? CurrentRestingPoint->DisplayName : LOCTEXT("TitleRest", "RESTING POINT");
	case EOSFMenu::LevelUp: return LOCTEXT("TitleLevel", "LEVEL UP");
	case EOSFMenu::Inventory: return LOCTEXT("TitleInventory", "INVENTORY");
	case EOSFMenu::Equipment: return LOCTEXT("TitleEquipment", "EQUIPMENT");
	case EOSFMenu::EquipSelect: return LOCTEXT("TitleSelect", "SELECT ITEM");
	case EOSFMenu::Crafting: return LOCTEXT("TitleCrafting", "CRAFTING");
	case EOSFMenu::Storage: return LOCTEXT("TitleStorage", "STORAGE");
	case EOSFMenu::Vendor: return DialogueNPC.IsValid() ? DialogueNPC->NPCName : LOCTEXT("TitleVendor", "SHOP");
	case EOSFMenu::Dialogue: return DialogueNPC.IsValid() ? DialogueNPC->NPCName : FText::GetEmpty();
	case EOSFMenu::SaveSlots: return LOCTEXT("TitleSave", "SAVE GAME");
	case EOSFMenu::LoadSlots: return LOCTEXT("TitleLoad", "LOAD GAME");
	default: return FText::GetEmpty();
	}
}

FText AOSFPlayerController::GetMenuTabLabel() const
{
	if (MenuStack.Num() == 0)
	{
		return FText::GetEmpty();
	}
	const FOSFMenuFrame& Frame = MenuStack.Last();
	if (Frame.Menu == EOSFMenu::Vendor)
	{
		return Frame.Tab == 0 ? LOCTEXT("TabBuy", "< BUY >   sell") : LOCTEXT("TabSell", "buy   < SELL >");
	}
	if (Frame.Menu == EOSFMenu::Storage)
	{
		return Frame.Tab == 0 ? LOCTEXT("TabCarried", "< CARRIED >   storage") : LOCTEXT("TabStored", "carried   < STORAGE >");
	}
	return FText::GetEmpty();
}

TArray<FOSFMenuEntry> AOSFPlayerController::GetMenuEntries() const
{
	TArray<FOSFMenuEntry> Entries;
	const AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (MenuStack.Num() == 0 || !Hero)
	{
		return Entries;
	}
	const FOSFMenuFrame& Frame = MenuStack.Last();
	auto Add = [&Entries](const FText& Label, const FText& Value = FText::GetEmpty(), bool bEnabled = true) -> FOSFMenuEntry&
	{
		FOSFMenuEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Label = Label;
		Entry.Value = Value;
		Entry.bEnabled = bEnabled;
		Entry.Color = bEnabled ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f);
		return Entry;
	};

	switch (Frame.Menu)
	{
	case EOSFMenu::System:
		Add(LOCTEXT("Inventory", "Inventory"));
		Add(LOCTEXT("Equipment", "Equipment"));
		Add(LOCTEXT("Crafting", "Crafting"));
		Add(LOCTEXT("SaveGame", "Save Game"));
		Add(LOCTEXT("LoadGame", "Load Game"));
		Add(LOCTEXT("Resume", "Resume"));
		break;

	case EOSFMenu::Rest:
		Add(LOCTEXT("LevelUp", "Level Up"), FText::Format(LOCTEXT("LevelValue", "Lv {0}"), FText::AsNumber(Hero->Stats->GetLevel())));
		Add(LOCTEXT("Equipment", "Equipment"));
		Add(LOCTEXT("Crafting", "Crafting"));
		Add(LOCTEXT("Storage", "Storage Chest"));
		Add(LOCTEXT("PassTime", "Pass Time"), FText::FromString(AOSFTimeOfDay::Find(this) ? FString::Printf(TEXT("%02d:00"), FMath::FloorToInt(AOSFTimeOfDay::Find(this)->TimeOfDay)) : TEXT("")));
		Add(LOCTEXT("Leave", "Leave"));
		break;

	case EOSFMenu::LevelUp:
	{
		int32 Total = 0;
		for (const TPair<FGameplayTag, int32>& Pair : PendingLevelUp) { Total += Pair.Value; }
		for (const FGameplayTag& Attribute : GetLevelAttributes())
		{
			const int32 Base = Hero->Stats->GetBaseAttribute(Attribute);
			const int32* Pending = PendingLevelUp.Find(Attribute);
			const int32 Extra = Pending ? *Pending : 0;
			FOSFMenuEntry& Entry = Add(AttributeName(Attribute), Extra > 0 ? FText::Format(LOCTEXT("AttrUp", "{0}  >  {1}"), FText::AsNumber(Base), FText::AsNumber(Base + Extra)) : FText::AsNumber(Base));
			if (Extra > 0) { Entry.Color = FLinearColor(0.4f, 0.8f, 1.f); }
		}
		const int32 Cost = Hero->Stats->GetCostForPoints(Total);
		Add(LOCTEXT("Confirm", "Confirm"), FText::Format(LOCTEXT("Cost", "Cost {0}"), FText::AsNumber(Cost)), Total > 0 && Cost <= Hero->Inventory->GetCurrency());
		break;
	}

	case EOSFMenu::Inventory:
	{
		TArray<UOSFItemData*> Items;
		GetInventoryList(Items);
		for (UOSFItemData* Item : Items)
		{
			const int32 Count = Hero->Inventory->GetItemCount(Item);
			const bool bEquipped = Hero->Equipment->IsItemEquipped(Item);
			FOSFMenuEntry& Entry = Add(Item->DisplayName, FText::FromString(FString::Printf(TEXT("%s%s"), Item->MaxStack > 1 || Item->bIsFlask ? *FString::Printf(TEXT("x%d"), Count) : TEXT(""), bEquipped ? TEXT("  [E]") : TEXT(""))));
			Entry.Item = Item;
			Entry.Description = DescribeItem(Item);
			Entry.Color = bEquipped ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor::White;
		}
		if (Items.Num() == 0) { Add(LOCTEXT("NoItems", "(empty)"), FText::GetEmpty(), false); }
		break;
	}

	case EOSFMenu::Equipment:
		for (const FEquipRow& Row : GetEquipRows())
		{
			const UOSFItemData* Item = GetSlotItem(Hero->Equipment, Row);
			FOSFMenuEntry& Entry = Add(SlotName(Row), Item ? Item->DisplayName : LOCTEXT("EmptySlot", "-"));
			Entry.Description = DescribeItem(Item);
		}
		break;

	case EOSFMenu::EquipSelect:
	{
		Add(LOCTEXT("Unequip", "(Unequip)"));
		TArray<UOSFItemData*> Candidates;
		GetEquipCandidates(Frame.Slot, Candidates);
		for (UOSFItemData* Item : Candidates)
		{
			FOSFMenuEntry& Entry = Add(Item->DisplayName, Hero->Equipment->IsItemEquipped(Item) ? LOCTEXT("Equipped", "[E]") : FText::GetEmpty());
			Entry.Item = Item;
			Entry.Description = DescribeItem(Item);
		}
		break;
	}

	case EOSFMenu::Crafting:
		for (UOSFCraftingRecipe* Recipe : Hero->Inventory->KnownRecipes)
		{
			if (!Recipe || !Hero->Inventory->IsRecipeKnown(Recipe) || !Recipe->Result.Item)
			{
				continue;
			}
			FString Ingredients;
			for (const FOSFItemStack& Ingredient : Recipe->Ingredients)
			{
				if (Ingredient.Item)
				{
					Ingredients += FString::Printf(TEXT("%s  %d / %d\n"), *Ingredient.Item->DisplayName.ToString(), Hero->Inventory->GetItemCount(Ingredient.Item), Ingredient.Count);
				}
			}
			FOSFMenuEntry& Entry = Add(Recipe->DisplayName.IsEmpty() ? Recipe->Result.Item->DisplayName : Recipe->DisplayName,
				FText::Format(LOCTEXT("Have", "have {0}"), FText::AsNumber(Hero->Inventory->GetItemCount(Recipe->Result.Item))), Hero->Inventory->CanCraft(Recipe));
			Entry.Description = FText::FromString(Ingredients);
			Entry.Item = Recipe->Result.Item;
		}
		if (Entries.Num() == 0) { Add(LOCTEXT("NoRecipes", "(no recipes known)"), FText::GetEmpty(), false); }
		break;

	case EOSFMenu::Storage:
	{
		TArray<UOSFItemData*> Items;
		GetInventoryList(Items, Frame.Tab == 1);
		for (UOSFItemData* Item : Items)
		{
			const TArray<FOSFInventoryEntry>& List = Frame.Tab == 1 ? Hero->Inventory->GetStorage() : Hero->Inventory->GetItems();
			const FOSFInventoryEntry* Found = List.FindByPredicate([Item](const FOSFInventoryEntry& E) { return E.Item == Item; });
			const bool bMovable = !(Item->bIsFlask || Item->Category == EOSFItemCategory::KeyItem);
			FOSFMenuEntry& Entry = Add(Item->DisplayName, FText::Format(LOCTEXT("Count", "x{0}"), FText::AsNumber(Found ? Found->Count : 0)), bMovable);
			Entry.Item = Item;
		}
		if (Items.Num() == 0) { Add(LOCTEXT("NoItems", "(empty)"), FText::GetEmpty(), false); }
		break;
	}

	case EOSFMenu::Vendor:
	{
		const AOSFNPC* NPC = DialogueNPC.Get();
		if (!NPC || !NPC->Vendor)
		{
			break;
		}
		if (Frame.Tab == 0)
		{
			for (int32 Index = 0; Index < NPC->Vendor->Entries.Num(); ++Index)
			{
				const FOSFVendorEntry& VendorEntry = NPC->Vendor->Entries[Index];
				if (!VendorEntry.Item || !VendorEntry.RequiredProgress.IsEmpty() && !Hero->Progress->HasAllProgress(VendorEntry.RequiredProgress))
				{
					continue;
				}
				const int32 Stock = NPC->GetRemainingStock(Hero->Progress, Index);
				const FString StockText = Stock < 0 ? TEXT("") : FText::Format(NSLOCTEXT("OSFUI", "StockLeft", "  ({0} left)"), Stock).ToString();
				FOSFMenuEntry& Entry = Add(VendorEntry.Item->DisplayName, FText::FromString(FString::Printf(TEXT("%d%s"), VendorEntry.Price, *StockText)),
					Stock != 0 && Hero->Inventory->GetCurrency() >= VendorEntry.Price);
				Entry.Item = VendorEntry.Item;
				Entry.Description = DescribeItem(VendorEntry.Item);
			}
		}
		else
		{
			TArray<UOSFItemData*> Items;
			GetInventoryList(Items);
			for (UOSFItemData* Item : Items)
			{
				const bool bSellable = Item->SellPrice > 0 && !Item->bIsFlask && Item->Category != EOSFItemCategory::KeyItem;
				FOSFMenuEntry& Entry = Add(Item->DisplayName, FText::FromString(FString::Printf(TEXT("+%d   x%d"), FMath::RoundToInt(Item->SellPrice * NPC->Vendor->SellMultiplier), Hero->Inventory->GetItemCount(Item))), bSellable);
				Entry.Item = Item;
			}
		}
		if (Entries.Num() == 0) { Add(LOCTEXT("Nothing", "(nothing)"), FText::GetEmpty(), false); }
		break;
	}

	case EOSFMenu::Dialogue:
	{
		const AOSFNPC* NPC = DialogueNPC.Get();
		if (!bDialogueFinished)
		{
			Add(LOCTEXT("Continue", "Continue"));
		}
		else
		{
			const FOSFDialogEntry* Entry = (NPC && NPC->Dialog && NPC->Dialog->Entries.IsValidIndex(DialogueEntryIndex)) ? &NPC->Dialog->Entries[DialogueEntryIndex] : nullptr;
			if (NPC && NPC->Vendor && (!Entry || Entry->bOfferTrade))
			{
				Add(LOCTEXT("Trade", "Trade"));
			}
			Add(LOCTEXT("TalkAgain", "Talk"));
			Add(LOCTEXT("LeaveTalk", "Leave"));
		}
		break;
	}

	case EOSFMenu::SaveSlots:
	case EOSFMenu::LoadSlots:
	{
		const UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this);
		for (int32 Slot = 0; Save && Slot < Save->NumSlots; ++Slot)
		{
			const FOSFSaveSummary Summary = Save->GetSlotSummary(Slot);
			const FText Value = Summary.bExists
				? FText::Format(NSLOCTEXT("OSFUI", "SaveSummary", "Lv {0}   {1}"), Summary.Level, FText::FromString(Summary.Timestamp.ToString(TEXT("%Y-%m-%d %H:%M"))))
				: LOCTEXT("EmptySave", "(empty)");
			Add(FText::Format(LOCTEXT("SlotN", "Slot {0}"), FText::AsNumber(Slot + 1)), Value, Frame.Menu == EOSFMenu::SaveSlots || Summary.bExists);
		}
		break;
	}

	default:
		break;
	}
	return Entries;
}

FText AOSFPlayerController::GetMenuDetails() const
{
	const AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (MenuStack.Num() == 0 || !Hero)
	{
		return FText::GetEmpty();
	}
	const FOSFMenuFrame& Frame = MenuStack.Last();
	if (Frame.Menu == EOSFMenu::Dialogue)
	{
		const AOSFNPC* NPC = DialogueNPC.Get();
		if (NPC && NPC->Dialog && NPC->Dialog->Entries.IsValidIndex(DialogueEntryIndex))
		{
			const FOSFDialogEntry& Entry = NPC->Dialog->Entries[DialogueEntryIndex];
			if (Entry.Lines.IsValidIndex(DialogueLine))
			{
				return Entry.Lines[DialogueLine];
			}
			return Entry.Lines.Num() > 0 ? Entry.Lines.Last() : FText::GetEmpty();
		}
		return LOCTEXT("NothingToSay", "...");
	}
	if (Frame.Menu == EOSFMenu::LevelUp)
	{
		int32 Total = 0;
		for (const TPair<FGameplayTag, int32>& Pair : PendingLevelUp) { Total += Pair.Value; }
		return FText::FromString(FString::Printf(TEXT("Level %d  >  %d\nRunes held: %d\nRunes needed: %d\n\nHP %.0f   FP %.0f   Stamina %.0f\n\n[Left/Right] allocate points"),
			Hero->Stats->GetLevel(), Hero->Stats->GetLevel() + Total, Hero->Inventory->GetCurrency(), Hero->Stats->GetCostForPoints(FMath::Max(Total, 1)),
			Hero->Stats->GetMax(OSFTags::Stat_Health), Hero->Stats->GetMax(OSFTags::Stat_FP), Hero->Stats->GetMax(OSFTags::Stat_Stamina)));
	}
	const TArray<FOSFMenuEntry> Entries = GetMenuEntries();
	if (Entries.IsValidIndex(Frame.Selection))
	{
		return Entries[Frame.Selection].Description;
	}
	return FText::GetEmpty();
}

// --- Confirm --------------------------------------------------------------------------------

void AOSFPlayerController::ConfirmEntry(FOSFMenuFrame& Frame, int32 Index)
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (!Hero)
	{
		return;
	}
	const EOSFMenu Menu = Frame.Menu;

	switch (Menu)
	{
	case EOSFMenu::System:
		switch (Index)
		{
		case 0: OpenMenu(EOSFMenu::Inventory); break;
		case 1: OpenMenu(EOSFMenu::Equipment); break;
		case 2: OpenMenu(EOSFMenu::Crafting); break;
		case 3: OpenMenu(EOSFMenu::SaveSlots); break;
		case 4: OpenMenu(EOSFMenu::LoadSlots); break;
		default: CloseAllMenus(); break;
		}
		break;

	case EOSFMenu::Rest:
		switch (Index)
		{
		case 0: OpenMenu(EOSFMenu::LevelUp); break;
		case 1: OpenMenu(EOSFMenu::Equipment); break;
		case 2: OpenMenu(EOSFMenu::Crafting); break;
		case 3: OpenMenu(EOSFMenu::Storage); break;
		case 4:
			if (AOSFTimeOfDay* Time = AOSFTimeOfDay::Find(this))
			{
				Time->PassTime(6.f);
				PushNotification(LOCTEXT("TimePassed", "Time passes..."), EOSFNotifyType::Hint);
			}
			break;
		default: CloseAllMenus(); break;
		}
		break;

	case EOSFMenu::LevelUp:
		if (Index == GetLevelAttributes().Num())
		{
			TMap<FGameplayTag, int32> Increments;
			for (const TPair<FGameplayTag, int32>& Pair : PendingLevelUp)
			{
				if (Pair.Value > 0) { Increments.Add(Pair.Key, Pair.Value); }
			}
			if (Hero->Stats->LevelUp(Increments))
			{
				PendingLevelUp.Reset();
				Hero->Stats->RestoreAll();
				PushNotification(FText::Format(LOCTEXT("LevelReached", "Level {0}"), FText::AsNumber(Hero->Stats->GetLevel())), EOSFNotifyType::Hint);
			}
		}
		break;

	case EOSFMenu::Inventory:
	{
		TArray<UOSFItemData*> Items;
		GetInventoryList(Items);
		if (!Items.IsValidIndex(Index))
		{
			break;
		}
		UOSFItemData* Item = Items[Index];
		if (Item->IsUsable())
		{
			if (UOSFAction_UseItem* Use = Cast<UOSFAction_UseItem>(Hero->ActionComponent->FindAction(OSFTags::Action_UseItem)))
			{
				Use->ItemOverride = Item;
				CloseAllMenus();
				if (!Hero->ActionComponent->TryExecuteAction(OSFTags::Action_UseItem, false))
				{
					Use->ItemOverride = nullptr;
				}
			}
		}
		else if (Item->IsEquippable())
		{
			if (Hero->Equipment->IsItemEquipped(Item)) { Hero->Equipment->UnequipItem(Item); }
			else { Hero->Equipment->EquipItemAuto(Item); }
		}
		break;
	}

	case EOSFMenu::Equipment:
	{
		const TArray<FEquipRow> Rows = GetEquipRows();
		if (Rows.IsValidIndex(Index))
		{
			OpenMenu(EOSFMenu::EquipSelect);
			MenuStack.Last().Slot = Rows[Index].Slot;
			MenuStack.Last().SlotIndex = Rows[Index].Index;
		}
		break;
	}

	case EOSFMenu::EquipSelect:
	{
		const EOSFEquipSlot Slot = Frame.Slot;
		const int32 SlotIndex = Frame.SlotIndex;
		TArray<UOSFItemData*> Candidates;
		GetEquipCandidates(Slot, Candidates);
		UOSFItemData* Item = Index == 0 ? nullptr : (Candidates.IsValidIndex(Index - 1) ? Candidates[Index - 1] : nullptr);
		UOSFEquipmentComponent* Equipment = Hero->Equipment;
		switch (Slot)
		{
		case EOSFEquipSlot::RightHand: Equipment->EquipWeapon(Cast<UOSFWeaponData>(Item), true, SlotIndex); break;
		case EOSFEquipSlot::LeftHand: Equipment->EquipWeapon(Cast<UOSFWeaponData>(Item), false, SlotIndex); break;
		case EOSFEquipSlot::Talisman: Equipment->EquipTalisman(Cast<UOSFTalismanData>(Item), SlotIndex); break;
		case EOSFEquipSlot::QuickItem: Equipment->SetQuickItem(Item, SlotIndex); break;
		default:
			if (Item) { Equipment->EquipArmor(Cast<UOSFArmorData>(Item)); }
			else
			{
				Equipment->Armor.Remove(Slot);
				Equipment->RefreshEquipment();
			}
			break;
		}
		CloseMenu();
		break;
	}

	case EOSFMenu::Crafting:
	{
		int32 RecipeIndex = 0;
		for (UOSFCraftingRecipe* Recipe : Hero->Inventory->KnownRecipes)
		{
			if (!Recipe || !Hero->Inventory->IsRecipeKnown(Recipe) || !Recipe->Result.Item)
			{
				continue;
			}
			if (RecipeIndex++ == Index)
			{
				Hero->Inventory->Craft(Recipe);
				break;
			}
		}
		break;
	}

	case EOSFMenu::Storage:
	{
		TArray<UOSFItemData*> Items;
		GetInventoryList(Items, Frame.Tab == 1);
		if (Items.IsValidIndex(Index))
		{
			if (Frame.Tab == 0)
			{
				if (Hero->Equipment->IsItemEquipped(Items[Index]) && Hero->Inventory->GetItemCount(Items[Index]) <= 1)
				{
					Hero->Equipment->UnequipItem(Items[Index]);
				}
				Hero->Inventory->DepositToStorage(Items[Index], 1);
			}
			else
			{
				Hero->Inventory->WithdrawFromStorage(Items[Index], 1);
			}
		}
		break;
	}

	case EOSFMenu::Vendor:
	{
		AOSFNPC* NPC = DialogueNPC.Get();
		if (!NPC || !NPC->Vendor)
		{
			break;
		}
		if (Frame.Tab == 0)
		{
			int32 Visible = 0;
			for (int32 EntryIndex = 0; EntryIndex < NPC->Vendor->Entries.Num(); ++EntryIndex)
			{
				const FOSFVendorEntry& VendorEntry = NPC->Vendor->Entries[EntryIndex];
				if (!VendorEntry.Item || !VendorEntry.RequiredProgress.IsEmpty() && !Hero->Progress->HasAllProgress(VendorEntry.RequiredProgress))
				{
					continue;
				}
				if (Visible++ == Index)
				{
					NPC->Buy(Hero, EntryIndex);
					break;
				}
			}
		}
		else
		{
			TArray<UOSFItemData*> Items;
			GetInventoryList(Items);
			if (Items.IsValidIndex(Index))
			{
				NPC->Sell(Hero, Items[Index]);
			}
		}
		break;
	}

	case EOSFMenu::Dialogue:
	{
		AOSFNPC* NPC = DialogueNPC.Get();
		if (!bDialogueFinished)
		{
			const FOSFDialogEntry* Entry = (NPC && NPC->Dialog && NPC->Dialog->Entries.IsValidIndex(DialogueEntryIndex)) ? &NPC->Dialog->Entries[DialogueEntryIndex] : nullptr;
			++DialogueLine;
			if (!Entry || DialogueLine >= Entry->Lines.Num())
			{
				bDialogueFinished = true;
				if (Entry) { NPC->CompleteDialog(Hero, *Entry); }
				DialogueLine = Entry ? Entry->Lines.Num() - 1 : 0;
				Frame.Selection = 0;
			}
			break;
		}
		const TArray<FOSFMenuEntry> Entries = GetMenuEntries();
		const FString Choice = Entries.IsValidIndex(Index) ? Entries[Index].Label.ToString() : FString();
		if (Choice == LOCTEXT("Trade", "Trade").ToString())
		{
			OpenMenu(EOSFMenu::Vendor);
		}
		else if (Choice == LOCTEXT("TalkAgain", "Talk").ToString())
		{
			OpenDialogue(NPC);
		}
		else
		{
			CloseAllMenus();
		}
		break;
	}

	case EOSFMenu::SaveSlots:
		if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this))
		{
			if (Save->SaveToSlot(Hero, Index))
			{
				PushNotification(LOCTEXT("Saved", "Game saved"), EOSFNotifyType::Hint);
			}
		}
		break;

	case EOSFMenu::LoadSlots:
		if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this))
		{
			CloseAllMenus();
			Save->LoadFromSlot(this, Index);
		}
		break;

	default:
		break;
	}
}

// --- Exec commands ----------------------------------------------------------------------------

void AOSFPlayerController::OSFGiveRunes(int32 Amount)
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer()) { Hero->Inventory->AddCurrency(Amount); }
}

void AOSFPlayerController::OSFGiveItem(const FString& ItemId, int32 Count)
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	UOSFItemData* Item = UOSFItemRegistry::Find(FName(*ItemId));
	if (Hero && Item)
	{
		Hero->Inventory->AddItem(Item, FMath::Max(1, Count));
	}
	else
	{
		UE_LOG(LogOSF, Warning, TEXT("OSFGiveItem: unknown item '%s'"), *ItemId);
	}
}

void AOSFPlayerController::OSFGod()
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer())
	{
		if (Hero->HasStateTag(OSFTags::State_Invincible)) { Hero->ActionComponent->ClearStateTag(OSFTags::State_Invincible); }
		else { Hero->ActionComponent->AddStateTag(OSFTags::State_Invincible); }
		UE_LOG(LogOSF, Log, TEXT("God mode: %s"), Hero->HasStateTag(OSFTags::State_Invincible) ? TEXT("ON") : TEXT("OFF"));
	}
}

void AOSFPlayerController::OSFHeal()
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer()) { Hero->Stats->RestoreAll(); }
}

void AOSFPlayerController::OSFKillAll()
{
	for (TActorIterator<AOSFEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->Combat->Kill(GetPawn());
	}
}

void AOSFPlayerController::OSFRest()
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	AOSFRestingPoint* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AOSFRestingPoint> It(GetWorld()); It && Hero; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), Hero->GetActorLocation());
		if (Distance < BestDistance) { BestDistance = Distance; Best = *It; }
	}
	if (Best && Hero)
	{
		Best->Discover(Hero);
		Hero->RestAt(Best);
	}
}

void AOSFPlayerController::OSFSave(int32 Slot)
{
	if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this)) { Save->SaveToSlot(GetOSFPlayer(), Slot); }
}

void AOSFPlayerController::OSFLoad(int32 Slot)
{
	if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this)) { Save->LoadFromSlot(this, Slot); }
}

void AOSFPlayerController::OSFAction(const FString& ActionTag)
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	FString TagName = ActionTag;
	if (!TagName.StartsWith(TEXT("OSF.")))
	{
		TagName = TEXT("OSF.Action.") + TagName;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
	if (!Hero || !Tag.IsValid())
	{
		return;
	}
	if (Tag == OSFTags::Action_Guard && Hero->HasStateTag(OSFTags::State_Guarding))
	{
		Hero->ActionComponent->StopAction(Tag);
		return;
	}
	const bool bStarted = Hero->PerformAction(Tag);
	UE_LOG(LogOSF, Log, TEXT("OSFAction %s -> %s"), *TagName, bStarted ? TEXT("started") : TEXT("buffered/blocked"));
}

void AOSFPlayerController::OSFLockOn()
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer()) { Hero->LockOn->ToggleLockOn(); }
}

void AOSFPlayerController::OSFCycleAmmo()
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	UOSFWeaponData* Ranged = Hero ? Hero->Equipment->GetActiveRangedWeapon() : nullptr;
	if (Ranged && Hero->Equipment->CycleAmmo(Ranged))
	{
		Hero->PerformAction(OSFTags::Action_Reload);
	}
	const FOSFMagazine Magazine = Hero ? Hero->Equipment->GetMagazine(Ranged) : FOSFMagazine();
	UE_LOG(LogOSF, Log, TEXT("OSFCycleAmmo -> %s"), Magazine.Ammo ? *Magazine.Ammo->ItemId.ToString() : TEXT("none"));
}

void AOSFPlayerController::OSFFace(const FString& Filter)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It != MyPawn && (It->GetName().Contains(Filter) || It->GetClass()->GetName().Contains(Filter)))
		{
			const FRotator Facing = (It->GetActorLocation() - MyPawn->GetActorLocation()).GetSafeNormal2D().Rotation();
			MyPawn->SetActorRotation(Facing);
			SetControlRotation(FRotator(-10.f, Facing.Yaw, 0.f));
			return;
		}
	}
}

void AOSFPlayerController::OSFTeleport(float X, float Y, float Z)
{
	if (APawn* MyPawn = GetPawn()) { MyPawn->SetActorLocation(FVector(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics); }
}

void AOSFPlayerController::OSFDebugTraces()
{
	for (TActorIterator<AOSFCharacterBase> It(GetWorld()); It; ++It)
	{
		It->WeaponTrace->bDrawDebug = !It->WeaponTrace->bDrawDebug;
	}
}

void AOSFPlayerController::OSFMenu(const FString& Command)
{
	if (Command == TEXT("up")) { MenuNavigate(0, -1); }
	else if (Command == TEXT("down")) { MenuNavigate(0, 1); }
	else if (Command == TEXT("left")) { MenuNavigate(-1, 0); }
	else if (Command == TEXT("right")) { MenuNavigate(1, 0); }
	else if (Command == TEXT("confirm")) { MenuConfirm(); }
	else if (Command == TEXT("back")) { MenuBack(); }
	else if (Command == TEXT("open")) { ToggleSystemMenu(); }
	UE_LOG(LogOSF, Log, TEXT("Menu %d selection %d: %s"), static_cast<int32>(GetCurrentMenu()), GetMenuSelection(), *GetMenuTitle().ToString());
}

FString AOSFPlayerController::GetStatusReport() const
{
	const AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (!Hero)
	{
		return TEXT("No player");
	}
	FString Report = FString::Printf(TEXT("Level %d | HP %.0f/%.0f | FP %.0f/%.0f | ST %.0f/%.0f | Poise %.0f | Runes %d | Dead %d | Pos %s"),
		Hero->Stats->GetLevel(),
		Hero->Stats->GetCurrent(OSFTags::Stat_Health), Hero->Stats->GetMax(OSFTags::Stat_Health),
		Hero->Stats->GetCurrent(OSFTags::Stat_FP), Hero->Stats->GetMax(OSFTags::Stat_FP),
		Hero->Stats->GetCurrent(OSFTags::Stat_Stamina), Hero->Stats->GetMax(OSFTags::Stat_Stamina),
		Hero->Stats->GetCurrent(OSFTags::Stat_Poise),
		Hero->Inventory->GetCurrency(), Hero->IsDead() ? 1 : 0, *Hero->GetActorLocation().ToCompactString());
	Report += FString::Printf(TEXT(" | Tags %s | Action %s | LockOn %s"),
		*Hero->ActionComponent->GetStateTags().ToStringSimple(),
		Hero->ActionComponent->GetCurrentAction() ? *Hero->ActionComponent->GetCurrentAction()->ActionTag.ToString() : TEXT("-"),
		*GetNameSafe(Hero->LockOn->GetTarget()));
	for (TActorIterator<AOSFEnemyCharacter> It(GetWorld()); It; ++It)
	{
		Report += FString::Printf(TEXT("\n  Enemy %s HP %.0f/%.0f dead=%d dist=%.0f tags=%s"), *It->EnemyName.ToString(),
			It->Stats->GetCurrent(OSFTags::Stat_Health), It->Stats->GetMax(OSFTags::Stat_Health), It->IsDead() ? 1 : 0,
			FVector::Dist(It->GetActorLocation(), Hero->GetActorLocation()), *It->ActionComponent->GetStateTags().ToStringSimple());
	}
	return Report;
}

void AOSFPlayerController::OSFStatus()
{
	// One log line per entry so log readers (MCP log tools) get every line.
	TArray<FString> Lines;
	GetStatusReport().ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		UE_LOG(LogOSF, Log, TEXT("[OSFStatus] %s"), *Line.TrimStart());
	}
}

void AOSFPlayerController::OSFInteract()
{
	if (AOSFPlayerCharacter* Hero = GetOSFPlayer())
	{
		const bool bInteracted = Hero->Interaction->TryInteract();
		UE_LOG(LogOSF, Log, TEXT("OSFInteract -> %s"), bInteracted ? *GetNameSafe(Hero->Interaction->GetFocusedInteractable()) : TEXT("nothing in range"));
	}
}

void AOSFPlayerController::OSFGoTo(const FString& Filter)
{
	AOSFPlayerCharacter* Hero = GetOSFPlayer();
	if (!Hero)
	{
		return;
	}
	CloseAllMenus();
	if (Hero->GetCurrentLadder())
	{
		Hero->ExitLadder(false);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == Hero || Actor->IsHidden() || !(Actor->GetName().Contains(Filter) || Actor->GetClass()->GetName().Contains(Filter)))
		{
			continue;
		}
		// Stand in front of the actor, facing it (the fog gate is entered from its back side).
		const bool bFromBehind = Actor->IsA<AOSFFogGate>();
		const FVector Forward = Actor->GetActorForwardVector().GetSafeNormal2D() * (bFromBehind ? -1.f : 1.f);
		const FVector Location = Actor->GetActorLocation() + Forward * 160.f + FVector(0.f, 0.f, 100.f);
		Hero->SetActorLocationAndRotation(Location, (-Forward).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		SetControlRotation((-Forward).Rotation());
		Hero->Interaction->UpdateFocus();
		UE_LOG(LogOSF, Log, TEXT("OSFGoTo %s -> %s (focus: %s)"), *Filter, *Actor->GetName(), *GetNameSafe(Hero->Interaction->GetFocusedInteractable()));
		return;
	}
	UE_LOG(LogOSF, Warning, TEXT("OSFGoTo: nothing matches '%s'"), *Filter);
}

#undef LOCTEXT_NAMESPACE
