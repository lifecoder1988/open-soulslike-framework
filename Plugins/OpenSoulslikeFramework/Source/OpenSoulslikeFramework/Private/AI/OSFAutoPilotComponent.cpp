// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFAutoPilotComponent.h"
#include "AI/OSFEnemyCharacter.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFLockOnComponent.h"
#include "Game/OSFPlayerController.h"
#include "Interaction/OSFInteractionComponent.h"
#include "Interaction/OSFInteractable.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Interaction/OSFLadder.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Stats/OSFStatComponent.h"
#include "OSFGameplayTags.h"
#include "OpenSoulslikeFramework.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// --- Tasks ---------------------------------------------------------------------------

FOSFAutoTask FOSFAutoTask::MoveTo(const FVector& InLocation, float InRadius, const FString& InLabel)
{
	FOSFAutoTask Task;
	Task.Type = EOSFAutoTaskType::MoveTo;
	Task.Location = InLocation;
	Task.Radius = InRadius;
	Task.Label = InLabel;
	return Task;
}

FOSFAutoTask FOSFAutoTask::MoveToActor(AActor* InActor, float InRadius, const FString& InLabel)
{
	FOSFAutoTask Task = MoveTo(InActor ? InActor->GetActorLocation() : FVector::ZeroVector, InRadius, InLabel);
	Task.Actor = InActor;
	return Task;
}

FOSFAutoTask FOSFAutoTask::InteractWith(AActor* InActor, float InApproachOffset, const FString& InLabel)
{
	FOSFAutoTask Task;
	Task.Type = EOSFAutoTaskType::Interact;
	Task.Actor = InActor;
	Task.ApproachOffset = InApproachOffset;
	Task.Label = InLabel;
	return Task;
}

FOSFAutoTask FOSFAutoTask::ClearArea(const FVector& InLocation, float InRadius, const FString& InLabel)
{
	FOSFAutoTask Task;
	Task.Type = EOSFAutoTaskType::ClearArea;
	Task.Location = InLocation;
	Task.Radius = InRadius;
	Task.Label = InLabel;
	return Task;
}

FOSFAutoTask FOSFAutoTask::Wait(float InSeconds, const FString& InLabel)
{
	FOSFAutoTask Task;
	Task.Type = EOSFAutoTaskType::Wait;
	Task.Seconds = InSeconds;
	Task.Label = InLabel;
	return Task;
}

FOSFAutoTask FOSFAutoTask::Milestone(const FString& InLabel)
{
	FOSFAutoTask Task;
	Task.Type = EOSFAutoTaskType::Flag;
	Task.Label = InLabel;
	return Task;
}

// --- Component -----------------------------------------------------------------------

UOSFAutoPilotComponent::UOSFAutoPilotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// The title screen and the game menu pause the world: the pilot still has to drive them.
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

AOSFPlayerCharacter* UOSFAutoPilotComponent::GetPlayer() const
{
	return Cast<AOSFPlayerCharacter>(GetOwner());
}

AOSFPlayerController* UOSFAutoPilotComponent::GetPC() const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	return Player ? Player->GetOSFController() : nullptr;
}

void UOSFAutoPilotComponent::SetTasks(const TArray<FOSFAutoTask>& InTasks)
{
	Tasks = InTasks;
}

void UOSFAutoPilotComponent::Start()
{
	TaskIndex = Tasks.Num() > 0 ? 0 : INDEX_NONE;
	bRunning = Tasks.Num() > 0;
	bFinished = false;
	bWasDead = false;
	CurrentTarget = nullptr;
	LastMenu = 0;
	Milestones.Reset();
	ResetTaskState();
	Milestone(FString::Printf(TEXT("start (%d steps)"), Tasks.Num()));
	if (!bRunning)
	{
		UE_LOG(LogOSF, Warning, TEXT("[AutoPilot] no steps to run"));
	}
}

void UOSFAutoPilotComponent::Stop()
{
	if (AOSFPlayerCharacter* Player = GetPlayer(); Player && bRunning)
	{
		Player->SetDesiredMoveDirection(FVector::ZeroVector);
		Player->SetSprinting(false);
	}
	bRunning = false;
}

FString UOSFAutoPilotComponent::GetStatus() const
{
	const FOSFAutoTask* Task = GetCurrentTask();
	return FString::Printf(TEXT("%s step %d/%d%s%s | %s"), bFinished ? TEXT("finished") : bRunning ? TEXT("running") : TEXT("stopped"),
		TaskIndex + 1, Tasks.Num(), Task ? TEXT(" ") : TEXT(""), Task ? *Task->Label : TEXT(""), *LastDecision);
}

