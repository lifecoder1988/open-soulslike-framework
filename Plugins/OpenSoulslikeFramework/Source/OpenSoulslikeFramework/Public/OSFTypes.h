// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "OSFTypes.generated.h"

class UAnimMontage;
class UAnimSequence;
class UOSFItemData;
class UOSFBuffData;

/** Result of an incoming hit after guard / parry / i-frame resolution. */
UENUM(BlueprintType)
enum class EOSFHitResult : uint8
{
	Ignored,		// i-frames, already dead, friendly fire...
	Hit,
	Guarded,
	PerfectGuarded,
	GuardBroken,
	Parried,
	Killed
};

/** How the victim reacts physically to a hit. */
UENUM(BlueprintType)
enum class EOSFHitReaction : uint8
{
	None,
	Flinch,
	Stagger,
	GuardBreak,
	PoiseBreak,	// stance broken - open to a riposte
	Parried		// attacker got parried - open to a riposte
};

UENUM(BlueprintType)
enum class EOSFAttackType : uint8
{
	Light,
	Heavy,
	Running,
	Jumping,
	Skill
};

UENUM(BlueprintType)
enum class EOSFItemCategory : uint8
{
	Consumable,
	Weapon,
	Shield,
	Armor,
	Talisman,
	KeyItem,
	Material,
	Ammo
};

UENUM(BlueprintType)
enum class EOSFEquipSlot : uint8
{
	None,
	RightHand,
	LeftHand,
	Head,
	Chest,
	Arms,
	Legs,
	Talisman,
	QuickItem
};

UENUM(BlueprintType)
enum class EOSFEquipLoad : uint8
{
	Light,		// <= 30%: fast roll
	Medium,		// <= 70%: medium roll
	Heavy,		// <= 100%: fat roll
	Overloaded	// > 100%: no roll, slow walk
};

/** Linear modifier applied to a stat max value or an attribute. Final = (Base + Sum(Additive)) * Product(Multiplier). */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFStatModifier
{
	GENERATED_BODY()

	/** Stat, attribute, damage-negation (OSF.Damage.*) or status-resistance (OSF.Status.*) tag to modify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Additive = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Multiplier = 1.f;
};

/** Procedural timing used by actions when no montage is provided (or as the source of truth for AI/tests). */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFActionTiming
{
	GENERATED_BODY()

	/** Seconds before the active (damaging / invincible) window opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float Windup = 0.35f;

	/** Length of the active window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float Active = 0.2f;

	/** Recovery after the active window. The combo / cancel window lives inside the recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float Recovery = 0.45f;

	/** Fraction (0..1) of the recovery after which the action can be chained / cancelled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0, ClampMax = 1))
	float CancelWindowStart = 0.25f;

	float GetTotal() const { return Windup + Active + Recovery; }
};

/**
 * A trimmed animation sequence played by UOSFClipAnimInstance (no anim blueprint or montage needed).
 * Mirrors the clip binding of asset pipelines: sequence, [Start, End] in seconds, play rate and looping.
 */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFAnimClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UAnimSequence> Sequence = nullptr;

	/** Clip start inside the sequence (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float Start = 0.f;

	/** Clip end inside the sequence (seconds). <= Start means "until the end of the sequence". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float End = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0.05))
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float BlendIn = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0))
	float BlendOut = 0.2f;

	bool IsValid() const { return Sequence != nullptr; }

	/** Clip end, resolved against the sequence length. */
	float GetEnd() const;

	/** Real-time duration: (End - Start) / PlayRate. */
	float GetDuration() const;
};

/** One attack of a moveset (combo step, heavy, running, jumping, skill...). */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFAttackDefinition
{
	GENERATED_BODY()

	/** Optional montage. If set, timing comes from the montage and its OSF anim notifies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FName MontageSection = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float MontagePlayRate = 1.f;

	/**
	 * Optional clip for characters driven by UOSFClipAnimInstance (no montage). The procedural Timing still
	 * drives the windows; it is stretched to the clip duration so the hit lands where Timing puts it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FOSFAnimClip Clip;

	/** Used when Montage is null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FOSFActionTiming Timing;

	/** Damage multiplier applied to weapon attack rating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float MotionValue = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float PoiseDamageMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float StaminaCost = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float FPCost = 0.f;

	/** Fallback trace (no weapon sockets): sphere sweep this far in front of the owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Reach = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float TraceRadius = 45.f;

	/** Forward velocity applied at the start of the active window (lunges). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Lunge = 250.f;

	/** Grants hyper armor (no stagger) during windup + active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bHyperArmor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bCanBeGuarded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bCanBeParried = true;

	/** Visual swing used by the procedural (mesh-less) presentation: yaw sweep in degrees (negative = right to left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	float SwingYaw = -110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	float SwingPitch = 0.f;

	/** Extra damage per damage type added on top of the weapon (e.g. a fire skill). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TMap<FGameplayTag, float> BonusDamage;

	/**
	 * Perilous attack: during the windup the attacker is OSF.State.Perilous. A hit carrying any of CounterTags
	 * (e.g. OSF.Source.Projectile) in that window counters the attack: it is cancelled, the attacker takes
	 * CounterPoiseDamage (ignoring hyper armor) and staggers or has its stance broken.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Perilous")
	bool bPerilous = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Perilous", meta = (EditCondition = "bPerilous"))
	FGameplayTagContainer CounterTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Perilous", meta = (EditCondition = "bPerilous"))
	float CounterPoiseDamage = 60.f;
};

/** Fully resolved outgoing damage. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFDamageSpec
{
	GENERATED_BODY()

	/** Damage per damage type (OSF.Damage.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TMap<FGameplayTag, float> Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float PoiseDamage = 10.f;

	/** Status buildup per status effect tag (OSF.Status.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TMap<FGameplayTag, float> StatusBuildup;

	/** Stamina damage dealt to a guarding victim, as a multiplier of total damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float GuardStaminaDamageScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bCanBeGuarded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bCanBeParried = true;

	/** Finisher (riposte / backstab) damage ignores guard and poise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bIsCritical = false;

	/** Describes the source of the hit (OSF.Source.Melee / Projectile / Explosion...), used by counters and reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTagContainer SourceTags;

	float GetTotalDamage() const
	{
		float Total = 0.f;
		for (const TPair<FGameplayTag, float>& Pair : Damage) { Total += Pair.Value; }
		return Total;
	}
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFDamageEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FOSFDamageSpec Spec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FVector HitLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFHitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	EOSFHitResult Result = EOSFHitResult::Ignored;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	EOSFHitReaction Reaction = EOSFHitReaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float DamageDealt = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The hit countered a perilous attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	bool bCountered = false;

	/** Fraction of the raw damage removed by negation (0..1): high values mean the hit barely worked ("ricochet"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float NegatedFraction = 0.f;
};

/** Item + count pair (inventory entries, loot, recipes, rewards). */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UOSFItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 1))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UOSFItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 1))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF", meta = (ClampMin = 0, ClampMax = 1))
	float Chance = 1.f;
};

namespace OSF
{
	/** Returns the first component of the given class on an actor (null-safe). */
	template <typename T>
	T* Get(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<T>() : nullptr;
	}
}
