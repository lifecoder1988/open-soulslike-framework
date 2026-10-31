// Copyright Open Soulslike Framework contributors. MIT License.

#include "Stats/OSFBuffComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "OSFGameplayTags.h"

UOSFBuffComponent::UOSFBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSFBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TArray<UOSFBuffData*> Expired;
	float HealthDelta = 0.f;
	for (FOSFActiveBuff& Active : ActiveBuffs)
	{
		if (!Active.Buff)
		{
			continue;
		}
		HealthDelta += Active.Buff->HealthPerSecond * DeltaTime;
		if (Active.Duration > 0.f)
		{
			Active.Remaining -= DeltaTime;
			if (Active.Remaining <= 0.f)
			{
				Expired.Add(Active.Buff);
			}
		}
	}

	if (!FMath::IsNearlyZero(HealthDelta))
	{
		if (HealthDelta > 0.f)
		{
			if (UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner()))
			{
				Stats->ModifyCurrent(OSFTags::Stat_Health, HealthDelta);
			}
		}
		else if (UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(GetOwner()))
		{
			Combat->ApplyDirectDamage(-HealthDelta, nullptr);
		}
	}

	for (UOSFBuffData* Buff : Expired)
	{
		RemoveBuff(Buff);
	}
}

void UOSFBuffComponent::ApplyBuff(UOSFBuffData* Buff, float DurationOverride)
{
	if (!Buff)
	{
		return;
	}
	// 0 = use the asset duration, < 0 = infinite (talisman passives), > 0 = explicit duration.
	const float Duration = DurationOverride != 0.f ? DurationOverride : Buff->Duration;
	if (FOSFActiveBuff* Existing = ActiveBuffs.FindByPredicate([Buff](const FOSFActiveBuff& A) { return A.Buff == Buff; }))
	{
		Existing->Duration = Duration;
		Existing->Remaining = Duration;
		return;
	}

	FOSFActiveBuff& Active = ActiveBuffs.AddDefaulted_GetRef();
	Active.Buff = Buff;
	Active.Duration = Duration;
	Active.Remaining = Duration;
	ApplyModifiers(Buff, true);
	OnBuffChanged.Broadcast(Buff, true);
}

void UOSFBuffComponent::RemoveBuff(UOSFBuffData* Buff)
{
	const int32 Removed = ActiveBuffs.RemoveAll([Buff](const FOSFActiveBuff& A) { return A.Buff == Buff; });
	if (Removed > 0)
	{
		ApplyModifiers(Buff, false);
		OnBuffChanged.Broadcast(Buff, false);
	}
}

void UOSFBuffComponent::RemoveAllBuffs(bool bOnlyTimed)
{
	TArray<UOSFBuffData*> ToRemove;
	for (const FOSFActiveBuff& Active : ActiveBuffs)
	{
		if (!bOnlyTimed || Active.Duration > 0.f)
		{
			ToRemove.Add(Active.Buff);
		}
	}
	for (UOSFBuffData* Buff : ToRemove)
	{
		RemoveBuff(Buff);
	}
}

bool UOSFBuffComponent::HasBuff(const UOSFBuffData* Buff) const
{
	return ActiveBuffs.ContainsByPredicate([Buff](const FOSFActiveBuff& A) { return A.Buff == Buff; });
}

bool UOSFBuffComponent::HasBuffWithTag(FGameplayTag Tag) const
{
	return ActiveBuffs.ContainsByPredicate([Tag](const FOSFActiveBuff& A) { return A.Buff && A.Buff->BuffTag.MatchesTag(Tag); });
}

float UOSFBuffComponent::GetAttackPowerMultiplier() const
{
	float Value = 1.f;
	for (const FOSFActiveBuff& A : ActiveBuffs) { if (A.Buff) { Value *= A.Buff->AttackPowerMultiplier; } }
	return Value;
}

float UOSFBuffComponent::GetDamageTakenMultiplier() const
{
	float Value = 1.f;
	for (const FOSFActiveBuff& A : ActiveBuffs) { if (A.Buff) { Value *= A.Buff->DamageTakenMultiplier; } }
	return Value;
}

float UOSFBuffComponent::GetStaminaRegenMultiplier() const
{
	float Value = 1.f;
	for (const FOSFActiveBuff& A : ActiveBuffs) { if (A.Buff) { Value *= A.Buff->StaminaRegenMultiplier; } }
	return Value;
}

void UOSFBuffComponent::AppendGrantedTags(FGameplayTagContainer& OutTags) const
{
	for (const FOSFActiveBuff& A : ActiveBuffs)
	{
		if (A.Buff)
		{
			OutTags.AppendTags(A.Buff->GrantedTags);
		}
	}
}

void UOSFBuffComponent::ApplyModifiers(UOSFBuffData* Buff, bool bAdd)
{
	UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	if (!Stats || !Buff || Buff->StatModifiers.Num() == 0)
	{
		return;
	}
	if (bAdd)
	{
		Stats->SetModifiers(this, Buff->GetFName(), Buff->StatModifiers);
	}
	else
	{
		Stats->RemoveModifiers(this, Buff->GetFName());
	}
}