void UOSFAutoPilotComponent::Milestone(const FString& Text)
{
	const double Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Milestones.Add({ Time, Text });
	UE_LOG(LogOSF, Log, TEXT("[AutoPilot] %s"), *Text);
}

void UOSFAutoPilotComponent::ResetTaskState()
{
	TaskStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bTaskInteracted = false;
	TaskInteractTime = 0.0;
	ApproachShrink = 0.f;
	EscapeUntil = 0.0;
	LastProgressTime = FPlatformTime::Seconds();
	LastProgressLocation = GetPlayer() ? GetPlayer()->GetActorLocation() : FVector::ZeroVector;
}

void UOSFAutoPilotComponent::NextTask()
{
	const FOSFAutoTask* Task = GetCurrentTask();
	if (Task && !Task->Label.IsEmpty() && Task->Type != EOSFAutoTaskType::Flag)
	{
		Milestone(FString::Printf(TEXT("done: %s"), *Task->Label));
	}
	++TaskIndex;
	ResetTaskState();
	if (AOSFPlayerCharacter* Player = GetPlayer())
	{
		Player->SetDesiredMoveDirection(FVector::ZeroVector);
	}
}

void UOSFAutoPilotComponent::TickPerf(float DeltaTime)
{
	// Frame rate and memory along the run: the budget line for a cooked build (an editor PIE frame carries the editor).
	FrameTimes.Add(DeltaTime);
	const double Now = FPlatformTime::Seconds();
	if (LastPerfReport <= 0.0)
	{
		LastPerfReport = Now;
		return;
	}
	if (Now - LastPerfReport < PerfReportInterval || FrameTimes.Num() < 10)
	{
		return;
	}
	TArray<float> Sorted = FrameTimes;
	Sorted.Sort();
	double Sum = 0.0;
	for (float T : Sorted) { Sum += T; }
	// Average over wall time (ticks stop while the world is paused, so the mean delta would flatter the result).
	const double Elapsed = FMath::Max(Now - LastPerfReport, 0.001);
	const float Worst1 = Sorted[FMath::Clamp(static_cast<int32>(Sorted.Num() * 0.99f), 0, Sorted.Num() - 1)];
	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	Milestone(FString::Printf(TEXT("perf: %.0f fps avg, 1%% low %.0f fps, %.1f ms max, %d frames in %.0f s (ticked %.0f s), mem %.0f MB (peak %.0f MB)"),
		Sorted.Num() / Elapsed, 1.f / FMath::Max(Worst1, 0.0001f), Sorted.Last() * 1000.f, Sorted.Num(), Elapsed, Sum,
		Mem.UsedPhysical / (1024.0 * 1024.0), Mem.PeakUsedPhysical / (1024.0 * 1024.0)));
	FrameTimes.Reset();
	LastPerfReport = Now;
}

void UOSFAutoPilotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRunning)
	{
		return;
	}
	TickPerf(DeltaTime);
	AOSFPlayerCharacter* Player = GetPlayer();
	AOSFPlayerController* PC = GetPC();
	if (!Player || !PC)
	{
		return;
	}
	if (PC->IsInMenu())
	{
		Player->SetDesiredMoveDirection(FVector::ZeroVector);
		TickMenus();
		return;
	}
	LastMenu = 0;
	if (IsWorldBusy())
	{
		return;
	}
	// Death: wait for the respawn, then pick the script up again at the step's retry point.
	if (Player->IsDead())
	{
		bWasDead = true;
		CurrentTarget = nullptr;
		return;
	}
	if (bWasDead)
	{
		bWasDead = false;
		if (Tasks.IsValidIndex(TaskIndex) && Tasks[TaskIndex].OnDeathTask != INDEX_NONE)
		{
			TaskIndex = Tasks[TaskIndex].OnDeathTask;
		}
		ResetTaskState();
		Milestone(TEXT("respawned"));
	}
	if (!Tasks.IsValidIndex(TaskIndex))
	{
		Stop();
		bFinished = true;
		Milestone(TEXT("finished"));
		return;
	}
	TickTask(DeltaTime);
}

// --- Menus ---------------------------------------------------------------------------

