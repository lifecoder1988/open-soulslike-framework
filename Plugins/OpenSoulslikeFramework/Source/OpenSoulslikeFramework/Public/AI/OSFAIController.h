// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "OSFAIController.generated.h"

class AOSFEnemyCharacter;
class UAISenseConfig_Sight;
class UOSFActionComponent;
class UOSFAction;

UENUM(BlueprintType)
enum class EOSFAIState : uint8
{
	Idle,
	Patrol,
	Combat,
	Return,
	Dead
};

/**
 * AI behaviour manager. A compact state machine (idle / patrol / combat / return) that needs no
 * behavior tree or navmesh to work: approach, strafe and space, pick abilities, chain combos,
 * react to the player's attacks (guard / dodge) and punish whiffs. Uses navigation when available.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API AOSFAIController : public AAIController
{
	GENERATED_BODY()

public:
	AOSFAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "OSF|AI")
	AActor* GetTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintCallable, Category = "OSF|AI")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "OSF|AI")
	EOSFAIState GetState() const { return State; }

	void HandlePawnDeath();
	void ResetAI();

protected:
	UPROPERTY(VisibleAnywhere, Category = "OSF|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleTargetActionStarted(UOSFActionComponent* Component, UOSFAction* Action);

	void TickIdle(float DeltaSeconds);
	void TickCombat(float DeltaSeconds);
	void TickReturn(float DeltaSeconds);
	void LookForTarget();
	bool IsValidTarget(const AActor* Candidate) const;
	void SteerTowards(const FVector& Direction, float Scale);
	void FaceTarget(float DeltaSeconds);
	void SetCombatMovement(bool bCombat);
	void ScheduleNextAttack();
	/** Other regular enemies currently attacking Victim (UOSFAICombatComponent::MaxSimultaneousAttackers). */
	int32 CountOtherAttackers(const AActor* Victim) const;

	AOSFEnemyCharacter* GetEnemy() const;

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UOSFActionComponent> TargetActions;
	EOSFAIState State = EOSFAIState::Idle;
	int32 PatrolIndex = 0;
	double NextAttackTime = 0.0;
	double StrafeSwitchTime = 0.0;
	double GuardReleaseTime = 0.0;
	double ReactionTime = 0.0;
	int32 PendingFollowUp = INDEX_NONE;
	float StrafeDirection = 1.f;
	bool bWasBusy = false;
	bool bPendingReaction = false;
	bool bReactionDodge = false;
};
