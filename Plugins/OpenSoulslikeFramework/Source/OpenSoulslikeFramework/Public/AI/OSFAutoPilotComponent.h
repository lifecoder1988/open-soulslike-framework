// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "OSFAutoPilotComponent.generated.h"

class AOSFPlayerCharacter;
class AOSFPlayerController;
class AOSFEnemyCharacter;
class AOSFCharacterBase;
class UOSFAutoPilotComponent;

/** What one step of an autopilot script does. */
UENUM(BlueprintType)
enum class EOSFAutoTaskType : uint8
{
	MoveTo,		// walk to Location (or Actor) until within Radius
	Interact,	// walk up to Actor (ApproachOffset along its forward vector) and interact with it; menus it opens are driven too
	ClearArea,	// fight every hostile within Radius of Location until none is alive
	Wait,		// stand for Seconds (world time)
	Flag,		// log a milestone ("[AutoPilot] Label") and move on
	Custom		// Custom(this) decides when the step is done
};

/** One step of an autopilot script. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFAutoTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	EOSFAutoTaskType Type = EOSFAutoTaskType::MoveTo;

	/** Shown in the status and in the milestone log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	FVector Location = FVector::ZeroVector;

	/** Target of MoveTo / Interact; its location replaces Location when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	TWeakObjectPtr<AActor> Actor;

	/** MoveTo: accept radius; ClearArea: radius of the area (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float Radius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float Seconds = 0.f;

	/** Sprint while travelling (no hostiles around). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	bool bSprint = true;

	/** Interact: stand this far along the actor's forward vector (negative = behind it, e.g. a fog gate entered from outside). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float ApproachOffset = 160.f;

	/** Task to go back to after the player dies and respawns during this one (-1 = keep going with this one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	int32 OnDeathTask = INDEX_NONE;

	/** Custom steps: return true when done. */
	TFunction<bool(UOSFAutoPilotComponent*)> Custom;

	/** MoveTo / Interact: finds the actor when Actor is unset (a chest that appears later); null = keep waiting. */
	TFunction<AActor*(UOSFAutoPilotComponent*)> ResolveActor;

	static FOSFAutoTask MoveTo(const FVector& InLocation, float InRadius, const FString& InLabel = FString());
	static FOSFAutoTask MoveToActor(AActor* InActor, float InRadius, const FString& InLabel = FString());
	static FOSFAutoTask InteractWith(AActor* InActor, float InApproachOffset = 160.f, const FString& InLabel = FString());
	static FOSFAutoTask ClearArea(const FVector& InLocation, float InRadius, const FString& InLabel = FString());
	static FOSFAutoTask Wait(float InSeconds, const FString& InLabel = FString());
	static FOSFAutoTask Milestone(const FString& InLabel);
};

/**
 * Plays the game like a player would, through the same components player input uses (movement input, the action
 * component, lock-on, interaction, menus): a scripted list of steps (walk here, fight everything around there, use
 * that resting point, talk to this NPC, open that door) plus a combat loop that locks on, keeps its distance, shoots
 * or swings, rolls out of wind-ups, counters perilous attacks with a shot and heals when low.
 *
 * Meant for recorded playthroughs and end-to-end playtests of a level without a human at the controls. Games
 * subclass it for their own rules (ammo choice, special meters, cutscenes) and build the script from their level data.
 * Console: `OSFAutoPilot start|stop|status` (AOSFPlayerController).
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFAutoPilotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFAutoPilotComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Tuning ---

	/** Hostiles closer than this that have noticed the player (or stand in the area being cleared) are fought before the script goes on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float EngageRadius = 1600.f;

	/** Hostiles closer than this are fought whether or not they have noticed the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float NearEngageRadius = 800.f;

	/** Distance band kept from the target with a ranged weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float RangedMinDistance = 330.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float RangedMaxDistance = 900.f;

	/** Distance closed to before swinging a melee weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float MeleeDistance = 170.f;

	/** Heals (quick item) below this health fraction when nothing is winding up nearby. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float HealBelowFraction = 0.45f;

	/** Rolls this long before an attack would land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float DodgeLead = 0.38f;

	/** Seconds between menu inputs (readable on a recording). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float MenuInputDelay = 0.7f;

	/** Seconds each dialogue line stays up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float DialogueLineSeconds = 3.4f;

	/** Seconds the title screen is left up before "New Game". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float TitleSeconds = 4.f;

	/** Seconds a resting point menu stays open before leaving it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float RestSeconds = 2.5f;

	/** Dropped items this close are picked up on the way (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float PickupRadius = 900.f;

	/** A MoveTo / Interact step still unfinished after this many seconds is skipped with a warning (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|AutoPilot")
	float TaskTimeout = 75.f;

	// --- Script ---

	UFUNCTION(BlueprintCallable, Category = "OSF|AutoPilot")
	void SetTasks(const TArray<FOSFAutoTask>& InTasks);

	void AddTask(const FOSFAutoTask& Task) { Tasks.Add(Task); }
	int32 NumTasks() const { return Tasks.Num(); }

	UFUNCTION(BlueprintCallable, Category = "OSF|AutoPilot")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "OSF|AutoPilot")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "OSF|AutoPilot")
	bool IsRunning() const { return bRunning; }

	/** Every step done (and the pilot stopped itself). */
	UFUNCTION(BlueprintPure, Category = "OSF|AutoPilot")
	bool IsFinished() const { return bFinished; }

	UFUNCTION(BlueprintPure, Category = "OSF|AutoPilot")
	int32 GetTaskIndex() const { return TaskIndex; }

	UFUNCTION(BlueprintPure, Category = "OSF|AutoPilot")
	FString GetStatus() const;

	const FOSFAutoTask* GetCurrentTask() const { return Tasks.IsValidIndex(TaskIndex) ? &Tasks[TaskIndex] : nullptr; }

	/** Logs "[AutoPilot] Text" (LogOSF) and remembers it with the world time: recordings cut on these. */
	void Milestone(const FString& Text);

	const TArray<TPair<double, FString>>& GetMilestones() const { return Milestones; }

	AOSFPlayerCharacter* GetPlayer() const;
	AOSFPlayerController* GetPC() const;

