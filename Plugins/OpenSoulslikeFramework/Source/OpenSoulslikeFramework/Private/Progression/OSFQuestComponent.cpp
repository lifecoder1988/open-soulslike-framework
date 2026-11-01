// Copyright Open Soulslike Framework contributors. MIT License.

#include "Progression/OSFQuestComponent.h"
#include "Progression/OSFProgressComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Characters/OSFPlayerCharacter.h"

#define LOCTEXT_NAMESPACE "OSFQuest"

UOSFQuestComponent::UOSFQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UOSFProgressComponent* UOSFQuestComponent::GetProgress() const
{
	return OSF::Get<UOSFProgressComponent>(GetOwner());
}

UOSFInventoryComponent* UOSFQuestComponent::GetInventory() const
{
	return OSF::Get<UOSFInventoryComponent>(GetOwner());
}

FName UOSFQuestComponent::KillCounter(FName KillKey)
{
	return FName(*FString::Printf(TEXT("Kill.%s"), *KillKey.ToString()));
}

FName UOSFQuestComponent::QuestFlag(const UOSFQuestData* Quest, const TCHAR* Suffix)
{
	return FName(*FString::Printf(TEXT("Quest.%s.%s"), *Quest->QuestId.ToString(), Suffix));
}

FName UOSFQuestComponent::BaselineKey(const UOSFQuestData* Quest, int32 ObjectiveIndex) const
{
	return FName(*FString::Printf(TEXT("Quest.%s.Base%d"), *Quest->QuestId.ToString(), ObjectiveIndex));
}

bool UOSFQuestComponent::IsQuestActive(const UOSFQuestData* Quest) const
{
	const UOSFProgressComponent* Progress = GetProgress();
	return Quest && Progress && Progress->HasFlag(QuestFlag(Quest, TEXT("Active"))) && !Progress->HasFlag(QuestFlag(Quest, TEXT("Done")));
}

bool UOSFQuestComponent::IsQuestComplete(const UOSFQuestData* Quest) const
{
	const UOSFProgressComponent* Progress = GetProgress();
	return Quest && Progress && Progress->HasFlag(QuestFlag(Quest, TEXT("Done")));
}

bool UOSFQuestComponent::StartQuest(UOSFQuestData* Quest)
{
	UOSFProgressComponent* Progress = GetProgress();
	if (!Quest || !Progress || IsQuestActive(Quest) || IsQuestComplete(Quest))
	{
		return false;
	}
	Quests.AddUnique(Quest);
	Progress->AddFlag(QuestFlag(Quest, TEXT("Active")));
	// Kill objectives count kills made after the quest started.
	for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
	{
		const FOSFQuestObjective& Objective = Quest->Objectives[Index];
		if (Objective.Type == EOSFQuestObjectiveType::Kill)
		{
			const FName Base = BaselineKey(Quest, Index);
			Progress->AddCounter(Base, Progress->GetCounter(KillCounter(Objective.Key)) - Progress->GetCounter(Base));
		}
	}
	LastObjective.Add(Quest, GetCurrentObjective(Quest));
	OnQuestStarted.Broadcast(this, Quest);
	if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(GetOwner()))
	{
		Player->Notify(FText::Format(LOCTEXT("Started", "New quest: {0}"), Quest->Title), EOSFNotifyType::Toast);
	}
	Evaluate();
	return true;
}

