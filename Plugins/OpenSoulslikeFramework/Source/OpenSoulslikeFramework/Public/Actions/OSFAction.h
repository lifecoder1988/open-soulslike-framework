// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OSFTypes.h"
#include "OSFAction.generated.h"

class ACharacter;
class UAnimMontage;
class UOSFActionComponent;
class UOSFStatComponent;

/** Windows inside an action, opened either by procedural timing or by anim notify states. */
UENUM(BlueprintType)
enum class EOSFActionWindow : uint8
{
	Active,		// damage window (weapon trace)
	Invincible,	// i-frames
	HyperArmor,	// no stagger
	Cancel,		// combo / input-buffer window: the action can be chained or cancelled
	Parry		// parry window
};

UENUM(BlueprintType)
enum class EOSFActionPhase : uint8
{
	None,
	Windup,
	Active,
	Recovery
};

/**
 * Base class of everything a character "does": attacks, dodges, guard, item use, executions...
 * Actions are UObjects instanced per character by the Action Manager (UOSFActionComponent).
 *
 * An action runs in one of two modes:
 *  - Montage mode: a montage is played and its OSF anim notifies open/close windows; the action ends with the montage.
 *  - Procedural mode: windows follow FOSFActionTiming. This makes the whole framework playable and testable
 *    without any animation asset, and is what the AI and automation tests use by default.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction : public UObject
{
	GENERATED_BODY()

public:
	UOSFAction();

	virtual UWorld* GetWorld() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FGameplayTag ActionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	float StaminaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	float FPCost = 0.f;

	/** Owner must have all of these tags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer RequiredTags;

	/** Owner must have none of these tags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer BlockedTags;

	/** Tags owned for the whole duration of the action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer GrantedTags;

	/** Actions (by tag) allowed to interrupt this action at any time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer CancelledByActions;

	/** If true, any action can interrupt this one while the Cancel window is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	bool bCancellableInCancelWindow = true;

	/** If the input arrives while another action runs, keep it in the input buffer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	bool bCanBeBuffered = true;

	/** Hold actions (guard) run until StopAction is called. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	bool bIsHoldAction = false;

	/** Movement input scale while the action runs (0 = rooted). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	float MovementScale = 0.f;

	/** Rotation rate (deg/s) towards input / lock target allowed during the windup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	float WindupRotationRate = 540.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action")
	FOSFActionTiming Timing;

	/**
	 * Clip played when the character uses UOSFClipAnimInstance and no montage plays. The clip plays at its own
	 * pace: the action lasts the clip, the Active window opens on the clip's hit frame (FOSFAnimClip::ImpactRule)
	 * and lasts Timing.Active (0.08 - 0.3 s), the cancel / combo window opens at 45% of the clip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FOSFAnimClip Clip;

	/** Clip mode: retime the clip to the action's Timing instead of stretching the Timing to the clip (short actions such as a parry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bFitClipToTiming = false;

	// --- Lifecycle (called by the action component) ---

	void Initialize(UOSFActionComponent* InComponent);

	/** Gives the action a chance to hand over to another action (e.g. light attack -> riposte). */
	virtual UOSFAction* GetRedirect() { return nullptr; }

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Action")
	bool CanActivate() const;

	void Start();
	void TickAction(float DeltaTime);
	void Finish(bool bInterrupted);

	/** Hold actions: input released. */
	virtual void Release();

	/** Anim notify / procedural window entry point. */
	void SetWindow(EOSFActionWindow Window, bool bOpen);

	bool CanBeCancelledBy(const UOSFAction* Other) const;

	UFUNCTION(BlueprintPure, Category = "Action")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category = "Action")
	bool IsWindowOpen(EOSFActionWindow Window) const { return OpenWindows.Contains(Window); }

	UFUNCTION(BlueprintPure, Category = "Action")
	float GetElapsed() const { return Elapsed; }

	UFUNCTION(BlueprintPure, Category = "Action")
	EOSFActionPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Action")
	ACharacter* GetCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Action")
	UOSFActionComponent* GetActionComponent() const { return ActionComponent; }

	UOSFStatComponent* GetStats() const;

	/** Whether the procedural timeline drives the windows (no montage playing). */
	bool IsProcedural() const { return !bMontageMode; }

	/** A clip (UOSFClipAnimInstance) is playing for this action. */
	bool IsClipMode() const { return bClipMode; }

	/** Timing actually used by the timeline (GetTiming, stretched to the clip in clip mode). */
	const FOSFActionTiming& GetEffectiveTiming() const { return bClipMode ? ClipTiming : GetTiming(); }

	/** Clip mode: seconds from the start of the action to the clip's hit frame (-1 otherwise). */
	float GetClipImpactTime() const { return bClipMode ? ClipImpactTime : -1.f; }

protected:
	/** Override to pick a montage at activation (defaults to Montage). */
	virtual UAnimMontage* GetMontageToPlay() const { return Montage; }
	virtual FName GetMontageSection() const { return NAME_None; }
	virtual float GetMontagePlayRate() const { return 1.f; }
	virtual const FOSFActionTiming& GetTiming() const { return Timing; }
	virtual const FOSFAnimClip* GetClipToPlay() const { return Clip.IsValid() ? &Clip : nullptr; }
	virtual float GetStaminaCost() const { return StaminaCost; }
	virtual float GetFPCost() const { return FPCost; }

	/** Called first in Start(), before costs are committed and the montage is chosen. */
	virtual void OnPreStart() {}
	virtual void OnStarted() {}
	virtual void OnTick(float DeltaTime) {}
	virtual void OnEnded(bool bInterrupted) {}
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) {}
	virtual void OnPhaseChanged(EOSFActionPhase NewPhase) {}

	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "On Started"))
	void K2_OnStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "On Ended"))
	void K2_OnEnded(bool bInterrupted);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "On Window Changed"))
	void K2_OnWindowChanged(EOSFActionWindow Window, bool bOpen);

	/** Ends the action from inside (procedural end, release, montage end). */
	UFUNCTION(BlueprintCallable, Category = "Action")
	void EndAction();

	void RotateTowardsDesiredDirection(float DeltaTime, float RateDegPerSec);

	UPROPERTY(Transient)
	TObjectPtr<UOSFActionComponent> ActionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage = nullptr;

	TSet<EOSFActionWindow> OpenWindows;
	float Elapsed = 0.f;
	EOSFActionPhase Phase = EOSFActionPhase::None;
	bool bRunning = false;
	bool bMontageMode = false;
	bool bClipMode = false;
	bool bEnding = false;
	FOSFActionTiming ClipTiming;
	float ClipImpactTime = -1.f;

private:
	void UpdateProceduralTimeline();
	void SetPhase(EOSFActionPhase NewPhase);
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
};
