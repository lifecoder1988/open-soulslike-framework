// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFAICombatComponent.generated.h"

/** One attack the AI can choose. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFAIAbility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FName Name = TEXT("Attack");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FOSFAttackDefinition Attack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MaxRange = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = 0))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = 0))
	float Cooldown = 2.f;

	/** Boss phases (0-based) in which the ability can be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 MinPhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 MaxPhase = 99;

	/** Chain into another ability (by index) right after this one, for multi-hit strings. -1 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 FollowUp = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = 0, ClampMax = 1))
	float FollowUpChance = 0.f;
};

/**
 * AI combat manager: ability selection (range, weight, cooldown, boss phase), combo strings,
 * reactive guarding / dodging and aggression pacing.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFAICombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFAICombatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI")
	TArray<FOSFAIAbility> Abilities;

	/** Seconds between attacks (random in range) - the strafing / spacing time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI")
	FVector2D AttackInterval = FVector2D(0.8f, 2.2f);

	/** Distance kept while strafing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI")
	float PreferredDistance = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI", meta = (ClampMin = 0, ClampMax = 1))
	float GuardChance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI", meta = (ClampMin = 0, ClampMax = 1))
	float DodgeChance = 0.15f;

	/** Chance to try a backstep-and-punish when the player whiffs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI", meta = (ClampMin = 0, ClampMax = 1))
	float PunishChance = 0.4f;

	/**
	 * Group pacing: at most this many regular enemies attack the same target at once; the others keep circling at
	 * PreferredDistance until a slot frees up (bosses ignore it). 0 = no limit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AI", meta = (ClampMin = 0))
	int32 MaxSimultaneousAttackers = 2;

	/** Picks an ability valid at the given distance / phase (weighted random). INDEX_NONE if none. */
	UFUNCTION(BlueprintCallable, Category = "OSF|AI")
	int32 SelectAbility(float DistanceToTarget, int32 Phase) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|AI")
	bool ExecuteAbility(int32 Index);

	UFUNCTION(BlueprintPure, Category = "OSF|AI")
	bool IsOnCooldown(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "OSF|AI")
	float GetMaxAbilityRange(int32 Phase) const;

	UFUNCTION(BlueprintPure, Category = "OSF|AI")
	int32 GetLastAbility() const { return LastAbility; }

	/** Returns a follow up ability index if the last ability chains, else INDEX_NONE. */
	int32 RollFollowUp() const;

	/** Adds a simple default moveset (light slash, heavy overhead, lunge) when no abilities are configured. */
	void EnsureDefaultAbilities();

protected:
	TMap<int32, double> CooldownUntil;
	int32 LastAbility = INDEX_NONE;
	double Now() const;
};