int32 UOSFQuestComponent::GetObjectiveProgress(const UOSFQuestData* Quest, int32 ObjectiveIndex) const
{
	const UOSFProgressComponent* Progress = GetProgress();
	if (!Quest || !Progress || !Quest->Objectives.IsValidIndex(ObjectiveIndex))
	{
		return 0;
	}
	if (IsQuestComplete(Quest))
	{
		return Quest->Objectives[ObjectiveIndex].Count;
	}
	const FOSFQuestObjective& Objective = Quest->Objectives[ObjectiveIndex];
	int32 Value = 0;
	switch (Objective.Type)
	{
	case EOSFQuestObjectiveType::Kill:
		Value = Progress->GetCounter(KillCounter(Objective.Key)) - Progress->GetCounter(BaselineKey(Quest, ObjectiveIndex));
		break;
	case EOSFQuestObjectiveType::Flag:
		Value = Progress->HasFlag(Objective.Key) ? Objective.Count : 0;
		break;
	case EOSFQuestObjectiveType::Progress:
		Value = Progress->HasProgress(Objective.ProgressTag) ? Objective.Count : 0;
		break;
	case EOSFQuestObjectiveType::Item:
		Value = GetInventory() ? GetInventory()->GetItemCount(Objective.Item) : 0;
		break;
	}
	return FMath::Clamp(Value, 0, Objective.Count);
}

int32 UOSFQuestComponent::GetCurrentObjective(const UOSFQuestData* Quest) const
{
	if (!Quest)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
	{
		if (GetObjectiveProgress(Quest, Index) < Quest->Objectives[Index].Count)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

TArray<UOSFQuestData*> UOSFQuestComponent::GetActiveQuests() const
{
	TArray<UOSFQuestData*> Out;
	for (UOSFQuestData* Quest : Quests)
	{
		if (IsQuestActive(Quest))
		{
			Out.Add(Quest);
		}
	}
	return Out;
}

void UOSFQuestComponent::NotifyKill(FName KillKey)
{
	if (UOSFProgressComponent* Progress = GetProgress(); Progress && !KillKey.IsNone())
	{
		Progress->AddCounter(KillCounter(KillKey));
		Evaluate();
	}
}

void UOSFQuestComponent::CompleteQuest(UOSFQuestData* Quest)
{
	UOSFProgressComponent* Progress = GetProgress();
	if (!Progress)
	{
		return;
	}
	Progress->AddFlag(QuestFlag(Quest, TEXT("Done")));
	for (const FName& Flag : Quest->CompletionFlags)
	{
		Progress->AddFlag(Flag);
	}
	if (UOSFInventoryComponent* Inventory = GetInventory())
	{
		for (const FOSFItemStack& Reward : Quest->RewardItems)
		{
			if (Reward.Item)
			{
				Inventory->AddItem(Reward.Item, Reward.Count);
			}
		}
		if (Quest->RewardCurrency > 0)
		{
			Inventory->AddCurrency(Quest->RewardCurrency);
		}
	}
	if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(GetOwner()))
	{
		Player->Notify(FText::Format(LOCTEXT("Completed", "Quest complete: {0}"), Quest->Title), EOSFNotifyType::Toast);
	}
	OnQuestCompleted.Broadcast(this, Quest);
}

void UOSFQuestComponent::Evaluate()
{
	const UOSFProgressComponent* Progress = GetProgress();
	if (!Progress)
	{
		return;
	}
	for (UOSFQuestData* Quest : TArray<TObjectPtr<UOSFQuestData>>(Quests))
	{
		if (!Quest || IsQuestComplete(Quest))
		{
			continue;
		}
		if (!IsQuestActive(Quest))
		{
			if (!Quest->StartFlag.IsNone() && Progress->HasFlag(Quest->StartFlag))
			{
				StartQuest(Quest);
			}
			continue;
		}
		const int32 Current = GetCurrentObjective(Quest);
		const int32* Last = LastObjective.Find(Quest);
		if (!Last || *Last != Current)
		{
			LastObjective.Add(Quest, Current);
			if (Last)
			{
				OnQuestUpdated.Broadcast(this, Quest);
			}
		}
		if (Current == INDEX_NONE)
		{
			CompleteQuest(Quest);
		}
	}
}

void UOSFQuestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EvaluateAccumulator += DeltaTime;
	if (EvaluateAccumulator >= 0.25f)
	{
		EvaluateAccumulator = 0.f;
		Evaluate();
	}
}

#undef LOCTEXT_NAMESPACE
