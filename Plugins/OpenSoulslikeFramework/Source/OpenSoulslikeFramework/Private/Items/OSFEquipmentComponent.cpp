// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFWeaponActor.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemRegistry.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Characters/OSFCharacterBase.h"
#include "OSFGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

#define LOCTEXT_NAMESPACE "OSFEquipment"

UOSFEquipmentComponent::UOSFEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultWeaponActorClass = AOSFWeaponActor::StaticClass();
	EnsureSlotArrays();
}

void UOSFEquipmentComponent::EnsureSlotArrays()
{
	RightHand.SetNum(NumHandSlots);
	LeftHand.SetNum(NumHandSlots);
	Talismans.SetNum(NumTalismanSlots);
	QuickItems.SetNum(NumQuickSlots);
}

void UOSFEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureSlotArrays();

	if (!UnarmedWeapon)
	{
		UnarmedWeapon = NewObject<UOSFWeaponData>(this, TEXT("Unarmed"));
		UnarmedWeapon->ItemId = TEXT("Unarmed");
		UnarmedWeapon->DisplayName = LOCTEXT("Unarmed", "Unarmed");
		UnarmedWeapon->BaseDamage.Reset();
		UnarmedWeapon->BaseDamage.Add(OSFTags::Damage_Physical, 30.f);
		UnarmedWeapon->Scaling.Reset();
		UnarmedWeapon->Scaling.Add(OSFTags::Attribute_Strength, 0.5f);
		UnarmedWeapon->PoiseDamage = 5.f;
		UnarmedWeapon->Weight = 0.f;
		UnarmedWeapon->BladeLength = 30.f;
		UnarmedWeapon->BuildDefaultMoveset(1.3f);
		for (FOSFAttackDefinition& Attack : UnarmedWeapon->LightCombo) { Attack.Reach = 130.f; Attack.StaminaCost = 8.f; }
		for (FOSFAttackDefinition& Attack : UnarmedWeapon->HeavyCombo) { Attack.Reach = 140.f; }
	}

	if (bAddEquippedItemsToInventory)
	{
		if (UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(GetOwner()))
		{
			auto Ensure = [Inventory](UOSFItemData* Item)
			{
				if (Item && Inventory->GetItemCount(Item) == 0)
				{
					Inventory->AddItem(Item, 1, false);
				}
			};
			for (UOSFWeaponData* W : RightHand) { Ensure(W); }
			for (UOSFWeaponData* W : LeftHand) { Ensure(W); }
			for (const TPair<EOSFEquipSlot, TObjectPtr<UOSFArmorData>>& Pair : Armor) { Ensure(Pair.Value); }
			for (UOSFTalismanData* T : Talismans) { Ensure(T); }
		}
	}

	// Start on the first non-empty slots.
	for (int32 Index = 0; Index < NumHandSlots; ++Index)
	{
		if (RightHand[Index]) { ActiveRight = Index; break; }
	}
	for (int32 Index = 0; Index < NumHandSlots; ++Index)
	{
		if (LeftHand[Index]) { ActiveLeft = Index; break; }
	}
	RefreshEquipment();
}

void UOSFEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RightWeaponActor) { RightWeaponActor->Destroy(); }
	if (LeftWeaponActor) { LeftWeaponActor->Destroy(); }
	Super::EndPlay(EndPlayReason);
}

bool UOSFEquipmentComponent::EquipWeapon(UOSFWeaponData* Weapon, bool bRightHand, int32 SlotIndex)
{
	EnsureSlotArrays();
	if (!FMath::IsWithinInclusive(SlotIndex, 0, NumHandSlots - 1))
	{
		return false;
	}
	if (Weapon)
	{
		// A weapon can only be in one slot at a time.
		for (TObjectPtr<UOSFWeaponData>& Slot : RightHand) { if (Slot == Weapon) { Slot = nullptr; } }
		for (TObjectPtr<UOSFWeaponData>& Slot : LeftHand) { if (Slot == Weapon) { Slot = nullptr; } }
	}
	(bRightHand ? RightHand : LeftHand)[SlotIndex] = Weapon;
	if (bRightHand) { ActiveRight = SlotIndex; }
	else { ActiveLeft = SlotIndex; }
	RefreshEquipment();
	return true;
}

bool UOSFEquipmentComponent::EquipArmor(UOSFArmorData* ArmorPiece)
{
	if (!ArmorPiece)
	{
		return false;
	}
	Armor.Add(ArmorPiece->Slot, ArmorPiece);
	RefreshEquipment();
	return true;
}

