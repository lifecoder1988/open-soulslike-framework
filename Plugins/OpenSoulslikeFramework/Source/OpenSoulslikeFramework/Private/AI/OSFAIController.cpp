// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFAIController.h"
#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFAICombatComponent.h"
#include "AI/OSFBossComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Combat/OSFCombatComponent.h"
#include "Stats/OSFStatComponent.h"
#include "OSFGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AOSFAIController::AOSFAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1600.f;
	SightConfig->LoseSightRadius = 2200.f;
	SightConfig->PeripheralVisionAngleDegrees = 65.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

AOSFEnemyCharacter* AOSFAIController::GetEnemy() const
{
	return Cast<AOSFEnemyCharacter>(GetPawn());
}

void AOSFAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		SightConfig->SightRadius = Enemy->SightRadius;
		SightConfig->LoseSightRadius = Enemy->SightRadius * 1.4f;
		Perception->ConfigureSense(*SightConfig);
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AOSFAIController::HandleTargetPerceptionUpdated);
	}
	if (Enemy->BehaviorTree)
	{
		RunBehaviorTree(Enemy->BehaviorTree);
	}
	State = Enemy->PatrolPoints.Num() > 0 ? EOSFAIState::Patrol : EOSFAIState::Idle;
	SetCombatMovement(false);
}

void AOSFAIController::OnUnPossess()
{
	SetTarget(nullptr);
	Super::OnUnPossess();
}

bool AOSFAIController::IsValidTarget(const AActor* Candidate) const
{
	const AOSFEnemyCharacter* Enemy = GetEnemy();
	const UOSFCombatComponent* CandidateCombat = OSF::Get<UOSFCombatComponent>(Candidate);
	return Enemy && Candidate && Candidate != Enemy && CandidateCombat && !CandidateCombat->IsDead() && !Enemy->Combat->IsSameTeam(Candidate);
}

void AOSFAIController::SetTarget(AActor* NewTarget)
{
	if (Target.Get() == NewTarget)
	{
		return;
	}
	if (UOSFActionComponent* OldActions = TargetActions.Get())
	{
		OldActions->OnActionStarted.RemoveDynamic(this, &AOSFAIController::HandleTargetActionStarted);
	}
	Target = NewTarget;
	TargetActions = OSF::Get<UOSFActionComponent>(NewTarget);
	if (UOSFActionComponent* NewActions = TargetActions.Get())
	{
		NewActions->OnActionStarted.AddUniqueDynamic(this, &AOSFAIController::HandleTargetActionStarted);
	}
	if (NewTarget)
	{
		State = EOSFAIState::Combat;
		SetCombatMovement(true);
		ScheduleNextAttack();
		// Short reaction delay before the first swing.
		NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.4f, 0.9f);
	}
}

void AOSFAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (State == EOSFAIState::Dead || Target.IsValid())
	{
		return;
	}
	const AOSFEnemyCharacter* Enemy = GetEnemy();
	if (Stimulus.WasSuccessfullySensed() && IsValidTarget(Actor) && !(Enemy && Enemy->IsBoss()))
	{
		SetTarget(Actor);
	}
}

void AOSFAIController::HandleTargetActionStarted(UOSFActionComponent* Component, UOSFAction* Action)
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || !Action || bPendingReaction || Enemy->ActionComponent->IsBusy() || !Target.IsValid())
	{
		return;
	}
	if (!Action->GrantedTags.HasTag(OSFTags::State_Attacking))
	{
		return;
	}
	const float Distance = FVector::Dist2D(Target->GetActorLocation(), Enemy->GetActorLocation());
	if (Distance > 420.f)
	{
		return;
	}
	const float Roll = FMath::FRand();
	if (Roll < Enemy->AICombat->DodgeChance)
	{
		bPendingReaction = true;
		bReactionDodge = true;
	}
	else if (Roll < Enemy->AICombat->DodgeChance + Enemy->AICombat->GuardChance)
	{
		bPendingReaction = true;
		bReactionDodge = false;
	}
	ReactionTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.05f, 0.2f);
}

