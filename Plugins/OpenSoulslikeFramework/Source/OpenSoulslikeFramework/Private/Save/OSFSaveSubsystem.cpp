// Copyright Open Soulslike Framework contributors. MIT License.

#include "Save/OSFSaveSubsystem.h"
#include "Save/OSFSaveGame.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Game/OSFGameMode.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFItemRegistry.h"
#include "Progression/OSFProgressComponent.h"
#include "Stats/OSFStatComponent.h"
#include "World/OSFTimeOfDay.h"
#include "OpenSoulslikeFramework.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FName IdOf(const UOSFItemData* Item)
	{
		return Item ? (Item->ItemId.IsNone() ? Item->GetFName() : Item->ItemId) : NAME_None;
	}

	const EOSFEquipSlot ArmorSlots[] = { EOSFEquipSlot::Head, EOSFEquipSlot::Chest, EOSFEquipSlot::Arms, EOSFEquipSlot::Legs };
}

UOSFSaveSubsystem* UOSFSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOSFSaveSubsystem>() : nullptr;
}

void UOSFSaveSubsystem::CapturePlayer(AOSFPlayerCharacter* Player, UOSFSaveGame* Save)
{
	if (!Player || !Save)
	{
		return;
	}
	UWorld* World = Player->GetWorld();
	Save->MapName = UGameplayStatics::GetCurrentLevelName(Player, true);
	Save->Timestamp = FDateTime::Now();
	Save->PlayTime = World ? World->GetTimeSeconds() : 0.f;
	Save->PlayerTransform = Player->GetActorTransform();
	Save->RespawnTransform = Player->GetRespawnTransform();
	Save->LastRestingPoint = Player->GetLastRestingPoint();
	Save->Attributes = Player->Stats->GetAllBaseAttributes();
	Save->Level = Player->Stats->GetLevel();
	Save->Currency = Player->Inventory->GetCurrency();

	auto CaptureList = [](const TArray<FOSFInventoryEntry>& From, TArray<FOSFSavedItem>& To)
	{
		To.Reset();
		for (const FOSFInventoryEntry& Entry : From)
		{
			if (Entry.Item)
			{
				To.Add({ IdOf(Entry.Item), Entry.Count, Entry.MaxCharges });
			}
		}
	};
	CaptureList(Player->Inventory->GetItems(), Save->Inventory);
	CaptureList(Player->Inventory->GetStorage(), Save->Storage);

	const UOSFEquipmentComponent* Equipment = Player->Equipment;
	Save->RightHand.Reset();
	Save->LeftHand.Reset();
	Save->Talismans.Reset();
	Save->QuickItems.Reset();
	Save->Armor.Reset();
	for (const UOSFWeaponData* Item : Equipment->RightHand) { Save->RightHand.Add(IdOf(Item)); }
	for (const UOSFWeaponData* Item : Equipment->LeftHand) { Save->LeftHand.Add(IdOf(Item)); }
	for (const UOSFTalismanData* Item : Equipment->Talismans) { Save->Talismans.Add(IdOf(Item)); }
	for (const UOSFItemData* Item : Equipment->QuickItems) { Save->QuickItems.Add(IdOf(Item)); }
	for (const EOSFEquipSlot Slot : ArmorSlots)
	{
		const TObjectPtr<UOSFArmorData>* Piece = Equipment->Armor.Find(Slot);
		Save->Armor.Add(Piece ? IdOf(*Piece) : NAME_None);
	}
	Save->ActiveRight = Equipment->GetActiveSlot(true);
	Save->ActiveLeft = Equipment->GetActiveSlot(false);
	Save->ActiveQuick = Equipment->GetActiveQuickSlot();
	Save->bTwoHanding = Equipment->IsTwoHanding();

	Save->ProgressTags = Player->Progress->GetProgressTags();
	Save->ProgressFlags = Player->Progress->GetFlags();
	Save->ProgressCounters = Player->Progress->GetCounters();

	const AOSFGameMode* GameMode = World ? World->GetAuthGameMode<AOSFGameMode>() : nullptr;
	const AOSFBloodstain* Stain = GameMode ? GameMode->GetBloodstain() : nullptr;
	Save->bHasBloodstain = Stain != nullptr;
	Save->BloodstainLocation = Stain ? Stain->GetActorLocation() : FVector::ZeroVector;
	Save->BloodstainCurrency = Stain ? Stain->Currency : 0;

	if (const AOSFTimeOfDay* Time = AOSFTimeOfDay::Find(Player))
	{
		Save->TimeOfDay = Time->TimeOfDay;
	}
}