bool UOSFEquipmentComponent::EquipTalisman(UOSFTalismanData* Talisman, int32 SlotIndex)
{
	EnsureSlotArrays();
	if (!FMath::IsWithinInclusive(SlotIndex, 0, NumTalismanSlots - 1))
	{
		return false;
	}
	if (Talisman)
	{
		for (TObjectPtr<UOSFTalismanData>& Slot : Talismans) { if (Slot == Talisman) { Slot = nullptr; } }
	}
	Talismans[SlotIndex] = Talisman;
	RefreshEquipment();
	return true;
}

bool UOSFEquipmentComponent::SetQuickItem(UOSFItemData* Item, int32 SlotIndex)
{
	EnsureSlotArrays();
	if (!FMath::IsWithinInclusive(SlotIndex, 0, NumQuickSlots - 1))
	{
		return false;
	}
	if (Item)
	{
		for (TObjectPtr<UOSFItemData>& Slot : QuickItems) { if (Slot == Item) { Slot = nullptr; } }
	}
	QuickItems[SlotIndex] = Item;
	Changed();
	return true;
}

bool UOSFEquipmentComponent::EquipItemAuto(UOSFItemData* Item)
{
	EnsureSlotArrays();
	if (!Item)
	{
		return false;
	}
	auto FirstFree = [](const auto& Array) -> int32
	{
		for (int32 Index = 0; Index < Array.Num(); ++Index) { if (!Array[Index]) { return Index; } }
		return INDEX_NONE;
	};

	switch (Item->Category)
	{
	case EOSFItemCategory::Weapon:
		return EquipWeapon(Cast<UOSFWeaponData>(Item), true, ActiveRight);
	case EOSFItemCategory::Shield:
		return EquipWeapon(Cast<UOSFWeaponData>(Item), false, ActiveLeft);
	case EOSFItemCategory::Armor:
		return EquipArmor(Cast<UOSFArmorData>(Item));
	case EOSFItemCategory::Talisman:
	{
		const int32 Free = FirstFree(Talismans);
		return EquipTalisman(Cast<UOSFTalismanData>(Item), Free == INDEX_NONE ? 0 : Free);
	}
	case EOSFItemCategory::Consumable:
	{
		if (!Item->IsUsable())
		{
			return false;
		}
		const int32 Free = FirstFree(QuickItems);
		return SetQuickItem(Item, Free == INDEX_NONE ? ActiveQuick : Free);
	}
	default:
		return false;
	}
}

void UOSFEquipmentComponent::UnequipItem(UOSFItemData* Item)
{
	if (!Item)
	{
		return;
	}
	for (TObjectPtr<UOSFWeaponData>& Slot : RightHand) { if (Slot == Item) { Slot = nullptr; } }
	for (TObjectPtr<UOSFWeaponData>& Slot : LeftHand) { if (Slot == Item) { Slot = nullptr; } }
	for (TObjectPtr<UOSFTalismanData>& Slot : Talismans) { if (Slot == Item) { Slot = nullptr; } }
	for (TObjectPtr<UOSFItemData>& Slot : QuickItems) { if (Slot == Item) { Slot = nullptr; } }
	for (auto It = Armor.CreateIterator(); It; ++It)
	{
		if (It.Value() == Item) { It.RemoveCurrent(); }
	}
	RefreshEquipment();
}

bool UOSFEquipmentComponent::IsItemEquipped(const UOSFItemData* Item) const
{
	if (!Item)
	{
		return false;
	}
	for (const UOSFWeaponData* W : RightHand) { if (W == Item) { return true; } }
	for (const UOSFWeaponData* W : LeftHand) { if (W == Item) { return true; } }
	for (const UOSFTalismanData* T : Talismans) { if (T == Item) { return true; } }
	for (const UOSFItemData* Q : QuickItems) { if (Q == Item) { return true; } }
	for (const TPair<EOSFEquipSlot, TObjectPtr<UOSFArmorData>>& Pair : Armor) { if (Pair.Value == Item) { return true; } }
	return false;
}

void UOSFEquipmentComponent::CycleWeapon(bool bRightHand)
{
	EnsureSlotArrays();
	TArray<TObjectPtr<UOSFWeaponData>>& Slots = bRightHand ? RightHand : LeftHand;
	int32& Active = bRightHand ? ActiveRight : ActiveLeft;
	// Cycle through non-empty slots; if every slot is empty stay on 0 (unarmed).
	for (int32 Step = 1; Step <= NumHandSlots; ++Step)
	{
		const int32 Candidate = (Active + Step) % NumHandSlots;
		if (Slots[Candidate] || Step == NumHandSlots)
		{
			Active = Candidate;
			break;
		}
	}
	RefreshEquipment();
}

void UOSFEquipmentComponent::CycleQuickItem()
{
	EnsureSlotArrays();
	for (int32 Step = 1; Step <= NumQuickSlots; ++Step)
	{
		const int32 Candidate = (ActiveQuick + Step) % NumQuickSlots;
		if (QuickItems[Candidate])
		{
			ActiveQuick = Candidate;
			break;
		}
	}
	Changed();
}

