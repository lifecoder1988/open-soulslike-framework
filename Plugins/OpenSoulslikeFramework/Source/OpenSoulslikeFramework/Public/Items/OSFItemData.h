// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OSFTypes.h"
#include "OSFItemData.generated.h"

class UTexture2D;
class UStaticMesh;
class USkeletalMesh;
class UAnimMontage;
class UAnimInstance;
class USoundBase;
class UMaterialInterface;
class UOSFBuffData;
class UOSFAction;
class AOSFWeaponActor;

UENUM(BlueprintType)
enum class EOSFItemEffectType : uint8
{
	RestoreStat,	// Tag = stat, Magnitude = amount (or percent if bPercent)
	ApplyBuff,		// Buff
	CureStatus,		// Tag = status (empty = all)
	GrantCurrency,	// Magnitude = amount
	AddBuildup,		// Tag = status, applied to the user (e.g. a cursed item)
	GrantProgress	// Tag = progress tag
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFItemEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	EOSFItemEffectType Type = EOSFItemEffectType::RestoreStat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Magnitude = 0.f;

	/** Magnitude is a fraction of the stat max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bPercent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UOSFBuffData> Buff = nullptr;
};

/**
 * Base item definition. Items are identified by ItemId (also their primary asset id "OSFItem:ItemId"),
 * which is what gets written to save games.
 */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", AssetRegistrySearchable)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Played when the item takes effect (drink, injection, throw...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USoundBase> UseSound = nullptr;

