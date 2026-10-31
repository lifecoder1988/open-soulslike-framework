// Copyright Open Soulslike Framework contributors. MIT License.

#include "Stats/OSFStatusEffectComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "OSFGameplayTags.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "OSFStatus"

UOSFStatusEffectData* UOSFStatusEffectData::CreateDefault(UObject* Outer, FGameplayTag Tag)
{
	UOSFStatusEffectData* Data = NewObject<UOSFStatusEffectData>(Outer);
	Data->StatusTag = Tag;
	if (Tag == OSFTags::Status_Bleed)
	{
		Data->DisplayName = LOCTEXT("Bleed", "Hemorrhage");
		Data->Color = FLinearColor(0.75f, 0.02f, 0.05f);
		Data->Threshold = 100.f;
		Data->FlatDamage = 60.f;
		Data->PercentMaxHealthDamage = 0.15f;
	}
	else if (Tag == OSFTags::Status_Poison)
	{
		Data->DisplayName = LOCTEXT("Poison", "Poison");
		Data->Color = FLinearColor(0.35f, 0.6f, 0.1f);
		Data->Threshold = 120.f;
		Data->DamagePerSecond = 6.f;
		Data->PercentMaxHealthPerSecond = 0.002f;
		Data->Duration = 25.f;
	}
	else if (Tag == OSFTags::Status_Rot)
	{
		Data->DisplayName = LOCTEXT("Rot", "Scarlet Rot");
		Data->Color = FLinearColor(0.85f, 0.25f, 0.1f);
		Data->Threshold = 150.f;
		Data->DamagePerSecond = 12.f;
		Data->PercentMaxHealthPerSecond = 0.0035f;
		Data->Duration = 30.f;
	}
	else if (Tag == OSFTags::Status_Frostbite)
	{
		Data->DisplayName = LOCTEXT("Frostbite", "Frostbite");
		Data->Color = FLinearColor(0.55f, 0.8f, 1.f);
		Data->Threshold = 110.f;
		Data->FlatDamage = 30.f;
		Data->PercentMaxHealthDamage = 0.1f;
		Data->Duration = 20.f;

		UOSFBuffData* Debuff = NewObject<UOSFBuffData>(Data);
		Debuff->DisplayName = LOCTEXT("FrostbiteDebuff", "Frostbitten");
		Debuff->BuffTag = Tag;
		Debuff->Duration = 20.f;
		Debuff->DamageTakenMultiplier = 1.2f;
		Debuff->StaminaRegenMultiplier = 0.5f;
		Debuff->Color = Data->Color;
		Data->ApplyBuff = Debuff;
	}
	else
	{
		Data->DisplayName = FText::FromName(Tag.GetTagName());
	}
	return Data;
}

UOSFStatusEffectComponent::UOSFStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSFStatusEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaults();
}

void UOSFStatusEffectComponent::EnsureDefaults()
{
	for (const FGameplayTag& Tag : { OSFTags::Status_Bleed.GetTag(), OSFTags::Status_Poison.GetTag(), OSFTags::Status_Rot.GetTag(), OSFTags::Status_Frostbite.GetTag() })
	{
		if (!FindEffect(Tag))
		{
			Effects.Add(UOSFStatusEffectData::CreateDefault(this, Tag));
		}
	}
}

UOSFStatusEffectData* UOSFStatusEffectComponent::FindEffect(FGameplayTag StatusTag) const
{
	for (UOSFStatusEffectData* Effect : Effects)
	{
		if (Effect && Effect->StatusTag == StatusTag)
		{
			return Effect;
		}
	}
	return nullptr;
}

float UOSFStatusEffectComponent::GetThreshold(FGameplayTag StatusTag) const
{
	const UOSFStatusEffectData* Effect = FindEffect(StatusTag);
	if (!Effect)
	{
		return 0.f;
	}
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	const float Resistance = Stats ? Stats->GetModifierAdditive(StatusTag) : 0.f;
	return FMath::Max(1.f, Effect->Threshold + Resistance);
}