void UOSFEquipmentComponent::ToggleTwoHand()
{
	bTwoHanding = !bTwoHanding;
	if (UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(GetOwner()))
	{
		if (bTwoHanding) { Actions->AddStateTag(OSFTags::State_TwoHanding); }
		else { Actions->ClearStateTag(OSFTags::State_TwoHanding); }
	}
	RefreshEquipment();
}

UOSFWeaponData* UOSFEquipmentComponent::GetEquippedWeapon(bool bRightHand) const
{
	const TArray<TObjectPtr<UOSFWeaponData>>& Slots = bRightHand ? RightHand : LeftHand;
	const int32 Active = bRightHand ? ActiveRight : ActiveLeft;
	return Slots.IsValidIndex(Active) ? Slots[Active].Get() : nullptr;
}

UOSFWeaponData* UOSFEquipmentComponent::GetActiveWeapon(bool bRightHand) const
{
	UOSFWeaponData* Weapon = GetEquippedWeapon(bRightHand);
	return Weapon ? Weapon : UnarmedWeapon.Get();
}

UOSFItemData* UOSFEquipmentComponent::GetActiveQuickItem() const
{
	return QuickItems.IsValidIndex(ActiveQuick) ? QuickItems[ActiveQuick].Get() : nullptr;
}

float UOSFEquipmentComponent::GetEquipLoad() const
{
	float Load = 0.f;
	for (const UOSFWeaponData* W : RightHand) { if (W) { Load += W->Weight; } }
	for (const UOSFWeaponData* W : LeftHand) { if (W) { Load += W->Weight; } }
	for (const UOSFTalismanData* T : Talismans) { if (T) { Load += T->Weight; } }
	for (const TPair<EOSFEquipSlot, TObjectPtr<UOSFArmorData>>& Pair : Armor) { if (Pair.Value) { Load += Pair.Value->Weight; } }
	return Load;
}

float UOSFEquipmentComponent::GetEquipLoadRatio() const
{
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	const float Max = Stats ? Stats->GetMax(OSFTags::Stat_EquipLoad) : 0.f;
	return Max > 0.f ? GetEquipLoad() / Max : 0.f;
}

EOSFEquipLoad UOSFEquipmentComponent::GetEquipLoadCategory() const
{
	const float Ratio = GetEquipLoadRatio();
	if (Ratio <= 0.3f) return EOSFEquipLoad::Light;
	if (Ratio <= 0.7f) return EOSFEquipLoad::Medium;
	if (Ratio <= 1.f) return EOSFEquipLoad::Heavy;
	return EOSFEquipLoad::Overloaded;
}

float UOSFEquipmentComponent::GetArmorNegation(FGameplayTag DamageType) const
{
	float Remaining = 1.f;
	for (const TPair<EOSFEquipSlot, TObjectPtr<UOSFArmorData>>& Pair : Armor)
	{
		if (const UOSFArmorData* Piece = Pair.Value)
		{
			if (const float* Value = Piece->Negation.Find(DamageType))
			{
				Remaining *= 1.f - FMath::Clamp(*Value / 100.f, 0.f, 1.f);
			}
		}
	}
	return 1.f - Remaining;
}

void UOSFEquipmentComponent::RefreshEquipment()
{
	SpawnWeaponActors();
	ApplyModifiers();
	RefreshArmorMeshes();

	if (UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(GetOwner()))
	{
		const UOSFWeaponData* Right = GetEquippedWeapon(true);
		Actions->SetActionOverride(OSFTags::Action_WeaponSkill, Right ? Right->SkillActionClass : nullptr);
	}

	// Weapon specific animset (linked anim layers).
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			const UOSFWeaponData* Right = GetActiveWeapon(true);
			if (Right && Right->AnimLayerClass && Mesh->GetAnimInstance())
			{
				Mesh->LinkAnimClassLayers(Right->AnimLayerClass);
			}
		}
	}
	Changed();
}

void UOSFEquipmentComponent::SpawnWeaponActors()
{
	if (RightWeaponActor) { RightWeaponActor->Destroy(); RightWeaponActor = nullptr; }
	if (LeftWeaponActor) { LeftWeaponActor->Destroy(); LeftWeaponActor = nullptr; }
	if (!GetWorld() || !GetOwner() || !GetWorld()->IsGameWorld())
	{
		return;
	}
	RightWeaponActor = SpawnWeaponActor(GetEquippedWeapon(true), true);
	LeftWeaponActor = SpawnWeaponActor(GetEquippedWeapon(false), false);
}

