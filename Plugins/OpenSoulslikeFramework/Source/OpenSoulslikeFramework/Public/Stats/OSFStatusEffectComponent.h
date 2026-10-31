// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "OSFTypes.h"
#include "OSFStatusEffectComponent.generated.h"

class UOSFBuffData;

/** Buildup-based status effect (bleed, poison, rot, frostbite...). */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFStatusEffectData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FGameplayTag StatusTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FLinearColor Color = FLinearColor::Red;

	/** Buildup needed to proc. Status resistance modifiers (same tag) add to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	float Threshold = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	float DecayPerSecond = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	float DecayDelay = 2.f;

	// --- On proc ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	float FlatDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	float PercentMaxHealthDamage = 0.f;

	/** Damage over time while the effect is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	float DamagePerSecond = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	float PercentMaxHealthPerSecond = 0.f;

	/** Seconds the effect stays active after proccing (DoT, debuff). 0 = instant only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Proc")
	TObjectPtr<UOSFBuffData> ApplyBuff = nullptr;

	/** Creates the built-in definition for one of the OSF.Status.* tags (used when no asset is configured). */
	static UOSFStatusEffectData* CreateDefault(UObject* Outer, FGameplayTag Tag);
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFStatusState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	float Buildup = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "OSF")
	float ActiveRemaining = 0.f;

	double LastBuildupTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnStatusTriggered, UOSFStatusEffectComponent*, Component, UOSFStatusEffectData*, Effect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnStatusCured, UOSFStatusEffectComponent*, Component, FGameplayTag, StatusTag);

/** Status effect manager: accumulates buildup, procs effects, runs DoTs and decays buildup. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFStatusEffectComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Known effects. Missing OSF.Status.* tags get a built-in default at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Status")
	TArray<TObjectPtr<UOSFStatusEffectData>> Effects;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Status")
	FOSFOnStatusTriggered OnStatusTriggered;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Status")
	FOSFOnStatusCured OnStatusCured;

	/** Adds buildup. Returns true if the effect procced. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Status")
	bool AddBuildup(FGameplayTag StatusTag, float Amount);

	UFUNCTION(BlueprintCallable, Category = "OSF|Status")
	void Cure(FGameplayTag StatusTag);

	UFUNCTION(BlueprintCallable, Category = "OSF|Status")
	void CureAll();

	UFUNCTION(BlueprintPure, Category = "OSF|Status")
	float GetBuildupPercent(FGameplayTag StatusTag) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Status")
	bool IsEffectActive(FGameplayTag StatusTag) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Status")
	float GetThreshold(FGameplayTag StatusTag) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Status")
	UOSFStatusEffectData* FindEffect(FGameplayTag StatusTag) const;

	const TMap<FGameplayTag, FOSFStatusState>& GetStates() const { return States; }

protected:
	UPROPERTY(VisibleInstanceOnly, Category = "OSF|Status")
	TMap<FGameplayTag, FOSFStatusState> States;

	void EnsureDefaults();
	void Trigger(UOSFStatusEffectData* Effect);
	void DealDamage(float Amount);
};