void AOSFAIController::HandlePawnDeath()
{
	State = EOSFAIState::Dead;
	SetTarget(nullptr);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AOSFAIController::ResetAI()
{
	SetTarget(nullptr);
	StopMovement();
	bPendingReaction = false;
	PendingFollowUp = INDEX_NONE;
	PatrolIndex = 0;
	const AOSFEnemyCharacter* Enemy = GetEnemy();
	State = (Enemy && Enemy->PatrolPoints.Num() > 0) ? EOSFAIState::Patrol : EOSFAIState::Idle;
	SetCombatMovement(false);
}

void AOSFAIController::SetCombatMovement(bool bCombat)
{
	if (AOSFEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->GetCharacterMovement()->bOrientRotationToMovement = !bCombat;
	}
}

void AOSFAIController::ScheduleNextAttack()
{
	const AOSFEnemyCharacter* Enemy = GetEnemy();
	const FVector2D Interval = Enemy ? Enemy->AICombat->AttackInterval : FVector2D(1.f, 2.f);
	NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Interval.X, Interval.Y);
}

void AOSFAIController::SteerTowards(const FVector& Direction, float Scale)
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	const FVector Dir = Direction.GetSafeNormal2D();
	Enemy->SetDesiredMoveDirection(Dir);
	Enemy->AddMovementInput(Dir, Scale * Enemy->GetActionMovementScale());
}

void AOSFAIController::FaceTarget(float DeltaSeconds)
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	const AActor* Current = Target.Get();
	if (!Enemy || !Current || Enemy->GetActionMovementScale() <= 0.f)
	{
		return;
	}
	const FVector To = (Current->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return;
	}
	const FRotator NewRotation = FMath::RInterpConstantTo(Enemy->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, Enemy->TurnRate);
	Enemy->SetActorRotation(NewRotation);
	SetControlRotation(NewRotation);
}

void AOSFAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || Enemy->BehaviorTree)
	{
		return;
	}
	if (Enemy->IsDead())
	{
		State = EOSFAIState::Dead;
		return;
	}
	if (State == EOSFAIState::Dead)
	{
		State = EOSFAIState::Idle;
	}

	switch (State)
	{
	case EOSFAIState::Idle:
	case EOSFAIState::Patrol:
		TickIdle(DeltaSeconds);
		break;
	case EOSFAIState::Combat:
		TickCombat(DeltaSeconds);
		break;
	case EOSFAIState::Return:
		TickReturn(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AOSFAIController::LookForTarget()
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || Enemy->IsBoss())
	{
		return; // bosses wait for their fog gate
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Candidate = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!IsValidTarget(Candidate))
		{
			continue;
		}
		const FVector To = Candidate->GetActorLocation() - Enemy->GetActorLocation();
		const float Distance = To.Size();
		if (Distance <= Enemy->ProximityAggroRadius)
		{
			SetTarget(Candidate);
			return;
		}
		const bool bInView = FVector::DotProduct(Enemy->GetActorForwardVector(), To.GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(70.f));
		if (Distance <= Enemy->SightRadius && bInView && LineOfSightTo(Candidate))
		{
			SetTarget(Candidate);
			return;
		}
	}
}

void AOSFAIController::TickIdle(float DeltaSeconds)
{
	LookForTarget();
	if (State == EOSFAIState::Combat)
	{
		return;
	}
	AOSFEnemyCharacter* Enemy = GetEnemy();
	if (State == EOSFAIState::Patrol && Enemy->PatrolPoints.Num() > 0)
	{
		const FVector Goal = Enemy->PatrolPoints[PatrolIndex % Enemy->PatrolPoints.Num()];
		const FVector To = Goal - Enemy->GetActorLocation();
		if (To.Size2D() < 80.f)
		{
			PatrolIndex = (PatrolIndex + 1) % Enemy->PatrolPoints.Num();
		}
		else
		{
			SteerTowards(To, 0.45f);
		}
	}
}

void AOSFAIController::TickReturn(float DeltaSeconds)
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	const FVector Home = Enemy->GetHomeTransform().GetLocation();
	const FVector To = Home - Enemy->GetActorLocation();
	if (To.Size2D() < 90.f)
	{
		Enemy->SetActorRotation(Enemy->GetHomeTransform().GetRotation());
		Enemy->Stats->RestoreAll();
		State = Enemy->PatrolPoints.Num() > 0 ? EOSFAIState::Patrol : EOSFAIState::Idle;
		return;
	}
	SteerTowards(To, 0.7f);
	LookForTarget();
}