AOSFWeaponActor* UOSFEquipmentComponent::SpawnWeaponActor(UOSFWeaponData* Weapon, bool bRightHand)
{
	if (!Weapon)
	{
		return nullptr;
	}
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	TSubclassOf<AOSFWeaponActor> Class = Weapon->WeaponActorClass ? Weapon->WeaponActorClass : DefaultWeaponActorClass;
	if (!World || !Owner || !Class)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOSFWeaponActor* Actor = World->SpawnActor<AOSFWeaponActor>(Class, Owner->GetActorTransform(), Params);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->InitializeWeapon(Weapon);

	const ACharacter* Character = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const FName Socket = (bTwoHanding && bRightHand) ? Weapon->TwoHandAttachSocket : (bRightHand ? Weapon->AttachSocket : FName(TEXT("hand_l")));
	const AOSFCharacterBase* OSFCharacter = Cast<AOSFCharacterBase>(Owner);
	if (Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->DoesSocketExist(Socket))
	{
		Actor->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	}
	else if (OSFCharacter && OSFCharacter->GetProceduralHand(bRightHand))
	{
		Actor->AttachToComponent(OSFCharacter->GetProceduralHand(bRightHand), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else
	{
		Actor->AttachToActor(Owner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	return Actor;
}

void UOSFEquipmentComponent::SetWeaponsHidden(bool bHidden)
{
	if (RightWeaponActor) { RightWeaponActor->SetActorHiddenInGame(bHidden); }
	if (LeftWeaponActor) { LeftWeaponActor->SetActorHiddenInGame(bHidden); }
}

void UOSFEquipmentComponent::ApplyModifiers()
{
	UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	if (!Stats)
	{
		return;
	}
	TArray<FOSFStatModifier> Modifiers;
	for (const TPair<EOSFEquipSlot, TObjectPtr<UOSFArmorData>>& Pair : Armor)
	{
		if (const UOSFArmorData* Piece = Pair.Value)
		{
			Modifiers.Append(Piece->Modifiers);
			FOSFStatModifier& Poise = Modifiers.AddDefaulted_GetRef();
			Poise.Tag = OSFTags::Stat_Poise;
			Poise.Additive = Piece->Poise;
		}
	}
	UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(GetOwner());
	for (UOSFTalismanData* Talisman : Talismans)
	{
		if (Talisman)
		{
			Modifiers.Append(Talisman->Modifiers);
		}
	}
	Stats->SetModifiers(this, TEXT("Equipment"), Modifiers);

	if (Buffs)
	{
		// Passive talisman buffs stay active (no duration) while the talisman is equipped.
		for (UOSFBuffData* Buff : AppliedPassiveBuffs)
		{
			Buffs->RemoveBuff(Buff);
		}
		AppliedPassiveBuffs.Reset();
		for (UOSFTalismanData* Talisman : Talismans)
		{
			if (Talisman && Talisman->PassiveBuff)
			{
				Buffs->ApplyBuff(Talisman->PassiveBuff, -1.f);
				AppliedPassiveBuffs.Add(Talisman->PassiveBuff);
			}
		}
	}
}

void UOSFEquipmentComponent::RefreshArmorMeshes()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Leader = Character ? Character->GetMesh() : nullptr;
	if (!Leader || !Leader->GetSkeletalMeshAsset())
	{
		return;
	}
	for (const EOSFEquipSlot Slot : { EOSFEquipSlot::Head, EOSFEquipSlot::Chest, EOSFEquipSlot::Arms, EOSFEquipSlot::Legs })
	{
		const TObjectPtr<UOSFArmorData>* Piece = Armor.Find(Slot);
		USkeletalMesh* MeshAsset = (Piece && *Piece) ? (*Piece)->SkeletalMesh.Get() : nullptr;
		TObjectPtr<USkeletalMeshComponent>& Component = ArmorMeshes.FindOrAdd(Slot);
		if (!MeshAsset)
		{
			if (Component) { Component->SetSkeletalMesh(nullptr); }
			continue;
		}
		if (!Component)
		{
			Component = NewObject<USkeletalMeshComponent>(Character);
			Component->SetupAttachment(Leader);
			Component->RegisterComponent();
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Component->SetSkeletalMesh(MeshAsset);
		Component->SetLeaderPoseComponent(Leader);
	}
}

void UOSFEquipmentComponent::SetActiveSlots(int32 InRight, int32 InLeft, int32 InQuick, bool bInTwoHanding)
{
	ActiveRight = FMath::Clamp(InRight, 0, NumHandSlots - 1);
	ActiveLeft = FMath::Clamp(InLeft, 0, NumHandSlots - 1);
	ActiveQuick = FMath::Clamp(InQuick, 0, NumQuickSlots - 1);
	if (bTwoHanding != bInTwoHanding)
	{
		ToggleTwoHand();
	}
	else
	{
		RefreshEquipment();
	}
}

void UOSFEquipmentComponent::Changed()
{
	OnEquipmentChanged.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
