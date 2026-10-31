// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Actions/OSFAction.h"
#include "OSFActionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnActionStarted, UOSFActionComponent*, Component, UOSFAction*, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSFOnActionEnded, UOSFActionComponent*, Component, UOSFAction*, Action, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnStateTagChanged, FGameplayTag, Tag, bool, bAdded);

/**
 * Action manager + input buffer + state tags.
 *
 * Only one action runs at a time. Inputs that arrive while an action is running are buffered and fire
 * as soon as the running action reaches its Cancel window (or ends) - the classic soulslike input buffer.
 * The component is also the owner of the character's state tags (Attacking, Guarding, Invincible...).
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFActionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Action classes instanced at BeginPlay. Defaults: light/heavy attack, dodge, guard, weapon skill, use item, execute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Actions")
	TArray<TSubclassOf<UOSFAction>> DefaultActions;

	/** How long a buffered input stays valid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Actions")
	float InputBufferTime = 0.6f;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Actions")
	FOSFOnActionStarted OnActionStarted;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Actions")
	FOSFOnActionEnded OnActionEnded;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Actions")
	FOSFOnStateTagChanged OnStateTagChanged;

	// --- Actions ---

	/** Executes now, or buffers the request. Returns true if the action started immediately. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	bool TryExecuteAction(FGameplayTag ActionTag, bool bAllowBuffer = true);

	/** Releases a hold action (guard). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	void StopAction(FGameplayTag ActionTag);

	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	void CancelCurrentAction();

	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	void ClearInputBuffer() { BufferedAction = FGameplayTag(); }

	UFUNCTION(BlueprintPure, Category = "OSF|Actions")
	UOSFAction* GetCurrentAction() const { return CurrentAction; }

	UFUNCTION(BlueprintPure, Category = "OSF|Actions")
	bool IsBusy() const { return CurrentAction != nullptr; }

	UFUNCTION(BlueprintPure, Category = "OSF|Actions")
	UOSFAction* FindAction(FGameplayTag ActionTag) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	UOSFAction* AddAction(TSubclassOf<UOSFAction> ActionClass);

	/** Replaces the action bound to a tag (e.g. a weapon-specific skill). Passing null restores the default. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	void SetActionOverride(FGameplayTag ActionTag, TSubclassOf<UOSFAction> ActionClass);

	UFUNCTION(BlueprintPure, Category = "OSF|Actions")
	FGameplayTag GetBufferedAction() const { return BufferedAction; }

	/** Starts an action instance directly, bypassing buffering (used by redirects, AI and executions). */
	bool StartActionInstance(UOSFAction* Action);

	/** Anim notify entry point - forwards to the running action. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Actions")
	void NotifyWindow(EOSFActionWindow Window, bool bOpen);

	/** Called by the running action when it finishes. */
	void HandleActionFinished(UOSFAction* Action, bool bInterrupted);

	/** The last attack action that ended, and when (for combo chaining). */
	TWeakObjectPtr<UOSFAction> LastFinishedAction;
	double LastActionEndTime = -1.0;

	// --- State tags ---

	UFUNCTION(BlueprintCallable, Category = "OSF|State")
	void AddStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "OSF|State")
	void RemoveStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "OSF|State")
	void AddStateTags(const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintCallable, Category = "OSF|State")
	void RemoveStateTags(const FGameplayTagContainer& Tags);

	/** Removes every stack of a tag. */
	UFUNCTION(BlueprintCallable, Category = "OSF|State")
	void ClearStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "OSF|State")
	bool HasStateTag(FGameplayTag Tag) const { return StateTags.HasTag(Tag); }

	const FGameplayTagContainer& GetStateTags() const { return StateTags; }

protected:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UOSFAction>> Actions;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UOSFAction>> DefaultActionInstances;

	UPROPERTY(Transient)
	TObjectPtr<UOSFAction> CurrentAction = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "OSF|State")
	FGameplayTagContainer StateTags;

	TMap<FGameplayTag, int32> StateTagCounts;

	FGameplayTag BufferedAction;
	double BufferedTime = 0.0;
	TWeakObjectPtr<UOSFAction> BufferedDuring;
	bool bActionsCreated = false;

	void CreateDefaultActions();
	void ProcessInputBuffer();
	double Now() const;
};
