// Copyright Open Soulslike Framework contributors. MIT License.

#include "Progression/OSFProgressComponent.h"

UOSFProgressComponent::UOSFProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSFProgressComponent::AddProgress(FGameplayTag Tag)
{
	if (Tag.IsValid() && !ProgressTags.HasTagExact(Tag))
	{
		ProgressTags.AddTag(Tag);
		OnProgressChanged.Broadcast(Tag, true);
	}
}

void UOSFProgressComponent::RemoveProgress(FGameplayTag Tag)
{
	if (ProgressTags.RemoveTag(Tag))
	{
		OnProgressChanged.Broadcast(Tag, false);
	}
}

bool UOSFProgressComponent::MeetsRequirements(const FGameplayTagContainer& Required, const FGameplayTagContainer& Blocked) const
{
	return ProgressTags.HasAllExact(Required) && (Blocked.IsEmpty() || !ProgressTags.HasAnyExact(Blocked));
}