void AOSFAIController::TickCombat(float DeltaSeconds)
{
	AOSFEnemyCharacter* Enemy = GetEnemy();
	AActor* Current = Target.Get();
	const double Now = GetWorld()->GetTimeSeconds();

	if (!IsValidTarget(Current))
	{
		SetTarget(nullptr);
		State = Enemy->IsBoss() ? EOSFAIState::Idle : EOSFAIState::Return;
		SetCombatMovement(false);
		if (Enemy->IsBoss() && Enemy->Boss)
		{
			Enemy->Boss->EndFight();
		}
		return;
	}
	const float HomeDistance = FVector::Dist2D(Enemy->GetActorLocation(), Enemy->GetHomeTransform().GetLocation());
	if (!Enemy->IsBossFightActive() && HomeDistance > Enemy->LeashRadius)
	{
		SetTarget(nullptr);
		State = EOSFAIState::Return;
		SetCombatMovement(false);
		return;
	}

	UOSFActionComponent* Actions = Enemy->ActionComponent;
	if (Actions->IsBusy())
	{
		bWasBusy = true;
		if (Actions->GetCurrentAction() && Actions->GetCurrentAction()->ActionTag == OSFTags::Action_Guard && Now >= GuardReleaseTime)
		{
			Actions->StopAction(OSFTags::Action_Guard);
		}
		return;
	}
	if (Enemy->GetActionMovementScale() <= 0.f)
	{
		return; // staggered / broken
	}

	const float Distance = FVector::Dist2D(Current->GetActorLocation(), Enemy->GetActorLocation());
	const FVector ToTarget = (Current->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	const int32 Phase = Enemy->Boss ? Enemy->Boss->GetCurrentPhase() : 0;
	UOSFAICombatComponent* AICombat = Enemy->AICombat;

	if (bWasBusy)
	{
		bWasBusy = false;
		PendingFollowUp = AICombat->RollFollowUp();
		if (PendingFollowUp == INDEX_NONE)
		{
			ScheduleNextAttack();
		}
	}

	FaceTarget(DeltaSeconds);

	if (PendingFollowUp != INDEX_NONE)
	{
		const int32 FollowUp = PendingFollowUp;
		PendingFollowUp = INDEX_NONE;
		if (Distance <= AICombat->Abilities[FollowUp].MaxRange && AICombat->ExecuteAbility(FollowUp))
		{
			return;
		}
		ScheduleNextAttack();
	}

	if (bPendingReaction && Now >= ReactionTime)
	{
		bPendingReaction = false;
		if (bReactionDodge)
		{
			const FVector Side = FVector::CrossProduct(ToTarget, FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
			Enemy->SetDesiredMoveDirection((Side - ToTarget * 0.5f).GetSafeNormal());
			Actions->TryExecuteAction(OSFTags::Action_Dodge, false);
		}
		else if (Actions->TryExecuteAction(OSFTags::Action_Guard, false))
		{
			GuardReleaseTime = Now + FMath::FRandRange(0.7f, 1.3f);
		}
		return;
	}

	// Punish: the target is stuck in a long recovery / out of stamina right next to us.
	const UOSFActionComponent* TargetActionComp = TargetActions.Get();
	const bool bTargetVulnerable = TargetActionComp && (TargetActionComp->HasStateTag(OSFTags::State_Staggered) || TargetActionComp->HasStateTag(OSFTags::State_UsingItem));
	if (bTargetVulnerable && Now + 0.5 < NextAttackTime && FMath::FRand() < AICombat->PunishChance * DeltaSeconds * 4.f)
	{
		NextAttackTime = Now;
	}

	if (Now >= NextAttackTime)
	{
		const int32 Ability = AICombat->SelectAbility(Distance, Phase);
		if (Ability != INDEX_NONE && AICombat->ExecuteAbility(Ability))
		{
			return;
		}
		// Nothing in range: close the distance (run if far).
		if (Distance > 160.f)
		{
			Enemy->SetSprinting(Distance > 900.f);
			SteerTowards(ToTarget, 1.f);
		}
		return;
	}

	// Spacing & strafing between attacks.
	Enemy->SetSprinting(false);
	if (Now >= StrafeSwitchTime)
	{
		StrafeDirection = FMath::RandBool() ? 1.f : -1.f;
		StrafeSwitchTime = Now + FMath::FRandRange(1.2f, 2.8f);
	}
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * StrafeDirection;
	if (Distance > AICombat->PreferredDistance + 150.f)
	{
		SteerTowards(ToTarget + Side * 0.2f, Distance > 900.f ? 1.f : 0.75f);
		Enemy->SetSprinting(Distance > 900.f);
	}
	else if (Distance < AICombat->PreferredDistance - 120.f)
	{
		SteerTowards(-ToTarget + Side * 0.5f, 0.4f);
	}
	else
	{
		SteerTowards(Side, 0.35f);
	}
}
