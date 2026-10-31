// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFAICombatComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "OSFGameplayTags.h"
#include "Engine/World.h"

UOSFAICombatComponent::UOSFAICombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

double UOSFAICombatComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UOSFAICombatComponent::EnsureDefaultAbilities()
{
	if (Abilities.Num() > 0)
	{
		return;
	}
	FOSFAIAbility Slash;
	Slash.Name = TEXT("Slash");
	Slash.Attack.Timing.Windup = 0.55f;
	Slash.Attack.Timing.Active = 0.2f;
	Slash.Attack.Timing.Recovery = 0.6f;
	Slash.Attack.Reach = 200.f;
	Slash.MaxRange = 230.f;
	Slash.Cooldown = 1.2f;
	Slash.Weight = 3.f;
	Slash.FollowUp = 0;
	Slash.FollowUpChance = 0.35f;
	Abilities.Add(Slash);

	FOSFAIAbility Overhead;
	Overhead.Name = TEXT("Overhead");
	Overhead.Attack.Timing.Windup = 0.9f;
	Overhead.Attack.Timing.Active = 0.2f;
	Overhead.Attack.Timing.Recovery = 0.9f;
	Overhead.Attack.MotionValue = 1.7f;
	Overhead.Attack.PoiseDamageMultiplier = 2.f;
	Overhead.Attack.SwingYaw = 0.f;
	Overhead.Attack.SwingPitch = -120.f;
	Overhead.Attack.Reach = 220.f;
	Overhead.MaxRange = 240.f;
	Overhead.Cooldown = 4.f;
	Abilities.Add(Overhead);

	FOSFAIAbility Lunge;
	Lunge.Name = TEXT("Lunge");
	Lunge.Attack.Timing.Windup = 0.7f;
	Lunge.Attack.Timing.Active = 0.3f;
	Lunge.Attack.Timing.Recovery = 0.8f;
	Lunge.Attack.Lunge = 900.f;
	Lunge.Attack.Reach = 220.f;
	Lunge.Attack.SwingYaw = 0.f;
	Lunge.Attack.SwingPitch = -40.f;
	Lunge.MinRange = 280.f;
	Lunge.MaxRange = 650.f;
	Lunge.Cooldown = 6.f;
	Abilities.Add(Lunge);
}

bool UOSFAICombatComponent::IsOnCooldown(int32 Index) const
{
	const double* Until = CooldownUntil.Find(Index);
	return Until && Now() < *Until;
}

int32 UOSFAICombatComponent::SelectAbility(float DistanceToTarget, int32 Phase) const
{
	float TotalWeight = 0.f;
	TArray<int32, TInlineAllocator<8>> Valid;
	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FOSFAIAbility& Ability = Abilities[Index];
		if (IsOnCooldown(Index) || Phase < Ability.MinPhase || Phase > Ability.MaxPhase)
		{
			continue;
		}
		if (DistanceToTarget < Ability.MinRange || DistanceToTarget > Ability.MaxRange)
		{
			continue;
		}
		Valid.Add(Index);
		TotalWeight += Ability.Weight;
	}
	if (Valid.Num() == 0 || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}
	float Roll = FMath::FRand() * TotalWeight;
	for (int32 Index : Valid)
	{
		Roll -= Abilities[Index].Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Valid.Last();
}

float UOSFAICombatComponent::GetMaxAbilityRange(int32 Phase) const
{
	float Range = 0.f;
	for (const FOSFAIAbility& Ability : Abilities)
	{
		if (Phase >= Ability.MinPhase && Phase <= Ability.MaxPhase)
		{
			Range = FMath::Max(Range, Ability.MaxRange);
		}
	}
	return Range > 0.f ? Range : 200.f;
}

bool UOSFAICombatComponent::ExecuteAbility(int32 Index)
{
	if (!Abilities.IsValidIndex(Index))
	{
		return false;
	}
	UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(GetOwner());
	UOSFAction_AIAbility* Action = Actions ? Cast<UOSFAction_AIAbility>(Actions->FindAction(OSFTags::Action_AIAbility)) : nullptr;
	if (!Action)
	{
		return false;
	}
	Action->AttackOverride = Abilities[Index].Attack;
	if (!Actions->TryExecuteAction(OSFTags::Action_AIAbility, false))
	{
		Action->AttackOverride.Reset();
		return false;
	}
	CooldownUntil.Add(Index, Now() + Abilities[Index].Cooldown);
	LastAbility = Index;
	return true;
}

int32 UOSFAICombatComponent::RollFollowUp() const
{
	if (!Abilities.IsValidIndex(LastAbility))
	{
		return INDEX_NONE;
	}
	const FOSFAIAbility& Last = Abilities[LastAbility];
	if (Abilities.IsValidIndex(Last.FollowUp) && FMath::FRand() < Last.FollowUpChance)
	{
		return Last.FollowUp;
	}
	return INDEX_NONE;
}
