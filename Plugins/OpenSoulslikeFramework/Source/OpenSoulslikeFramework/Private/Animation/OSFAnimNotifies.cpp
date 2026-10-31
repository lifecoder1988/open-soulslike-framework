// Copyright Open Soulslike Framework contributors. MIT License.

#include "Animation/OSFAnimNotifies.h"
#include "Actions/OSFActionComponent.h"
#include "Stats/OSFStatComponent.h"
#include "OSFTypes.h"
#include "Components/SkeletalMeshComponent.h"

void UOSFAnimNotifyState_Window::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UOSFActionComponent* Actions = MeshComp ? OSF::Get<UOSFActionComponent>(MeshComp->GetOwner()) : nullptr)
	{
		Actions->NotifyWindow(Window, true);
	}
}

void UOSFAnimNotifyState_Window::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UOSFActionComponent* Actions = MeshComp ? OSF::Get<UOSFActionComponent>(MeshComp->GetOwner()) : nullptr)
	{
		Actions->NotifyWindow(Window, false);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UOSFAN_AdjustStat::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (UOSFStatComponent* Stats = MeshComp ? OSF::Get<UOSFStatComponent>(MeshComp->GetOwner()) : nullptr)
	{
		Stats->ModifyCurrent(Stat, Delta, Delta < 0.f);
	}
}
