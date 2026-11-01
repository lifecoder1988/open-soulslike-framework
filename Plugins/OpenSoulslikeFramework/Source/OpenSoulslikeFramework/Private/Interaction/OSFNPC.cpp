// Copyright Open Soulslike Framework contributors. MIT License.

#include "Interaction/OSFNPC.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFItemData.h"
#include "Progression/OSFProgressComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "OSFNPC"

int32 UOSFDialogData::SelectEntry(const UOSFProgressComponent* Progress) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FOSFDialogEntry& Entry = Entries[Index];
		if (Progress)
		{
			if (!Progress->MeetsRequirements(Entry.RequiredProgress, Entry.BlockedByProgress))
			{
				continue;
			}
			bool bFlagsOk = true;
			for (const FName& Flag : Entry.RequiredFlags) { bFlagsOk &= Progress->HasFlag(Flag); }
			for (const FName& Flag : Entry.BlockedByFlags) { bFlagsOk &= !Progress->HasFlag(Flag); }
			if (!bFlagsOk)
			{
				continue;
			}
		}
		else if (!Entry.RequiredProgress.IsEmpty() || Entry.RequiredFlags.Num() > 0)
		{
			continue;
		}
		return Index;
	}
	return INDEX_NONE;
}

AOSFNPC::AOSFNPC()
{
	Prompt = LOCTEXT("Talk", "Talk");
	NPCName = LOCTEXT("Stranger", "Stranger");
	Color = FLinearColor(0.25f, 0.3f, 0.45f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.5f, 1.3f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(Root);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 158.f));
	HeadMesh->SetRelativeScale3D(FVector(0.38f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereMesh.Object);
	}
	InteractionVolume->SetSphereRadius(220.f);

	SkeletalBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBody"));
	SkeletalBody->SetupAttachment(Root);
	SkeletalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalBody->SetVisibility(false);
}

void AOSFNPC::SetupSkeletalBody(USkeletalMesh* InMesh, float TargetHeight, float YawOffset, UAnimSequenceBase* IdleAnimation)
{
	if (!InMesh)
	{
		return;
	}
	SkeletalBody->SetSkeletalMeshAsset(InMesh);
	const FBox Box = InMesh->GetImportedBounds().GetBox();
	const float Scale = TargetHeight / FMath::Max(Box.GetSize().Z, 1.f);
	SkeletalBody->SetRelativeScale3D(FVector(Scale));
	SkeletalBody->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -Box.Min.Z * Scale), FRotator(0.f, YawOffset, 0.f));
	SkeletalBody->SetVisibility(true);
	if (IdleAnimation)
	{
		SkeletalBody->PlayAnimation(IdleAnimation, true);
	}
	// Keep the basic-shape body as an invisible blocker.
	Mesh->SetVisibility(false);
	HeadMesh->SetVisibility(false);
}

const FOSFDialogEntry* AOSFNPC::GetCurrentDialog(const UOSFProgressComponent* Progress) const
{
	if (!Dialog)
	{
		return nullptr;
	}
	const int32 Index = Dialog->SelectEntry(Progress);
	return Dialog->Entries.IsValidIndex(Index) ? &Dialog->Entries[Index] : nullptr;
}

void AOSFNPC::Interact_Implementation(AActor* Interactor)
{
	if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Interactor))
	{
		// Face the player.
		const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
		Player->BeginDialogue(this);
	}
}

FText AOSFNPC::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return FText::Format(LOCTEXT("TalkTo", "Talk to {0}"), NPCName);
}

void AOSFNPC::CompleteDialog(AOSFPlayerCharacter* Player, const FOSFDialogEntry& Entry)
{
	if (!Player)
	{
		return;
	}
	if (UOSFProgressComponent* Progress = Player->Progress)
	{
		for (const FGameplayTag& Tag : Entry.GrantProgress) { Progress->AddProgress(Tag); }
		for (const FName& Flag : Entry.GrantFlags) { Progress->AddFlag(Flag); }
		Progress->AddCounter(FName(*FString::Printf(TEXT("Talked.%s"), *GetSaveId().ToString())));
	}
	if (UOSFInventoryComponent* Inventory = Player->Inventory)
	{
		for (const FOSFItemStack& Stack : Entry.GrantItems)
		{
			Inventory->AddItem(Stack.Item, Stack.Count);
		}
	}
}

FName AOSFNPC::GetStockKey(int32 EntryIndex) const
{
	return FName(*FString::Printf(TEXT("Vendor.%s.%d"), *GetSaveId().ToString(), EntryIndex));
}

bool AOSFNPC::IsEntryAvailable(const UOSFProgressComponent* Progress, int32 EntryIndex) const
{
	if (!Vendor || !Vendor->Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}
	const FOSFVendorEntry& Entry = Vendor->Entries[EntryIndex];
	if (!Entry.RequiredProgress.IsEmpty() && !(Progress && Progress->HasAllProgress(Entry.RequiredProgress)))
	{
		return false;
	}
	return GetRemainingStock(Progress, EntryIndex) != 0;
}

int32 AOSFNPC::GetRemainingStock(const UOSFProgressComponent* Progress, int32 EntryIndex) const
{
	if (!Vendor || !Vendor->Entries.IsValidIndex(EntryIndex))
	{
		return 0;
	}
	const int32 Stock = Vendor->Entries[EntryIndex].Stock;
	if (Stock < 0)
	{
		return -1;
	}
	const int32 Bought = Progress ? Progress->GetCounter(GetStockKey(EntryIndex)) : 0;
	return FMath::Max(0, Stock - Bought);
}

bool AOSFNPC::Buy(AOSFPlayerCharacter* Player, int32 EntryIndex)
{
	if (!Player || !IsEntryAvailable(Player->Progress, EntryIndex))
	{
		return false;
	}
	const FOSFVendorEntry& Entry = Vendor->Entries[EntryIndex];
	if (!Player->Inventory->SpendCurrency(Entry.Price))
	{
		Player->Notify(LOCTEXT("NotEnough", "Not enough runes"), EOSFNotifyType::Hint);
		return false;
	}
	Player->Inventory->AddItem(Entry.Item, 1);
	if (Entry.Stock >= 0)
	{
		Player->Progress->AddCounter(GetStockKey(EntryIndex));
	}
	return true;
}

bool AOSFNPC::Sell(AOSFPlayerCharacter* Player, UOSFItemData* Item)
{
	if (!Player || !Item || Item->Category == EOSFItemCategory::KeyItem || Item->bIsFlask || Item->SellPrice <= 0)
	{
		return false;
	}
	if (Player->Equipment->IsItemEquipped(Item) && Player->Inventory->GetItemCount(Item) <= 1)
	{
		Player->Notify(LOCTEXT("Equipped", "Unequip the item first"), EOSFNotifyType::Hint);
		return false;
	}
	if (!Player->Inventory->RemoveItem(Item, 1))
	{
		return false;
	}
	const float Multiplier = Vendor ? Vendor->SellMultiplier : 1.f;
	Player->Inventory->AddCurrency(FMath::RoundToInt(Item->SellPrice * Multiplier));
	return true;
}

#undef LOCTEXT_NAMESPACE