void UOSFAutoPilotComponent::TickMenus()
{
	AOSFPlayerController* PC = GetPC();
	const double Now = FPlatformTime::Seconds();
	const EOSFMenu Menu = PC->GetCurrentMenu();
	if (static_cast<uint8>(Menu) != LastMenu)
	{
		LastMenu = static_cast<uint8>(Menu);
		MenuOpenTime = Now;
		LastMenuInputTime = Now;
	}
	if (Now - LastMenuInputTime < MenuInputDelay)
	{
		return;
	}
	auto Select = [PC](int32 Target)
	{
		const int32 Count = PC->GetMenuEntries().Num();
		for (int32 Guard = 0; Guard < Count && PC->GetMenuSelection() != Target; ++Guard)
		{
			PC->MenuNavigate(0, Target > PC->GetMenuSelection() ? 1 : -1);
		}
	};
	switch (Menu)
	{
	case EOSFMenu::Title:
		if (Now - MenuOpenTime < TitleSeconds)
		{
			return;
		}
		Select(0);	// New Game
		PC->MenuConfirm();
		Milestone(TEXT("new game"));
		break;

	case EOSFMenu::Dialogue:
	{
		const TArray<FOSFMenuEntry> Entries = PC->GetMenuEntries();
		if (Entries.Num() <= 1)
		{
			// One entry = "Continue": let the line (and its voice) play before the next one.
			if (Now - LastMenuInputTime < DialogueLineSeconds)
			{
				return;
			}
			PC->MenuConfirm();
		}
		else
		{
			Select(Entries.Num() - 1);	// Leave
			PC->MenuConfirm();
		}
		break;
	}

	case EOSFMenu::Rest:
		if (Now - MenuOpenTime < RestSeconds)
		{
			return;
		}
		Select(PC->GetMenuEntries().Num() - 1);	// Leave
		PC->MenuConfirm();
		break;

	default:
		DriveOtherMenu(PC);
		break;
	}
	LastMenuInputTime = FPlatformTime::Seconds();
}

void UOSFAutoPilotComponent::DriveOtherMenu(AOSFPlayerController* PC)
{
	PC->MenuBack();
}

// --- Script --------------------------------------------------------------------------

void UOSFAutoPilotComponent::TickTask(float DeltaTime)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	FOSFAutoTask& Task = Tasks[TaskIndex];
	const double Now = GetWorld()->GetTimeSeconds();

	if (TaskTimeout > 0.f && (Task.Type == EOSFAutoTaskType::MoveTo || Task.Type == EOSFAutoTaskType::Interact) && Now - TaskStartTime > TaskTimeout && !bTaskInteracted)
	{
		UE_LOG(LogOSF, Warning, TEXT("[AutoPilot] gave up on '%s' after %.0f s"), *Task.Label, TaskTimeout);
		Milestone(FString::Printf(TEXT("skipped: %s"), *Task.Label));
		NextTask();
		return;
	}
	if (Player->GetCurrentLadder())
	{
		// On a ladder only a Custom step (climbing) runs; nothing else can move the character anyway. The Interact
		// step that put us on it is over.
		if (Task.Type == EOSFAutoTaskType::Interact && bTaskInteracted)
		{
			NextTask();
		}
		else if (Task.Type == EOSFAutoTaskType::Custom && (!Task.Custom || Task.Custom(this)))
		{
			NextTask();
		}
		return;
	}
	if (TickCombat(DeltaTime))
	{
		return;
	}
	Player->SetDesiredMoveDirection(FVector::ZeroVector);
	if (Task.Type != EOSFAutoTaskType::Custom && TickPickups(DeltaTime))
	{
		return;
	}
	if ((Task.Type == EOSFAutoTaskType::MoveTo || Task.Type == EOSFAutoTaskType::Interact) && !Task.Actor.IsValid() && Task.ResolveActor)
	{
		Task.Actor = Task.ResolveActor(this);
		if (!Task.Actor.IsValid())
		{
			return;	// not there yet
		}
	}

	switch (Task.Type)
	{
	case EOSFAutoTaskType::MoveTo:
	{
		const FVector Goal = Task.Actor.IsValid() ? Task.Actor->GetActorLocation() : Task.Location;
		if (FVector::Dist2D(Goal, Player->GetActorLocation()) <= Task.Radius)
		{
			Player->SetSprinting(false);
			NextTask();
		}
		else
		{
			Steer(Goal, DeltaTime, Task.bSprint);
		}
		break;
	}

	case EOSFAutoTaskType::Interact:
	{
		AActor* Actor = Task.Actor.Get();
		if (!Actor)
		{
			UE_LOG(LogOSF, Warning, TEXT("[AutoPilot] %s: actor gone"), *Task.Label);
			NextTask();
			break;
		}
		if (bTaskInteracted)
		{
			// Doors swing, chests open: a beat before walking on. Menus the interaction opened were driven already.
			if (Now - TaskInteractTime > 1.2)
			{
				NextTask();
			}
			break;
		}
		if (ApproachAndInteract(Actor, Task.ApproachOffset, DeltaTime, Task.bSprint))
		{
			bTaskInteracted = true;
			TaskInteractTime = Now;
			Milestone(Task.Label.IsEmpty() ? FString::Printf(TEXT("interact %s"), *Actor->GetName()) : Task.Label);
		}
		break;
	}

	case EOSFAutoTaskType::ClearArea:
	{
		TArray<AOSFEnemyCharacter*> Hostiles;
		GatherHostiles(Task.Location, Task.Radius, Hostiles);
		if (Hostiles.Num() == 0)
		{
			Player->SetSprinting(false);
			NextTask();
			break;
		}
		// Nearest first; TickCombat takes over once it is within EngageRadius.
		Steer(Hostiles[0]->GetActorLocation(), DeltaTime, Task.bSprint);
		break;
	}

	case EOSFAutoTaskType::Wait:
		if (Now - TaskStartTime >= Task.Seconds)
		{
			NextTask();
		}
		break;

	case EOSFAutoTaskType::Flag:
		Milestone(Task.Label);
		NextTask();
		break;

	case EOSFAutoTaskType::Custom:
		if (!Task.Custom || Task.Custom(this))
		{
			NextTask();
		}
		break;
	}
}