	/** Effect on the user when the item takes effect (healing glow...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FOSFEffect UseEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EOSFItemCategory Category = EOSFItemCategory::Consumable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStack = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 SellPrice = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float Weight = 0.f;

	// --- Usable items ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	TArray<FOSFItemEffect> UseEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	bool bConsumeOnUse = true;

	/** Flasks: charges are refilled when resting instead of being consumed for good. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	bool bIsFlask = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	TObjectPtr<UAnimMontage> UseMontage = nullptr;

	/** Time before the effect is applied (procedural or when no montage notify is used). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	float UseDelay = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	float UseDuration = 1.1f;

	/** Movement speed multiplier while using the item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	float UseMoveSpeedScale = 0.35f;

	bool IsUsable() const { return UseEffects.Num() > 0; }
	bool IsEquippable() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** Ammunition fired by ranged weapons. Reserve rounds are regular inventory items; loaded rounds live in the magazine. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFAmmoData : public UOSFItemData
{
	GENERATED_BODY()

public:
	UOSFAmmoData();

	/** Damage per damage type of one projectile / pellet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TMap<FGameplayTag, float> Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	float PoiseDamage = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TMap<FGameplayTag, float> StatusBuildup;

	/** Projectiles per shot (shotgun shells). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 1))
	int32 Pellets = 1;

	/** Source tags put on the damage spec (OSF.Source.Projectile by default): perilous attacks can be countered by them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FGameplayTagContainer SourceTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.4f);

	/** Bullets: no hit stop, a nudge and a small shake by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FOSFHitFeel HitFeel;

	/** Effect where this round hits a character (blood mist, sha burn...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FOSFEffect ImpactEffect;
};

/** Ranged profile of a weapon. A weapon can have a melee moveset (butt strikes, bayonet) and a ranged profile at once. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFRangedStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	bool bIsRanged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = 1))
	int32 MagazineSize = 30;

	/** Compatible ammunition, in cycling order. The first one is loaded by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	TArray<TObjectPtr<UOSFAmmoData>> Ammo;

	/** Seconds between shots (recovery of the shoot action). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = 0.02))
	float FireInterval = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (ClampMin = 0.1))
	float ReloadTime = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	float Range = 6000.f;

	/** Random cone half angle (degrees) per projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	float SpreadDegrees = 1.2f;

	/** Aim assist: snaps to the closest enemy inside this cone (degrees) when no lock-on target is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	float AimAssistDegrees = 8.f;

	/** Multiplier on ammo damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	float DamageScale = 1.f;

	/** Muzzle position relative to the weapon actor (along the barrel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FVector MuzzleOffset = FVector(60.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FOSFAnimClip FireClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FOSFAnimClip ReloadClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	TObjectPtr<USoundBase> FireSound = nullptr;

	/** Muzzle flash at the barrel each shot (oriented along the shot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	FOSFEffect MuzzleEffect;

	/** Tracer / muzzle flash material (an emissive one reads much better); "Color" parameter = ammo tracer colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	TObjectPtr<UMaterialInterface> TracerMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	TObjectPtr<USoundBase> ReloadSound = nullptr;
};

/** Weapons (and shields: Category = Shield). */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFWeaponData : public UOSFItemData
{
	GENERATED_BODY()

public:
	UOSFWeaponData();

	// --- Visuals ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TSubclassOf<AOSFWeaponActor> WeaponActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Skeletal weapon mesh (tassels, chains...); used instead of StaticMesh when set, with the same MeshTransform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TObjectPtr<USkeletalMesh> WeaponSkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FTransform MeshTransform;

	/** Socket on the character mesh (falls back to a procedural hand pivot when the character has no skeleton). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName AttachSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FName TwoHandAttachSocket = TEXT("hand_r");

	/** Linked anim layer (weapon specific animset / stance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	/**
	 * Clip-animation stance while this weapon is in the active right hand (rifle carry, greatsword guard):
	 * replaces the character's IdleClip / WalkClip / RunClip. Only used when the clip's skeleton matches the
	 * character's mesh; empty clips keep the character's own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceIdleClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceWalkClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceRunClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceSprintClip;

	/** Dodge roll / backstep / guard pose with this weapon out (same rules as the stance locomotion). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceDodgeClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceBackstepClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	FOSFAnimClip StanceGuardClip;

	/**
	 * MeshTransform is the weapon mesh's pose in the AttachSocket bone's own space (e.g. a grip authored against the
	 * holder's skeleton in a content tool): the equipment scales it with the holder mesh, since weapons attach
	 * without inheriting the mesh scale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	bool bMeshTransformInBoneSpace = false;

	/**
	 * Held with both hands (long guns): the off-hand weapon stays equipped but sheathed (hidden) while this one is
	 * in the active right hand, and is drawn only while parrying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	bool bSheathesOffHand = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TObjectPtr<USoundBase> SwingSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	TObjectPtr<USoundBase> ImpactSound = nullptr;

	/** Length of the blade for the procedural trace (from the attach point along +X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
	float BladeLength = 120.f;

	// --- Damage ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	TMap<FGameplayTag, float> BaseDamage;

	/** Attribute scaling coefficients (E=0.25, D=0.45, C=0.7, B=1.0, A=1.4, S=1.8). Bonus = Base * Coef * (Attr / 100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	TMap<FGameplayTag, float> Scaling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	TMap<FGameplayTag, int32> Requirements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	float PoiseDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	float CriticalMultiplier = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	TMap<FGameplayTag, float> StatusBuildup;

	// --- Guarding ---

	/** Damage absorbed when guarding, per type (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Guard")
	TMap<FGameplayTag, float> GuardAbsorption;

	/** Reduces stamina damage while guarding (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Guard")
	float GuardBoost = 0.3f;

	/** The left-hand weapon skill becomes a parry when true (shields, parrying daggers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Guard")
	bool bCanParry = false;

	// --- Moveset ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	TArray<FOSFAttackDefinition> LightCombo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	TArray<FOSFAttackDefinition> HeavyCombo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	FOSFAttackDefinition RunningAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	FOSFAttackDefinition JumpingAttack;

	/** Weapon skill / "Ash of War". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	FOSFAttackDefinition SkillAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	FText SkillName;

	/** Optional custom action class for the weapon skill (overrides SkillAttack). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	TSubclassOf<UOSFAction> SkillActionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	TObjectPtr<UAnimMontage> RiposteMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Moveset")
	TObjectPtr<UAnimMontage> BackstabMontage = nullptr;

	// --- Ranged ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ranged")
	FOSFRangedStats Ranged;

	bool IsRanged() const { return Ranged.bIsRanged; }

	const FOSFAttackDefinition* GetAttack(EOSFAttackType Type, int32 ComboIndex) const;
	int32 GetComboLength(EOSFAttackType Type) const;

	/** Builds a reasonable default 3-hit light combo, 2-hit heavy combo, running/jumping/skill attacks. */
	void BuildDefaultMoveset(float Speed = 1.f);
};

/** Armor piece. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFArmorData : public UOSFItemData
{
	GENERATED_BODY()

public:
	UOSFArmorData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	EOSFEquipSlot Slot = EOSFEquipSlot::Chest;

	/** Modular skeletal mesh (follows the character mesh via leader pose). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** Damage negation per damage type, in percent (0..100). Multiple pieces stack multiplicatively. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	TMap<FGameplayTag, float> Negation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	float Poise = 5.f;

	/** Additional stat / attribute / resistance modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	TArray<FOSFStatModifier> Modifiers;
};

/** Talisman / ring: passive modifiers and an optional permanent buff. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFTalismanData : public UOSFItemData
{
	GENERATED_BODY()

public:
	UOSFTalismanData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talisman")
	TArray<FOSFStatModifier> Modifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talisman")
	TObjectPtr<UOSFBuffData> PassiveBuff = nullptr;
};

/** Crafting recipe. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFCraftingRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FOSFItemStack> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FOSFItemStack Result;

	/** Progress tags required to know the recipe (e.g. a cookbook). Empty = always known. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer RequiredProgress;
};
