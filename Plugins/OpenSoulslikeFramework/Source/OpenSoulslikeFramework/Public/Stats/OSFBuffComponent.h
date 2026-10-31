// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "OSFTypes.h"
#include "OSFBuffComponent.generated.h"

/** Data asset describing a buff / debuff. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFBuffData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	FGameplayTag BuffTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	FText DisplayName;

	/** Seconds. <= 0 means it lasts until removed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	float Duration = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	TArray<FOSFStatModifier> StatModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	float AttackPowerMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	float DamageTakenMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	float StaminaRegenMultiplier = 1.f;

	/** Health restored (negative = drained) per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	float HealthPerSecond = 0.f;

	/** Tags owned while the buff is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	FGameplayTagContainer GrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	FLinearColor Color = FLinearColor(1.f, 0.8f, 0.3f);
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFActiveBuff
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFBuffData> Buff = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	float Remaining = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	float Duration = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnBuffChanged, UOSFBuffData*, Buff, bool, bAdded);

/** Buff manager: timed stat modifiers, damage multipliers and granted tags. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFBuffComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Buffs")
	FOSFOnBuffChanged OnBuffChanged;

	/** Applies (or refreshes) a buff. DurationOverride: 0 = asset duration, > 0 = explicit, < 0 = infinite. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Buffs")
	void ApplyBuff(UOSFBuffData* Buff, float DurationOverride = 0.f);

	UFUNCTION(BlueprintCallable, Category = "OSF|Buffs")
	void RemoveBuff(UOSFBuffData* Buff);

	UFUNCTION(BlueprintCallable, Category = "OSF|Buffs")
	void RemoveAllBuffs(bool bOnlyTimed = true);

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	bool HasBuff(const UOSFBuffData* Buff) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	bool HasBuffWithTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	float GetAttackPowerMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	float GetDamageTakenMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	float GetStaminaRegenMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Buffs")
	const TArray<FOSFActiveBuff>& GetActiveBuffs() const { return ActiveBuffs; }

	void AppendGrantedTags(FGameplayTagContainer& OutTags) const;

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Buffs")
	TArray<FOSFActiveBuff> ActiveBuffs;

	void ApplyModifiers(UOSFBuffData* Buff, bool bAdd);
};
