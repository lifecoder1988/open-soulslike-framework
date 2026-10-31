// Copyright Open Soulslike Framework contributors. MIT License.

#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Engine/World.h"

UOSFActionComponent::UOSFActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UOSFActionComponent::BeginPlay()
{
	Super::BeginPlay();
	CreateDefaultActions();
}

double UOSFActionComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UOSFActionComponent::CreateDefaultActions()
{
	if (bActionsCreated)
	{
		return;
	}
	bActionsCreated = true;

	if (DefaultActions.Num() == 0)
	{
		DefaultActions = {
			UOSFAction_LightAttack::StaticClass(),
			UOSFAction_HeavyAttack::StaticClass(),
			UOSFAction_Dodge::StaticClass(),
			UOSFAction_Guard::StaticClass(),
			UOSFAction_WeaponSkill::StaticClass(),
			UOSFAction_Parry::StaticClass(),
			UOSFAction_UseItem::StaticClass(),
			UOSFAction_Execute::StaticClass(),
			UOSFAction_AIAbility::StaticClass()
		};
	}

	for (const TSubclassOf<UOSFAction>& ActionClass : DefaultActions)
	{
		if (UOSFAction* Action = AddAction(ActionClass))
		{
			DefaultActionInstances.Add(Action->ActionTag, Action);
		}
	}
}

UOSFAction* UOSFActionComponent::AddAction(TSubclassOf<UOSFAction> ActionClass)
{
	if (!ActionClass || ActionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	UOSFAction* Action = NewObject<UOSFAction>(this, ActionClass);
	Action->Initialize(this);
	if (Action->ActionTag.IsValid())
	{
		Actions.Add(Action->ActionTag, Action);
	}
	return Action;
}

void UOSFActionComponent::SetActionOverride(FGameplayTag ActionTag, TSubclassOf<UOSFAction> ActionClass)
{
	CreateDefaultActions();
	if (!ActionClass)
	{
		if (TObjectPtr<UOSFAction>* Default = DefaultActionInstances.Find(ActionTag))
		{
			Actions.Add(ActionTag, *Default);
		}
		return;
	}
	UOSFAction* Action = NewObject<UOSFAction>(this, ActionClass);
	Action->Initialize(this);
	Action->ActionTag = ActionTag;
	Actions.Add(ActionTag, Action);
}

UOSFAction* UOSFActionComponent::FindAction(FGameplayTag ActionTag) const
{
	const TObjectPtr<UOSFAction>* Found = Actions.Find(ActionTag);
	return Found ? Found->Get() : nullptr;
}

bool UOSFActionComponent::TryExecuteAction(FGameplayTag ActionTag, bool bAllowBuffer)
{
	CreateDefaultActions();
	UOSFAction* Action = FindAction(ActionTag);
	if (!Action)
	{
		return false;
	}
	if (UOSFAction* Redirect = Action->GetRedirect())
	{
		Action = Redirect;
	}

	if (CurrentAction)
	{
		if (CurrentAction == Action && Action->bIsHoldAction)
		{
			return false;
		}
		if (CurrentAction->CanBeCancelledBy(Action))
		{
			// The cancel check must not see the running action's own tags (Busy, Attacking...).
			UOSFAction* Previous = CurrentAction;
			HandleActionFinished(Previous, true);
			if (Action->CanActivate())
			{
				ClearInputBuffer();
				return StartActionInstance(Action);
			}
			return false;
		}
		if (bAllowBuffer && Action->bCanBeBuffered)
		{
			BufferedAction = ActionTag;
			BufferedTime = Now();
			BufferedDuring = CurrentAction;
		}
		return false;
	}

	if (Action->CanActivate())
	{
		ClearInputBuffer();
		return StartActionInstance(Action);
	}
	return false;
}

bool UOSFActionComponent::StartActionInstance(UOSFAction* Action)
{
	if (!Action)
	{
		return false;
	}
	if (CurrentAction && CurrentAction != Action)
	{
		HandleActionFinished(CurrentAction, true);
	}
	CurrentAction = Action;
	Action->Start();
	OnActionStarted.Broadcast(this, Action);
	return true;
}

void UOSFActionComponent::StopAction(FGameplayTag ActionTag)
{
	if (BufferedAction == ActionTag)
	{
		ClearInputBuffer();
	}
	if (CurrentAction && CurrentAction->ActionTag == ActionTag)
	{
		CurrentAction->Release();
	}
}

void UOSFActionComponent::CancelCurrentAction()
{
	ClearInputBuffer();
	if (CurrentAction)
	{
		HandleActionFinished(CurrentAction, true);
	}
}

void UOSFActionComponent::HandleActionFinished(UOSFAction* Action, bool bInterrupted)
{
	if (!Action)
	{
		return;
	}
	if (Action == CurrentAction)
	{
		CurrentAction = nullptr;
	}
	Action->Finish(bInterrupted);
	LastFinishedAction = Action;
	LastActionEndTime = Now();
	OnActionEnded.Broadcast(this, Action, bInterrupted);
}

void UOSFActionComponent::NotifyWindow(EOSFActionWindow Window, bool bOpen)
{
	if (CurrentAction)
	{
		CurrentAction->SetWindow(Window, bOpen);
	}
}

void UOSFActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (CurrentAction)
	{
		CurrentAction->TickAction(DeltaTime);
	}
	ProcessInputBuffer();
}

void UOSFActionComponent::ProcessInputBuffer()
{
	if (!BufferedAction.IsValid())
	{
		return;
	}
	// An input pressed during an action stays buffered until that action can be cancelled (or ends).
	if (CurrentAction && CurrentAction == BufferedDuring.Get() && !CurrentAction->IsWindowOpen(EOSFActionWindow::Cancel))
	{
		BufferedTime = Now();
	}
	if (Now() - BufferedTime > InputBufferTime)
	{
		ClearInputBuffer();
		return;
	}
	UOSFAction* Buffered = FindAction(BufferedAction);
	if (!Buffered)
	{
		ClearInputBuffer();
		return;
	}
	if (UOSFAction* Redirect = Buffered->GetRedirect())
	{
		Buffered = Redirect;
	}
	if (!CurrentAction || CurrentAction->CanBeCancelledBy(Buffered))
	{
		const FGameplayTag Tag = BufferedAction;
		ClearInputBuffer();
		TryExecuteAction(Tag, false);
	}
}

void UOSFActionComponent::AddStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}
	int32& Count = StateTagCounts.FindOrAdd(Tag);
	if (++Count == 1)
	{
		StateTags.AddTag(Tag);
		OnStateTagChanged.Broadcast(Tag, true);
	}
}

void UOSFActionComponent::RemoveStateTag(FGameplayTag Tag)
{
	int32* Count = StateTagCounts.Find(Tag);
	if (!Count || *Count <= 0)
	{
		return;
	}
	if (--(*Count) == 0)
	{
		StateTags.RemoveTag(Tag);
		StateTagCounts.Remove(Tag);
		OnStateTagChanged.Broadcast(Tag, false);
	}
}

void UOSFActionComponent::ClearStateTag(FGameplayTag Tag)
{
	if (StateTagCounts.Remove(Tag) > 0)
	{
		StateTags.RemoveTag(Tag);
		OnStateTagChanged.Broadcast(Tag, false);
	}
}

void UOSFActionComponent::AddStateTags(const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		AddStateTag(Tag);
	}
}

void UOSFActionComponent::RemoveStateTags(const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		RemoveStateTag(Tag);
	}
}
