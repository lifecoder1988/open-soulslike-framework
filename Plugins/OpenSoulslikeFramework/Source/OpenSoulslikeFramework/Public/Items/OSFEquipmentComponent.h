// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFEquipmentComponent.generated.h"

class UOSFItemData;
class UOSFWeaponData;
class UOSFArmorData;
class UOSFTalismanData;
class AOSFWeaponActor;
class USkeletalMeshComponent;
class UOSFBuffData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnEquipmentChanged, UOSFEquipmentComponent*, Equipment);

/**
 * Equipment manager: 3 right-hand + 3 left-hand weapon slots, 4 armor slots, talismans and quick items.
 * Spawns weapon actors, applies armor / talisman modifiers, handles two-handing and equip load.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFEquipmentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static constexpr int32 NumHandSlots = 3;
	static constexpr int32 NumTalismanSlots = 4;
	static constexpr int32 NumQuickSlots = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TArray<TObjectPtr<UOSFWeaponData>> RightHand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TArray<TObjectPtr<UOSFWeaponData>> LeftHand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TMap<EOSFEquipSlot, TObjectPtr<UOSFArmorData>> Armor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TArray<TObjectPtr<UOSFTalismanData>> Talismans;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TArray<TObjectPtr<UOSFItemData>> QuickItems;

	/** Weapon used when a hand is empty. Created at runtime if not set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TObjectPtr<UOSFWeaponData> UnarmedWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	TSubclassOf<AOSFWeaponActor> DefaultWeaponActorClass;

	/** Make sure equipped items are also present in the inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	bool bAddEquippedItemsToInventory = true;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Equipment")
	FOSFOnEquipmentChanged OnEquipmentChanged;

	// --- Equip ---

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	bool EquipWeapon(UOSFWeaponData* Weapon, bool bRightHand, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	bool EquipArmor(UOSFArmorData* ArmorPiece);

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	bool EquipTalisman(UOSFTalismanData* Talisman, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	bool SetQuickItem(UOSFItemData* Item, int32 SlotIndex);

	/** Equips into the most sensible slot (menus). Returns false if the item can't be equipped. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	bool EquipItemAuto(UOSFItemData* Item);

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void UnequipItem(UOSFItemData* Item);

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	bool IsItemEquipped(const UOSFItemData* Item) const;

	// --- Active slots ---

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void CycleWeapon(bool bRightHand);

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void CycleQuickItem();

	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void ToggleTwoHand();

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	bool IsTwoHanding() const { return bTwoHanding; }

	/** Equipped weapon in the active slot of a hand, or the unarmed weapon. */
	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	UOSFWeaponData* GetActiveWeapon(bool bRightHand) const;

	/** Same as GetActiveWeapon but returns null for an empty hand. */
	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	UOSFWeaponData* GetEquippedWeapon(bool bRightHand) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	AOSFWeaponActor* GetWeaponActor(bool bRightHand) const { return bRightHand ? RightWeaponActor : LeftWeaponActor; }

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	UOSFItemData* GetActiveQuickItem() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	int32 GetActiveSlot(bool bRightHand) const { return bRightHand ? ActiveRight : ActiveLeft; }

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	int32 GetActiveQuickSlot() const { return ActiveQuick; }

	// --- Derived values ---

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	float GetEquipLoad() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	float GetEquipLoadRatio() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	EOSFEquipLoad GetEquipLoadCategory() const;

	/** Combined armor negation (0..1) for a damage type. */
	UFUNCTION(BlueprintPure, Category = "OSF|Equipment")
	float GetArmorNegation(FGameplayTag DamageType) const;

	/** Rebuilds weapon actors, modifiers and anim layers. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void RefreshEquipment();

	/** Show / hide weapon actors (cinematics, menus). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment")
	void SetWeaponsHidden(bool bHidden);

	// --- Save support ---
	void SetActiveSlots(int32 InRight, int32 InLeft, int32 InQuick, bool bInTwoHanding);

protected:
	UPROPERTY(Transient)
	TObjectPtr<AOSFWeaponActor> RightWeaponActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AOSFWeaponActor> LeftWeaponActor = nullptr;

	UPROPERTY(Transient)
	TMap<EOSFEquipSlot, TObjectPtr<USkeletalMeshComponent>> ArmorMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOSFBuffData>> AppliedPassiveBuffs;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Equipment")
	int32 ActiveRight = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Equipment")
	int32 ActiveLeft = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Equipment")
	int32 ActiveQuick = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Equipment")
	bool bTwoHanding = false;

	void EnsureSlotArrays();
	void SpawnWeaponActors();
	AOSFWeaponActor* SpawnWeaponActor(UOSFWeaponData* Weapon, bool bRightHand);
	void ApplyModifiers();
	void RefreshArmorMeshes();
	void Changed();
};
