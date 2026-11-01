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
class UOSFAmmoData;

/** Rounds loaded in a ranged weapon. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFMagazine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFAmmoData> Ammo = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	int32 Rounds = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnEquipmentChanged, UOSFEquipmentComponent*, Equipment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSFOnShotFired, UOSFEquipmentComponent*, Equipment, UOSFWeaponData*, Weapon, int32, Hits);

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

	/** Hand bones / sockets of this character's skeleton, used when a weapon's AttachSocket doesn't exist on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	FName RightHandSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	FName LeftHandSocket = TEXT("hand_l");

	/** Weapon actors are spawned hidden (models that already hold their weapon); traces still use them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	bool bHideWeaponMeshes = false;

	/** Weapons follow the hand's position but keep the owner's facing (imported rigs with unknown hand bone axes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	bool bAlignWeaponsToOwner = false;

	/** Extra rotation for aligned weapons, per hand (e.g. a dagger held point-down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	FRotator RightHandFacingOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Equipment")
	FRotator LeftHandFacingOffset = FRotator::ZeroRotator;

	/** Make sure equipped items are also present in the inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Equipment")
	bool bAddEquippedItemsToInventory = true;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Equipment")
	FOSFOnEquipmentChanged OnEquipmentChanged;

	/** A ranged weapon fired (Hits = pellets that hit a character). */
	UPROPERTY(BlueprintAssignable, Category = "OSF|Equipment")
	FOSFOnShotFired OnShotFired;

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

	// --- Ranged ---

	/** The active ranged weapon (right hand first, then left hand), or null. */
	UFUNCTION(BlueprintPure, Category = "OSF|Equipment|Ranged")
	UOSFWeaponData* GetActiveRangedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment|Ranged")
	FOSFMagazine GetMagazine(const UOSFWeaponData* Weapon) const;

	/** Rounds of the loaded ammo type left in the inventory. */
	UFUNCTION(BlueprintPure, Category = "OSF|Equipment|Ranged")
	int32 GetReserveRounds(const UOSFWeaponData* Weapon) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Equipment|Ranged")
	bool CanReload(const UOSFWeaponData* Weapon) const;

	/** Moves rounds from the inventory into the magazine. Returns the number of rounds loaded. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment|Ranged")
	int32 Reload(UOSFWeaponData* Weapon);

	/** Fills the magazine without using reserve rounds (e.g. a reward). Ammo = null keeps the loaded type. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment|Ranged")
	void FillMagazine(UOSFWeaponData* Weapon, UOSFAmmoData* Ammo = nullptr);

	/** Spends one round. False if the magazine is empty. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment|Ranged")
	bool ConsumeRound(UOSFWeaponData* Weapon);

	/** Switches to the next compatible ammo type that has rounds; loaded rounds go back to the inventory. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment|Ranged")
	bool CycleAmmo(UOSFWeaponData* Weapon);

	/** Selects a specific ammo type (unloads the current one). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Equipment|Ranged")
	bool SelectAmmo(UOSFWeaponData* Weapon, UOSFAmmoData* Ammo);

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

	/** Loaded rounds per ranged weapon (by item id). */
	UPROPERTY(Transient)
	TMap<FName, FOSFMagazine> Magazines;

	FOSFMagazine& FindOrAddMagazine(const UOSFWeaponData* Weapon);

	void EnsureSlotArrays();
	void SpawnWeaponActors();
	AOSFWeaponActor* SpawnWeaponActor(UOSFWeaponData* Weapon, bool bRightHand);
	void ApplyModifiers();
	void RefreshArmorMeshes();
	void Changed();
};
