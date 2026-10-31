// Copyright Open Soulslike Framework contributors. MIT License.

#include "Interaction/OSFInteractionComponent.h"
#include "Interaction/OSFInteractable.h"
#include "Actions/OSFActionComponent.h"
#include "OSFGameplayTags.h"
#include "OSFTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

UOSFInteractionComponent::UOSFInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UOSFInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

void UOSFInteractionComponent::UpdateFocus()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	AActor* Best = nullptr;
	const UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(Owner);
	const bool bBlocked = Actions && (Actions->IsBusy() || Actions->HasStateTag(OSFTags::State_Dead) || Actions->HasStateTag(OSFTags::State_InMenu) || Actions->HasStateTag(OSFTags::State_OnLadder));
	if (!bBlocked)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFInteraction), false, Owner);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(SearchRadius), Params);

		float BestScore = -TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			if (!Candidate || Candidate == Owner || !Candidate->Implements<UOSFInteractable>())
			{
				continue;
			}
			if (!IOSFInteractable::Execute_CanInteract(Candidate, Owner))
			{
				continue;
			}
			const FVector To = Candidate->GetActorLocation() - Owner->GetActorLocation();
			const float Facing = FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D());
			const float Score = Facing - To.Size() / SearchRadius;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
			}
		}
	}

	if (Best != Focused.Get())
	{
		Focused = Best;
		OnFocusChanged.Broadcast(Best);
	}
}

FText UOSFInteractionComponent::GetFocusedPrompt() const
{
	AActor* Actor = Focused.Get();
	return Actor ? IOSFInteractable::Execute_GetInteractionPrompt(Actor, GetOwner()) : FText::GetEmpty();
}

bool UOSFInteractionComponent::TryInteract()
{
	UpdateFocus();
	AActor* Actor = Focused.Get();
	if (!Actor || !IOSFInteractable::Execute_CanInteract(Actor, GetOwner()))
	{
		return false;
	}
	IOSFInteractable::Execute_Interact(Actor, GetOwner());
	UpdateFocus();
	return true;
}
