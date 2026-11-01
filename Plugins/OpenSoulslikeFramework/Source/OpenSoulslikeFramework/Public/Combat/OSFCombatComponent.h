// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFCombatComponent.generated.h"

class UAnimMontage;
class UOSFActionComponent;
class UOSFStatComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnDamaged, UOSFCombatComponent*, Combat, const FOSFHitInfo&, HitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnDeath, UOSFCombatComponent*, Combat, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSFOnHitLanded, UOSFCombatComponent*, Combat, AActor*, Victim, const FOSFHitInfo&, HitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnHitReaction, UOSFCombatComponent*, Combat, EOSFHitReaction, Reaction);

/**
 * Combat manager: resolves incoming hits (i-frames, parry, perfect guard, guard, guard break, damage negation,
 * poise / stance break, status buildup, death) and finds finisher (riposte / backstab) opportunities.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Characters on the same team don't damage each other. 0 = player, 1 = enemies by convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat")
	uint8 TeamId = 1;

	/** Half angle (degrees) in front of the character covered by the guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat")
	float GuardHalfAngle = 75.f;

	/** Absorption used when guarding without a weapon / shield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat")
	float UnarmedGuardAbsorption = 0.3f;

	/** Natural damage negation in percent per damage type (enemies without armor items). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat")
	TMap<FGameplayTag, float> NaturalNegation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	float StaggerDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	float GuardBreakDuration = 1.3f;

	/** Time spent open to a riposte after a stance break or a parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	float RiposteWindow = 2.5f;

	/** Poise depletion opens a riposte (enemies / bosses) instead of a simple stagger (players). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	bool bPoiseBreakOpensRiposte = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	bool bCanBeRiposted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	bool bCanBeBackstabbed = true;

	/** Hit react / death montages (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	TMap<EOSFHitReaction, TObjectPtr<UAnimMontage>> ReactionMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	TObjectPtr<UAnimMontage> BeingExecutedMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat")
	float FinisherSearchRadius = 220.f;

	/** 0..1: how much of incoming knockback is ignored (heavy bosses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions", meta = (ClampMin = 0, ClampMax = 1))
	float KnockbackResistance = 0.f;

	/** Hit stop, knockback, camera shake, flash and sparks from FOSFDamageSpec::HitFeel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Combat|Reactions")
	bool bPlayHitFeel = true;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Combat")
	FOSFOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Combat")
	FOSFOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Combat")
	FOSFOnHitLanded OnHitLanded;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Combat")
	FOSFOnHitReaction OnHitReaction;

	/** Main entry point for incoming melee / projectile / AoE damage. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	FOSFHitInfo ReceiveDamage(const FOSFDamageEvent& Event);

	/** Raw health damage (status effects, falls, scripts). No reaction, no negation. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	void ApplyDirectDamage(float Amount, AActor* Causer);

	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	void Kill(AActor* Killer);

	/** Revives with full stats (respawn / enemy reset). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	void Revive();

	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	void ApplyReaction(EOSFHitReaction Reaction, float DurationOverride = -1.f);

	/** Called on the attacker when its attack was parried. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	void NotifyParried(AActor* Parrier);

	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	bool CanBeRiposted() const;

	/** True if Attacker stands behind us and could backstab. */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	bool CanBeBackstabbedBy(const AActor* Attacker) const;

	/** Looks for a riposte / backstab target in front of the owner. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Combat")
	AActor* FindFinisherTarget(bool& bOutBackstab) const;

	void BeginBeingExecuted(AActor* Executor);
	void EndBeingExecuted();

	/** Combined negation (0..1) for a damage type: armor pieces, natural negation and modifiers stack multiplicatively. */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	float GetDamageNegation(FGameplayTag DamageType) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	bool IsSameTeam(const AActor* Other) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	float GetLastDamageTime() const { return LastDamageTime; }

	UOSFActionComponent* GetActionComponent() const;
	UOSFStatComponent* GetStatComponent() const;

protected:
	bool bDead = false;
	double StunEndTime = 0.0;
	FGameplayTag ActiveStunTag;
	float LastDamageTime = -100.f;
	TWeakObjectPtr<AActor> CurrentExecutor;

	bool IsFacing(const AActor* Other) const;
	FOSFHitInfo ResolveDamage(const FOSFDamageEvent& Event);
	void PlayHitFeel(const FOSFDamageEvent& Event, const FOSFHitInfo& Info);
	float GetGuardAbsorption(FGameplayTag DamageType, float& OutGuardBoost) const;
	void Die(AActor* Killer);
	void PlayReactionMontage(UAnimMontage* Montage);
	double Now() const;
};
