// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "OSFBossComponent.generated.h"

class UOSFBuffData;
class USoundBase;
class UAudioComponent;
class UOSFStatComponent;

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFBossPhase
{
	GENERATED_BODY()

	/** Enter this phase once health drops to this fraction (e.g. 0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss", meta = (ClampMin = 0, ClampMax = 1))
	float HealthThreshold = 0.5f;

	/** Buff applied when entering the phase (faster, stronger...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	TObjectPtr<UOSFBuffData> PhaseBuff = nullptr;

	/** Seconds of hyper armor / roar on transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	float TransitionDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	FLinearColor PhaseColor = FLinearColor(0.8f, 0.2f, 0.1f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnBossPhaseChanged, UOSFBossComponent*, Boss, int32, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnBossFightChanged, UOSFBossComponent*, Boss, bool, bActive);

/** Boss manager: fight state, health-threshold phases, boss bar name, music and defeat progression. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFBossComponent();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Boss")
	bool bIsBoss = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Boss")
	FText BossName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Boss")
	TArray<FOSFBossPhase> Phases;

	/** Progress tag granted when defeated (optional, a flag "Boss.<SaveId>" is always granted). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Boss")
	FGameplayTag DefeatedProgressTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Boss")
	TObjectPtr<USoundBase> BossMusic = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Boss")
	FOSFOnBossPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Boss")
	FOSFOnBossFightChanged OnFightChanged;

	UFUNCTION(BlueprintCallable, Category = "OSF|Boss")
	void BeginFight();

	UFUNCTION(BlueprintCallable, Category = "OSF|Boss")
	void EndFight();

	UFUNCTION(BlueprintPure, Category = "OSF|Boss")
	bool IsFightActive() const { return bFightActive; }

	UFUNCTION(BlueprintPure, Category = "OSF|Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	/** Re-evaluates phases against current health. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Boss")
	void UpdatePhase();

	void ResetBoss();

protected:
	UFUNCTION()
	void HandleStatChanged(UOSFStatComponent* StatComponent, FGameplayTag Stat, float NewValue, float Delta);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	int32 CurrentPhase = 0;
	bool bFightActive = false;
};