bool UOSFStatusEffectComponent::AddBuildup(FGameplayTag StatusTag, float Amount)
{
	if (Amount <= 0.f)
	{
		return false;
	}
	EnsureDefaults();
	UOSFStatusEffectData* Effect = FindEffect(StatusTag);
	if (!Effect)
	{
		return false;
	}

	FOSFStatusState& State = States.FindOrAdd(StatusTag);
	// While an effect is active the meter is locked (souls behaviour).
	if (State.ActiveRemaining > 0.f)
	{
		return false;
	}
	State.Buildup += Amount;
	State.LastBuildupTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (State.Buildup >= GetThreshold(StatusTag))
	{
		State.Buildup = 0.f;
		State.ActiveRemaining = Effect->Duration;
		Trigger(Effect);
		return true;
	}
	return false;
}

void UOSFStatusEffectComponent::Trigger(UOSFStatusEffectData* Effect)
{
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	const float MaxHealth = Stats ? Stats->GetMax(OSFTags::Stat_Health) : 0.f;
	const float Instant = Effect->FlatDamage + Effect->PercentMaxHealthDamage * MaxHealth;
	if (Instant > 0.f)
	{
		DealDamage(Instant);
	}
	if (Effect->ApplyBuff)
	{
		if (UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(GetOwner()))
		{
			Buffs->ApplyBuff(Effect->ApplyBuff, Effect->Duration);
		}
	}
	OnStatusTriggered.Broadcast(this, Effect);
}

void UOSFStatusEffectComponent::DealDamage(float Amount)
{
	if (UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(GetOwner()))
	{
		Combat->ApplyDirectDamage(Amount, nullptr);
	}
	else if (UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner()))
	{
		Stats->ModifyCurrent(OSFTags::Stat_Health, -Amount);
	}
}

void UOSFStatusEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(GetOwner());
	const float MaxHealth = Stats ? Stats->GetMax(OSFTags::Stat_Health) : 0.f;

	float Dot = 0.f;
	for (TPair<FGameplayTag, FOSFStatusState>& Pair : States)
	{
		FOSFStatusState& State = Pair.Value;
		const UOSFStatusEffectData* Effect = FindEffect(Pair.Key);
		if (!Effect)
		{
			continue;
		}
		if (State.ActiveRemaining > 0.f)
		{
			Dot += (Effect->DamagePerSecond + Effect->PercentMaxHealthPerSecond * MaxHealth) * DeltaTime;
			State.ActiveRemaining = FMath::Max(0.f, State.ActiveRemaining - DeltaTime);
		}
		else if (State.Buildup > 0.f && Now - State.LastBuildupTime > Effect->DecayDelay)
		{
			State.Buildup = FMath::Max(0.f, State.Buildup - Effect->DecayPerSecond * DeltaTime);
		}
	}
	if (Dot > 0.f)
	{
		DealDamage(Dot);
	}
}

void UOSFStatusEffectComponent::Cure(FGameplayTag StatusTag)
{
	if (FOSFStatusState* State = States.Find(StatusTag))
	{
		State->Buildup = 0.f;
		State->ActiveRemaining = 0.f;
		if (const UOSFStatusEffectData* Effect = FindEffect(StatusTag))
		{
			if (Effect->ApplyBuff)
			{
				if (UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(GetOwner()))
				{
					Buffs->RemoveBuff(Effect->ApplyBuff);
				}
			}
		}
		OnStatusCured.Broadcast(this, StatusTag);
	}
}

void UOSFStatusEffectComponent::CureAll()
{
	TArray<FGameplayTag> Keys;
	States.GetKeys(Keys);
	for (const FGameplayTag& Tag : Keys)
	{
		Cure(Tag);
	}
}

float UOSFStatusEffectComponent::GetBuildupPercent(FGameplayTag StatusTag) const
{
	const FOSFStatusState* State = States.Find(StatusTag);
	if (!State)
	{
		return 0.f;
	}
	if (State->ActiveRemaining > 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(State->Buildup / GetThreshold(StatusTag), 0.f, 1.f);
}

bool UOSFStatusEffectComponent::IsEffectActive(FGameplayTag StatusTag) const
{
	const FOSFStatusState* State = States.Find(StatusTag);
	return State && State->ActiveRemaining > 0.f;
}

#undef LOCTEXT_NAMESPACE