bool UOSFAutoPilotComponent::ApproachAndInteract(AActor* Actor, float ApproachOffset, float DeltaTime, bool bSprint)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Forward = Actor->GetActorForwardVector().GetSafeNormal2D();
	const float Offset = FMath::Max(60.f, FMath::Abs(ApproachOffset) - ApproachShrink) * FMath::Sign(ApproachOffset);
	const FVector Goal = Actor->GetActorLocation() + Forward * Offset;
	if (FVector::Dist2D(Goal, Player->GetActorLocation()) > 55.f)
	{
		Steer(Goal, DeltaTime, bSprint);
		return false;
	}
	Player->SetSprinting(false);
	if (TaskInteractTime <= 0.0)
	{
		TaskInteractTime = Now;	// arrived
	}
	// Face it (the interaction focus scores facing) and look at it.
	const FVector To = (Actor->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	Player->SetActorRotation(FMath::RInterpTo(Player->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaTime, 10.f));
	Look(To, DeltaTime, 5.f);
	Player->Interaction->UpdateFocus();
	if (Player->Interaction->GetFocusedInteractable() == Actor)
	{
		return !Player->ActionComponent->IsBusy() && Player->Interaction->TryInteract();
	}
	if (Now - TaskInteractTime > 2.0)
	{
		// Not in reach from here: step closer.
		ApproachShrink += 40.f;
		TaskInteractTime = Now;
	}
	return false;
}

bool UOSFAutoPilotComponent::TickPickups(float DeltaTime)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	const double Now = GetWorld()->GetTimeSeconds();
	if (PickupRadius <= 0.f)
	{
		return false;
	}
	AActor* Target = PickupTarget.Get();
	if (Target && (!IOSFInteractable::Execute_CanInteract(Target, Player) || Now - PickupStartTime > 8.0))
	{
		if (Now - PickupStartTime > 8.0)
		{
			SkippedPickups.Add(Target);	// out of reach: leave it
		}
		PickupTarget = nullptr;
		Target = nullptr;
		ResetTaskState();
	}
	if (!Target)
	{
		float Best = PickupRadius;
		for (TActorIterator<AOSFItemPickup> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation());
			if (Distance < Best && !SkippedPickups.Contains(*It) && IOSFInteractable::Execute_CanInteract(*It, Player))
			{
				Best = Distance;
				Target = *It;
			}
		}
		if (!Target)
		{
			return false;
		}
		PickupTarget = Target;
		PickupStartTime = Now;
		ApproachShrink = 0.f;
		TaskInteractTime = 0.0;
	}
	// Drops have no facing: come at them from our side.
	const FVector Toward = (Player->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const float Yaw = Toward.IsNearlyZero() ? 0.f : Toward.Rotation().Yaw;
	Target->SetActorRotation(FRotator(0.f, Yaw, 0.f));
	if (ApproachAndInteract(Target, 70.f, DeltaTime, false))
	{
		PickupTarget = nullptr;
		ResetTaskState();
	}
	return true;
}