protected:
	// --- Policy hooks (per game) ---

	/** Cutscenes, loading, scripted sequences: the pilot waits. */
	virtual bool IsWorldBusy() const { return false; }

	/** A living enemy of another team the pilot should fight. */
	virtual bool ShouldFight(const AOSFEnemyCharacter* Enemy) const;

	/** The hostile to engage; the current target is kept unless another is clearly better. */
	virtual AOSFEnemyCharacter* PickTarget(const TArray<AOSFEnemyCharacter*>& Hostiles, AOSFEnemyCharacter* Current) const;

	/** Prefer the gun against this target (a ranged weapon with rounds is active). Melee otherwise. */
	virtual bool WantsRanged(const AOSFEnemyCharacter* Target) const;

	/** Called before shooting: switch ammo types, reload, etc. Default: cycle to an ammo type with rounds when empty. */
	virtual void ChooseAmmo(AOSFEnemyCharacter* Target);

	/** Extra per-tick combat behaviour (special meters, buffs). Return true to skip the default shoot / swing this tick. */
	virtual bool OnCombatTick(AOSFEnemyCharacter* Target, float Distance, float DeltaTime) { return false; }

	/** Menu the pilot doesn't know: default backs out of it. */
	virtual void DriveOtherMenu(AOSFPlayerController* PC);

	// --- Engine ---

	void TickMenus();
	void TickTask(float DeltaTime);
	/** Walks to and takes a nearby drop. True while busy with one. */
	bool TickPickups(float DeltaTime);
	/** Walks up to Actor (ApproachOffset along its forward) and interacts; true once the interaction happened. */
	bool ApproachAndInteract(AActor* Actor, float ApproachOffset, float DeltaTime, bool bSprint);
	/** Fights hostiles around the player. True while fighting (the script waits). */
	bool TickCombat(float DeltaTime);
	bool ReactToThreats(const TArray<AOSFEnemyCharacter*>& Hostiles, bool bCanCounter);
	bool TryHeal(const TArray<AOSFEnemyCharacter*>& Hostiles);
	void Steer(const FVector& Goal, float DeltaTime, bool bSprint);
	bool FindClearDirection(const FVector& Desired, FVector& OutDirection) const;
	void Move(const FVector& Direction);
	void Look(const FVector& Direction, float DeltaTime, float Speed = 4.f);
	void Dodge(const FVector& Direction);
	void GatherHostiles(const FVector& Center, float Radius, TArray<AOSFEnemyCharacter*>& Out) const;
	bool HasLineOfSight(const AActor* Target) const;
	void NextTask();
	void ResetTaskState();

	UPROPERTY(Transient)
	TArray<FOSFAutoTask> Tasks;

	int32 TaskIndex = INDEX_NONE;
	bool bRunning = false;
	bool bFinished = false;
	double TaskStartTime = 0.0;
	// Performance: frame times since the last report, written as a milestone every PerfReportInterval seconds.
	TArray<float> FrameTimes;
	double LastPerfReport = 0.0;
	float PerfReportInterval = 15.f;
	void TickPerf(float DeltaTime);
	bool bTaskInteracted = false;
	double TaskInteractTime = 0.0;
	float ApproachShrink = 0.f;
	// Steering
	FVector LastProgressLocation = FVector::ZeroVector;
	double LastProgressTime = 0.0;
	FVector EscapeDirection = FVector::ZeroVector;
	double EscapeUntil = 0.0;
	float PreferredSide = 1.f;
	int32 Stalls = 0;
	FVector StallOrigin = FVector::ZeroVector;
	FString LastDecision;
	mutable FString LastBlocker;
	// Combat
	TWeakObjectPtr<AOSFEnemyCharacter> CurrentTarget;
	TWeakObjectPtr<AActor> PickupTarget;
	double PickupStartTime = 0.0;
	TArray<TWeakObjectPtr<AActor>> SkippedPickups;
	double LastDodgeTime = -10.0;
	double LastHealTime = -10.0;
	double LastTargetTime = -10.0;
	float StrafeSign = 1.f;
	double StrafeFlipTime = 0.0;
	// Menus (real time: the world may be paused)
	double LastMenuInputTime = 0.0;
	double MenuOpenTime = 0.0;
	uint8 LastMenu = 0;
	bool bWasDead = false;
	TArray<TPair<double, FString>> Milestones;
};
