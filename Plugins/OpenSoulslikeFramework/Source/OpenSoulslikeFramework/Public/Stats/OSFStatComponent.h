// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFStatComponent.generated.h"

class UCurveFloat;

/** Contribution of an attribute to a stat's max value. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFAttributeScaling
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTag Attribute;

	/** Linear bonus per attribute point (used when Curve is null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float PerPoint = 0.f;

	/** Optional curve: X = attribute value, Y = bonus. Use it for soft caps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TObjectPtr<UCurveFloat> Curve = nullptr;
};

/** Static configuration of a stat (resource). */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFStatDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float BaseMax = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float RegenPerSecond = 0.f;

	/** Seconds after the last consumption before regeneration resumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float RegenDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TArray<FOSFAttributeScaling> AttributeScaling;
};

/** Runtime state of a stat. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFStatValue
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	float Current = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	float Max = 0.f;

	/** World time until which regen is suppressed. */
	double RegenBlockedUntil = 0.0;
};

USTRUCT()
struct FOSFModifierSource
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UObject> Source;

	UPROPERTY()
	FName Key;

	UPROPERTY()
	TArray<FOSFStatModifier> Modifiers;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOSFOnStatChanged, UOSFStatComponent*, StatComponent, FGameplayTag, Stat, float, NewValue, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnStatDepleted, UOSFStatComponent*, StatComponent, FGameplayTag, Stat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnStatsRecalculated, UOSFStatComponent*, StatComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnLevelChanged, UOSFStatComponent*, StatComponent, int32, NewLevel);

/**
 * Stat & Attribute manager.
 * - Stats are resources (Health/FP/Stamina/Poise...) with Current/Max and regeneration.
 * - Attributes (Vigor/Endurance/Strength...) are integer levels that feed stat max values and weapon scaling.
 * - Modifiers from equipment, talismans and buffs are registered per source and stacked.
 * - Character level = sum of attributes - LevelOffset, levelled up with currency.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFStatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Stat definitions. Defaults: Health/FP/Stamina/Poise/EquipLoad scaled by Vigor/Mind/Endurance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Stats")
	TArray<FOSFStatDefinition> StatDefinitions;

	/** Starting attribute values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Stats")
	TMap<FGameplayTag, int32> Attributes;

	/** Level = sum(attributes) - LevelOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Stats")
	int32 LevelOffset = 79;

	/** Multiplies stamina regen while guarding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Stats")
	float GuardingStaminaRegenScale = 0.35f;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Stats")
	FOSFOnStatChanged OnStatChanged;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Stats")
	FOSFOnStatDepleted OnStatDepleted;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Stats")
	FOSFOnStatsRecalculated OnStatsRecalculated;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Stats")
	FOSFOnLevelChanged OnLevelChanged;

	// --- Stats -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	float GetCurrent(FGameplayTag Stat) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	float GetMax(FGameplayTag Stat) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	float GetPercent(FGameplayTag Stat) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	bool HasStat(FGameplayTag Stat) const { return Stats.Contains(Stat); }

	/** Adds Delta to the current value (clamped). Returns the applied delta. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	float ModifyCurrent(FGameplayTag Stat, float Delta, bool bBlockRegen = false);

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void SetCurrent(FGameplayTag Stat, float NewValue);

	/** Consumes Amount if the stat is > 0 (souls rule: an action is allowed as long as you have *some* stamina). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	bool TryConsume(FGameplayTag Stat, float Amount, bool bRequireFullAmount = false);

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void RestoreAll();

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void BlockRegen(FGameplayTag Stat, float Seconds);

	/** Optional regen multiplier hook (stamina regen is slowed while guarding, buffs can scale it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Stats")
	TMap<FGameplayTag, float> RegenMultipliers;

	// --- Attributes & level -----------------------------------------------------

	/** Base attribute value + modifiers. */
	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	int32 GetAttribute(FGameplayTag Attribute) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	int32 GetBaseAttribute(FGameplayTag Attribute) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void SetBaseAttribute(FGameplayTag Attribute, int32 Value);

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	int32 GetLevel() const;

	/** Currency required to go from Level to Level+1. Override in Blueprint for custom curves. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "OSF|Stats")
	int32 GetLevelUpCost(int32 FromLevel) const;

	/** Cost to apply the given attribute increments from the current level. */
	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	int32 GetCostForPoints(int32 Points) const;

	/** Spends currency from the owner's inventory and raises the attributes. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	bool LevelUp(const TMap<FGameplayTag, int32>& AttributeIncrements);

	// --- Modifiers --------------------------------------------------------------

	/** Replaces all modifiers registered under (Source, Key). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void SetModifiers(UObject* Source, FName Key, const TArray<FOSFStatModifier>& Modifiers);

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void RemoveModifiers(UObject* Source, FName Key);

	/** Sum of additive modifiers for any tag (also used for damage negation & status resistance). */
	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	float GetModifierAdditive(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Stats")
	float GetModifierMultiplier(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Stats")
	void RecalculateStats();

	const TMap<FGameplayTag, FOSFStatValue>& GetAllStats() const { return Stats; }
	const TMap<FGameplayTag, int32>& GetAllBaseAttributes() const { return Attributes; }

	/** Builds the default soulslike stat set. Call from constructors of subclasses that want to start from defaults. */
	static TArray<FOSFStatDefinition> MakeDefaultStatDefinitions();
	static TMap<FGameplayTag, int32> MakeDefaultAttributes();

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Stats")
	TMap<FGameplayTag, FOSFStatValue> Stats;

	UPROPERTY()
	TArray<FOSFModifierSource> ModifierSources;

	bool bInitialized = false;

	void InitializeStats();
	float ComputeMax(const FOSFStatDefinition& Def) const;
	const FOSFStatDefinition* FindDefinition(FGameplayTag Stat) const;
	double GetWorldTime() const;
};