// --- Combat --------------------------------------------------------------------------

bool UOSFAutoPilotComponent::ShouldFight(const AOSFEnemyCharacter* Enemy) const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	if (!Enemy || !Player || Enemy->IsDead() || Enemy->IsHidden() || Enemy->Combat->IsSameTeam(Player))
	{
		return false;
	}
	// Bosses wait behind their fog gate.
	return !Enemy->IsBoss() || Enemy->IsBossFightActive();
}

void UOSFAutoPilotComponent::GatherHostiles(const FVector& Center, float Radius, TArray<AOSFEnemyCharacter*>& Out) const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	for (TActorIterator<AOSFEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (ShouldFight(*It) && FVector::Dist2D(It->GetActorLocation(), Center) <= Radius)
		{
			Out.Add(*It);
		}
	}
	const FVector From = Player ? Player->GetActorLocation() : Center;
	Out.Sort([&From](const AOSFEnemyCharacter& A, const AOSFEnemyCharacter& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), From) < FVector::DistSquared2D(B.GetActorLocation(), From);
	});
}

AOSFEnemyCharacter* UOSFAutoPilotComponent::PickTarget(const TArray<AOSFEnemyCharacter*>& Hostiles, AOSFEnemyCharacter* Current) const
{
	if (Hostiles.Num() == 0)
	{
		return nullptr;
	}
	const AOSFPlayerCharacter* Player = GetPlayer();
	AOSFEnemyCharacter* Nearest = Hostiles[0];
	if (Current && Hostiles.Contains(Current))
	{
		// Keep the target unless something else is breathing down our neck.
		const float CurrentDistance = FVector::Dist2D(Current->GetActorLocation(), Player->GetActorLocation());
		const float NearestDistance = FVector::Dist2D(Nearest->GetActorLocation(), Player->GetActorLocation());
		return (NearestDistance < 320.f && CurrentDistance > 600.f) ? Nearest : Current;
	}
	return Nearest;
}

bool UOSFAutoPilotComponent::WantsRanged(const AOSFEnemyCharacter* Target) const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	const UOSFWeaponData* Gun = Player->Equipment->GetActiveRangedWeapon();
	if (!Gun)
	{
		return false;
	}
	if (Player->Equipment->GetMagazine(Gun).Rounds > 0 || Player->Equipment->CanReload(Gun))
	{
		return true;
	}
	for (const UOSFAmmoData* Ammo : Gun->Ranged.Ammo)
	{
		if (Ammo && Player->Inventory->GetItemCount(Ammo) > 0)
		{
			return true;
		}
	}
	return false;
}

void UOSFAutoPilotComponent::ChooseAmmo(AOSFEnemyCharacter* Target)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	UOSFWeaponData* Gun = Player->Equipment->GetActiveRangedWeapon();
	if (Gun && Player->Equipment->GetMagazine(Gun).Rounds == 0 && !Player->Equipment->CanReload(Gun) && !Player->ActionComponent->IsBusy())
	{
		if (Player->Equipment->CycleAmmo(Gun))
		{
			Player->ActionComponent->TryExecuteAction(OSFTags::Action_Reload);
		}
	}
}

bool UOSFAutoPilotComponent::HasLineOfSight(const AActor* Target) const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	if (!Target || !Player)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFAutoPilotLOS), false, Player);
	for (TActorIterator<AOSFCharacterBase> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	const FVector From = Player->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector To = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	return !GetWorld()->LineTraceTestByChannel(From, To, ECC_Visibility, Params);
}