void UOSFSaveSubsystem::ApplyToPlayer(AOSFPlayerCharacter* Player, const UOSFSaveGame* Save, bool bTeleport)
{
	if (!Player || !Save)
	{
		return;
	}
	auto Resolve = [](FName Id) -> UOSFItemData* { return UOSFItemRegistry::Find(Id); };

	// Stats
	Player->Stats->Attributes = Save->Attributes;
	Player->Stats->RecalculateStats();
	Player->Stats->RestoreAll();

	// Inventory
	auto RestoreList = [&Resolve](const TArray<FOSFSavedItem>& From, TArray<FOSFInventoryEntry>& To)
	{
		for (const FOSFSavedItem& Saved : From)
		{
			if (UOSFItemData* Item = Resolve(Saved.ItemId))
			{
				FOSFInventoryEntry& Entry = To.AddDefaulted_GetRef();
				Entry.Item = Item;
				Entry.Count = Saved.Count;
				Entry.MaxCharges = Saved.MaxCharges;
			}
			else
			{
				UE_LOG(LogOSF, Warning, TEXT("Save: unknown item '%s' skipped"), *Saved.ItemId.ToString());
			}
		}
	};
	TArray<FOSFInventoryEntry> Items;
	TArray<FOSFInventoryEntry> Storage;
	RestoreList(Save->Inventory, Items);
	RestoreList(Save->Storage, Storage);
	Player->Inventory->SetRawContents(Items, Storage, Save->Currency);

	// Equipment
	UOSFEquipmentComponent* Equipment = Player->Equipment;
	for (int32 Index = 0; Index < UOSFEquipmentComponent::NumHandSlots; ++Index)
	{
		Equipment->RightHand[Index] = Save->RightHand.IsValidIndex(Index) ? Cast<UOSFWeaponData>(Resolve(Save->RightHand[Index])) : nullptr;
		Equipment->LeftHand[Index] = Save->LeftHand.IsValidIndex(Index) ? Cast<UOSFWeaponData>(Resolve(Save->LeftHand[Index])) : nullptr;
	}
	for (int32 Index = 0; Index < UOSFEquipmentComponent::NumTalismanSlots; ++Index)
	{
		Equipment->Talismans[Index] = Save->Talismans.IsValidIndex(Index) ? Cast<UOSFTalismanData>(Resolve(Save->Talismans[Index])) : nullptr;
	}
	for (int32 Index = 0; Index < UOSFEquipmentComponent::NumQuickSlots; ++Index)
	{
		Equipment->QuickItems[Index] = Save->QuickItems.IsValidIndex(Index) ? Resolve(Save->QuickItems[Index]) : nullptr;
	}
	Equipment->Armor.Reset();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ArmorSlots); ++Index)
	{
		if (UOSFArmorData* Piece = Save->Armor.IsValidIndex(Index) ? Cast<UOSFArmorData>(Resolve(Save->Armor[Index])) : nullptr)
		{
			Equipment->Armor.Add(ArmorSlots[Index], Piece);
		}
	}
	Equipment->SetActiveSlots(Save->ActiveRight, Save->ActiveLeft, Save->ActiveQuick, Save->bTwoHanding);

	// Progress
	Player->Progress->SetRaw(Save->ProgressTags, Save->ProgressFlags, Save->ProgressCounters);
	Player->SetRespawnPoint(Save->RespawnTransform, Save->LastRestingPoint);

	if (bTeleport)
	{
		Player->ResetCharacter(Save->PlayerTransform);
	}

	UWorld* World = Player->GetWorld();
	if (AOSFGameMode* GameMode = World ? World->GetAuthGameMode<AOSFGameMode>() : nullptr)
	{
		GameMode->ClearBloodstain();
		if (Save->bHasBloodstain)
		{
			GameMode->SpawnBloodstain(Save->BloodstainLocation, Save->BloodstainCurrency);
		}
		GameMode->ResetWorld(Player->Progress);
	}
	if (AOSFTimeOfDay* Time = AOSFTimeOfDay::Find(Player))
	{
		Time->TimeOfDay = Save->TimeOfDay;
	}
}

