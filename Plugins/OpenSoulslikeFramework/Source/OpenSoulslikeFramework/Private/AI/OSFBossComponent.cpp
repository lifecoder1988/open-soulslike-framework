// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFBossComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Characters/OSFCharacterBase.h"
#include "OSFGameplayTags.h"
#include "OSFTypes.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

UOSFBossComponent::UOSFBossComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSFBossComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner()))
	{
		Stats->OnStatChanged.AddDynamic(this, &UOSFBossComponent::HandleStatChanged);
	}
	// Sort phases from highest threshold to lowest.
	Phases.Sort([](const FOSFBossPhase& A, const FOSFBossPhase& B) { return A.HealthThreshold > B.HealthThreshold; });
}

void UOSFBossComponent::BeginFight()
{
	if (bFightActive || !bIsBoss)
	{
		return;
	}
	bFightActive = true;
	if (BossMusic)
	{
		MusicComponent = UGameplayStatics::SpawnSound2D(this, BossMusic, 0.8f);
	}
	OnFightChanged.Broadcast(this, true);
}

void UOSFBossComponent::EndFight()
{
	if (!bFightActive)
	{
		return;
	}
	bFightActive = false;
	if (MusicComponent)
	{
		MusicComponent->FadeOut(2.f, 0.f);
		MusicComponent = nullptr;
	}
	OnFightChanged.Broadcast(this, false);
}

void UOSFBossComponent::HandleStatChanged(UOSFStatComponent* StatComponent, FGameplayTag Stat, float NewValue, float Delta)
{
	if (Stat == OSFTags::Stat_Health && Delta < 0.f)
	{
		UpdatePhase();
	}
}

void UOSFBossComponent::UpdatePhase()
{
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	const UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(GetOwner());
	if (!bIsBoss || !Stats || (Combat && Combat->IsDead()))
	{
		return;
	}
	const float Percent = Stats->GetPercent(OSFTags::Stat_Health);
	int32 TargetPhase = 0;
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		if (Percent <= Phases[Index].HealthThreshold)
		{
			TargetPhase = Index + 1;
		}
	}
	while (CurrentPhase < TargetPhase)
	{
		const FOSFBossPhase& Phase = Phases[CurrentPhase];
		++CurrentPhase;
		if (UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(GetOwner()))
		{
			Buffs->ApplyBuff(Phase.PhaseBuff, -1.f);
		}
		if (UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(GetOwner()))
		{
			Actions->CancelCurrentAction();
		}
		if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetOwner()))
		{
			Character->BodyColor = Phase.PhaseColor;
		}
		OnPhaseChanged.Broadcast(this, CurrentPhase);
	}
}

void UOSFBossComponent::ResetBoss()
{
	EndFight();
	if (UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(GetOwner()))
	{
		for (const FOSFBossPhase& Phase : Phases)
		{
			Buffs->RemoveBuff(Phase.PhaseBuff);
		}
	}
	CurrentPhase = 0;
}