bool UOSFAutoPilotComponent::ReactToThreats(const TArray<AOSFEnemyCharacter*>& Hostiles, bool bCanCounter)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	UOSFActionComponent* Actions = Player->ActionComponent;
	const double Now = GetWorld()->GetTimeSeconds();
	for (AOSFEnemyCharacter* Hostile : Hostiles)
	{
		const float Distance = FVector::Dist2D(Hostile->GetActorLocation(), Player->GetActorLocation());
		if (Distance > 1000.f)
		{
			continue;
		}
		const UOSFAction_Attack* Attack = Cast<UOSFAction_Attack>(Hostile->ActionComponent->GetCurrentAction());
		if (!Attack || !Attack->IsRunning() || Attack->GetPhase() != EOSFActionPhase::Windup)
		{
			continue;
		}
		const FOSFAttackDefinition& Def = Attack->GetCurrentAttack();
		// Perilous wind-up: a shot counters it (the attack is cancelled and its stance breaks).
		if (bCanCounter && Def.bPerilous && Hostile->HasStateTag(OSFTags::State_Perilous) && Def.CounterTags.HasTag(OSFTags::Source_Projectile))
		{
			if (Player->LockOn->GetTarget() != Hostile)
			{
				Player->LockOn->LockOn(Hostile);
				CurrentTarget = Hostile;
			}
			if (!Actions->IsBusy())
			{
				Actions->TryExecuteAction(OSFTags::Action_Shoot, false);
			}
			return true;
		}
		const float Reach = Def.Reach + Def.Lunge + Def.TraceRadius + 160.f;
		if (Distance > Reach)
		{
			continue;
		}
		const float Impact = Attack->GetClipImpactTime() >= 0.f ? Attack->GetClipImpactTime() : Attack->GetEffectiveTiming().Windup;
		const float TimeToHit = Impact - Attack->GetElapsed();
		if (TimeToHit <= DodgeLead && TimeToHit > -0.05f && Now - LastDodgeTime > 0.9)
		{
			const FVector ToEnemy = (Hostile->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, ToEnemy) * StrafeSign;
			Dodge((Side * 0.9f - ToEnemy * 0.45f).GetSafeNormal2D());
			return true;
		}
	}
	return false;
}

void UOSFAutoPilotComponent::Dodge(const FVector& Direction)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	Player->SetDesiredMoveDirection(Direction);
	Player->AddMovementInput(Direction, 1.f);
	if (Player->ActionComponent->TryExecuteAction(OSFTags::Action_Dodge))
	{
		LastDodgeTime = GetWorld()->GetTimeSeconds();
	}
}

bool UOSFAutoPilotComponent::TryHeal(const TArray<AOSFEnemyCharacter*>& Hostiles)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Health = Player->Stats->GetCurrent(OSFTags::Stat_Health);
	const float MaxHealth = Player->Stats->GetMax(OSFTags::Stat_Health);
	if (MaxHealth <= 0.f || Health / MaxHealth >= HealBelowFraction || Now - LastHealTime < 3.0)
	{
		return false;
	}
	const UOSFItemData* Item = Player->Equipment->GetActiveQuickItem();
	if (!Item || !Item->IsUsable() || (!Item->bIsFlask && Player->Inventory->GetItemCount(Item) <= 0))
	{
		return false;
	}
	// Not with a blade already swinging at us - unless we are about to drop anyway.
	const bool bDesperate = Health / MaxHealth < HealBelowFraction * 0.6f;
	for (const AOSFEnemyCharacter* Hostile : Hostiles)
	{
		const UOSFAction_Attack* Attack = Cast<UOSFAction_Attack>(Hostile->ActionComponent->GetCurrentAction());
		if (!Attack || !Attack->IsRunning() || FVector::Dist2D(Hostile->GetActorLocation(), Player->GetActorLocation()) > 450.f)
		{
			continue;
		}
		const float Impact = Attack->GetClipImpactTime() >= 0.f ? Attack->GetClipImpactTime() : Attack->GetEffectiveTiming().Windup;
		if (!bDesperate || Impact - Attack->GetElapsed() < 0.6f)
		{
			return false;
		}
	}
	if (!Player->ActionComponent->IsBusy() && Player->ActionComponent->TryExecuteAction(OSFTags::Action_UseItem))
	{
		LastHealTime = Now;
		return true;
	}
	return false;
}