bool UOSFSaveSubsystem::SaveToSlot(AOSFPlayerCharacter* Player, int32 Slot)
{
	if (!Player || Slot < 0 || Slot >= NumSlots)
	{
		return false;
	}
	UOSFSaveGame* Save = Cast<UOSFSaveGame>(UGameplayStatics::CreateSaveGameObject(UOSFSaveGame::StaticClass()));
	CapturePlayer(Player, Save);
	ActiveSlot = Slot;
	SummaryCache.Remove(Slot);
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, GetSlotName(Slot), 0);
	UE_LOG(LogOSF, Log, TEXT("Saved slot %d: %s"), Slot, bOk ? TEXT("ok") : TEXT("FAILED"));
	return bOk;
}

bool UOSFSaveSubsystem::LoadFromSlot(UObject* WorldContext, int32 Slot)
{
	UOSFSaveGame* Save = Cast<UOSFSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(Slot), 0));
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!Save || !World)
	{
		return false;
	}
	ActiveSlot = Slot;
	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(World, true);
	APlayerController* PC = World->GetFirstPlayerController();
	AOSFPlayerCharacter* Player = PC ? Cast<AOSFPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Player && (Save->MapName.IsEmpty() || Save->MapName == CurrentMap))
	{
		ApplyToPlayer(Player, Save, true);
		UE_LOG(LogOSF, Log, TEXT("Loaded slot %d in place"), Slot);
		return true;
	}
	PendingLoad = Save;
	UGameplayStatics::OpenLevel(World, FName(*Save->MapName));
	return true;
}

void UOSFSaveSubsystem::ApplyPendingLoad(AOSFPlayerCharacter* Player)
{
	if (PendingLoad && Player)
	{
		UOSFSaveGame* Save = PendingLoad;
		PendingLoad = nullptr;
		ApplyToPlayer(Player, Save, true);
	}
}

bool UOSFSaveSubsystem::DeleteSlot(int32 Slot)
{
	SummaryCache.Remove(Slot);
	return UGameplayStatics::DeleteGameInSlot(GetSlotName(Slot), 0);
}

FOSFSaveSummary UOSFSaveSubsystem::GetSlotSummary(int32 Slot) const
{
	if (const FOSFSaveSummary* Cached = SummaryCache.Find(Slot))
	{
		return *Cached;
	}
	FOSFSaveSummary Summary;
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(Slot), 0))
	{
		if (const UOSFSaveGame* Save = Cast<UOSFSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(Slot), 0)))
		{
			Summary.bExists = true;
			Summary.Level = Save->Level;
			Summary.Currency = Save->Currency;
			Summary.MapName = Save->MapName;
			Summary.Timestamp = Save->Timestamp;
			Summary.PlayTime = Save->PlayTime;
		}
	}
	SummaryCache.Add(Slot, Summary);
	return Summary;
}

void UOSFSaveSubsystem::AutoSave(AOSFPlayerCharacter* Player)
{
	SaveToSlot(Player, ActiveSlot);
}