bool UOSFAutoPilotComponent::TickCombat(float DeltaTime)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	if (Player->GetCurrentLadder())
	{
		return false;
	}
	TArray<AOSFEnemyCharacter*> Hostiles;
	GatherHostiles(Player->GetActorLocation(), EngageRadius, Hostiles);
	// Far hostiles are only a fight when they have noticed us or stand in the area the script is clearing: a wave
	// that respawned behind us at the last rest is left alone.
	const FOSFAutoTask* Task = GetCurrentTask();
	const bool bClearing = Task && Task->Type == EOSFAutoTaskType::ClearArea;
	Hostiles.RemoveAll([Player, Task, bClearing, this](const AOSFEnemyCharacter* Enemy)
	{
		const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
		if (Distance < NearEngageRadius || Enemy->GetFocusTarget() == Player)
		{
			return false;
		}
		return !(bClearing && FVector::Dist2D(Enemy->GetActorLocation(), Task->Location) <= Task->Radius);
	});
	if (Hostiles.Num() == 0)
	{
		if (Player->LockOn->IsLockedOn())
		{
			Player->LockOn->ClearLockOn();
		}
		CurrentTarget = nullptr;
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	AOSFEnemyCharacter* Target = PickTarget(Hostiles, CurrentTarget.Get());
	if (!Target)
	{
		return false;
	}
	if (Target != CurrentTarget.Get() || Player->LockOn->GetTarget() != Target)
	{
		CurrentTarget = Target;
		LastTargetTime = Now;
		Player->LockOn->LockOn(Target);
	}
	Player->SetSprinting(false);
	const float Distance = FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation());
	const bool bRanged = WantsRanged(Target);
	if (ReactToThreats(Hostiles, bRanged))
	{
		return true;
	}
	if (TryHeal(Hostiles))
	{
		return true;
	}
	if (OnCombatTick(Target, Distance, DeltaTime))
	{
		return true;
	}
	if (Now > StrafeFlipTime)
	{
		StrafeSign = -StrafeSign;
		StrafeFlipTime = Now + FMath::FRandRange(2.5, 4.5);
	}
	UOSFActionComponent* Actions = Player->ActionComponent;
	const FVector ToTarget = (Target->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * StrafeSign;
	const bool bSight = HasLineOfSight(Target);
	FVector Wanted = FVector::ZeroVector;
	bool bApproach = false;
	if (bRanged)
	{
		ChooseAmmo(Target);
		if (!bSight || Distance > RangedMaxDistance)
		{
			bApproach = true;
		}
		else if (Distance < RangedMinDistance)
		{
			Wanted = (-ToTarget * 0.8f + Side * 0.6f).GetSafeNormal2D();
		}
		else
		{
			Wanted = Side * 0.55f;
		}
		if (bSight && Distance < RangedMaxDistance + 400.f && !Actions->IsBusy())
		{
			Actions->TryExecuteAction(OSFTags::Action_Shoot, false);
		}
	}
	else
	{
		if (Distance > MeleeDistance)
		{
			bApproach = true;
		}
		else if (!Actions->IsBusy())
		{
			Actions->TryExecuteAction(OSFTags::Action_LightAttack);
		}
	}
	LastDecision = FString::Printf(TEXT("%s %s d=%.0f sight=%d ammo=%s"), bRanged ? TEXT("gun") : TEXT("melee"), bApproach ? TEXT("approach") : Wanted.IsNearlyZero() ? TEXT("hold") : TEXT("strafe"),
		Distance, bSight ? 1 : 0, *GetNameSafe(Player->Equipment->GetActiveWeapon(true)));
	if (bApproach)
	{
		// Closing in is a walk like any other: the watchdog frees us when something unseen holds us.
		Steer(Target->GetActorLocation(), DeltaTime, false);
	}
	else if (!Wanted.IsNearlyZero())
	{
		FVector Clear;
		const float Scale = Wanted.Size();
		if (FindClearDirection(Wanted.GetSafeNormal2D(), Clear))
		{
			Move(Clear * Scale);
		}
	}
	else
	{
		Player->SetDesiredMoveDirection(FVector::ZeroVector);
	}
	return true;
}

// --- Movement ------------------------------------------------------------------------

void UOSFAutoPilotComponent::Move(const FVector& Direction)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	const FVector Clamped = Direction.GetClampedToMaxSize(1.f);
	Player->SetDesiredMoveDirection(Clamped);
	const float Scale = Player->GetActionMovementScale();
	if (Scale > 0.f && !Clamped.IsNearlyZero())
	{
		Player->AddMovementInput(Clamped.GetSafeNormal2D(), Scale * Clamped.Size());
	}
}

void UOSFAutoPilotComponent::Look(const FVector& Direction, float DeltaTime, float Speed)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	AOSFPlayerController* PC = GetPC();
	if (!PC || Player->LockOn->IsLockedOn() || Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator Desired(-11.f, Direction.Rotation().Yaw, 0.f);
	PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, Speed));
}

bool UOSFAutoPilotComponent::FindClearDirection(const FVector& Desired, FVector& OutDirection) const
{
	const AOSFPlayerCharacter* Player = GetPlayer();
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() * 0.9f;
	// From just above what the character can step over up to its head: low rocks and fire pits count as obstacles too.
	const float StepUp = 50.f;
	const float HalfHeight = FMath::Max(Radius, (Capsule->GetScaledCapsuleHalfHeight() * 2.f - StepUp) * 0.5f - 4.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFAutoPilotSteer), false, Player);
	for (TActorIterator<AOSFCharacterBase> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	const FVector Start = Player->GetActorLocation() + FVector(0.f, 0.f, StepUp * 0.5f);
	const float Ahead = Player->IsSprinting() ? 420.f : 260.f;
	static const float Angles[] = { 0.f, 28.f, -28.f, 56.f, -56.f, 84.f, -84.f, 112.f, -112.f, 140.f, -140.f };
	FHitResult Hit;
	for (const float Angle : Angles)
	{
		const FVector Direction = Desired.RotateAngleAxis(Angle * PreferredSide, FVector::UpVector).GetSafeNormal2D();
		const FVector End = Start + Direction * Ahead;
		if (!World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
		{
			OutDirection = Direction;
			return true;
		}
		if (Angle == 0.f)
		{
			LastBlocker = FString::Printf(TEXT("%s%s"), *GetNameSafe(Hit.GetActor()), Hit.bStartPenetrating ? TEXT(" (penetrating)") : TEXT(""));
		}
	}
	return false;
}

void UOSFAutoPilotComponent::Steer(const FVector& Goal, float DeltaTime, bool bSprint)
{
	AOSFPlayerCharacter* Player = GetPlayer();
	const double Now = FPlatformTime::Seconds();
	const FVector Location = Player->GetActorLocation();
	FVector To = Goal - Location;
	To.Z = 0.f;
	const float Distance = To.Size();
	const FVector Desired = To.GetSafeNormal2D();
	FVector Direction = Desired;
	if (Now < EscapeUntil)
	{
		Direction = EscapeDirection;
	}
	else
	{
		// A goal closer than the look-ahead is walked to straight (the thing we go to is often the obstacle).
		FVector Clear;
		if (Distance > 120.f && FindClearDirection(Desired, Clear))
		{
			Direction = Clear;
		}
	}
	Move(Direction);
	Look(Direction, DeltaTime, 3.f);
	const float Stamina = Player->Stats->GetCurrent(OSFTags::Stat_Stamina);
	Player->SetSprinting(bSprint && Distance > 700.f && Stamina > 25.f && !Player->ActionComponent->IsBusy());
	// Stuck against something the sweeps didn't see: break out sideways for a moment.
	if (Now - LastProgressTime > 1.2)
	{
		if (Stalls > 0 && FVector::Dist2D(Location, StallOrigin) > 350.f)
		{
			Stalls = 0;	// really out of it, not just a hop or a roll inside the trap
		}
		if (FVector::Dist2D(Location, LastProgressLocation) < 90.f)
		{
			if (Stalls == 0)
			{
				StallOrigin = Location;
			}
			// Sideways, or wherever a sweep finds room (back out the way we came when boxed in), then try the other side next time.
			const FVector Side = Desired.RotateAngleAxis(PreferredSide * FMath::FRandRange(70.f, 130.f), FVector::UpVector).GetSafeNormal2D();
			FVector Clear;
			const bool bRoom = FindClearDirection(Side, Clear);
			EscapeDirection = bRoom ? Clear : -Desired;
			EscapeUntil = Now + 1.0;
			PreferredSide = -PreferredSide;
			++Stalls;
			if (Stalls == 2)
			{
				Dodge(EscapeDirection);	// a roll's movement burst pops out of shallow overlaps
			}
			else if (Stalls >= 3)
			{
				// Wedged in geometry the capsule can't leave on its own (a prop with collision it fell into): step out of it.
				const FVector Nudge = Location + EscapeDirection * 90.f;
				Player->SetActorLocation(Nudge, false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogOSF, Warning, TEXT("[AutoPilot] stuck at %s (%s, ahead: %s): nudged out"), *Location.ToCompactString(), bRoom ? TEXT("sweeps clear") : TEXT("no clear sweep"), *LastBlocker);
				Stalls = 0;
			}
			else
			{
				Player->Jump();
			}
		}
		LastProgressLocation = Location;
		LastProgressTime = Now;
	}
}
