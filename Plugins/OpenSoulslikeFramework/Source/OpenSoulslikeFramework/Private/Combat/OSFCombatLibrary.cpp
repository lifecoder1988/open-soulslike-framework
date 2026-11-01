// Copyright Open Soulslike Framework contributors. MIT License.

#include "Combat/OSFCombatLibrary.h"
#include "Combat/OSFCombatComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Items/OSFItemData.h"
#include "OSFGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

TMap<FGameplayTag, float> UOSFCombatLibrary::GetAttackRating(const AActor* Attacker, const UOSFWeaponData* Weapon, bool bTwoHanded)
{
	TMap<FGameplayTag, float> Rating;
	const UOSFStatComponent* Stats = OSF::Get<UOSFStatComponent>(Attacker);
	if (!Weapon)
	{
		Rating.Add(OSFTags::Damage_Physical, 40.f + (Stats ? Stats->GetAttribute(OSFTags::Attribute_Strength) * 1.5f : 0.f));
		return Rating;
	}

	auto GetAttr = [Stats, bTwoHanded](FGameplayTag Attribute) -> float
	{
		float Value = Stats ? static_cast<float>(Stats->GetAttribute(Attribute)) : 10.f;
		if (bTwoHanded && Attribute == OSFTags::Attribute_Strength)
		{
			Value *= 1.5f;
		}
		return Value;
	};

	bool bMeetsRequirements = true;
	for (const TPair<FGameplayTag, int32>& Requirement : Weapon->Requirements)
	{
		if (GetAttr(Requirement.Key) < Requirement.Value)
		{
			bMeetsRequirements = false;
		}
	}

	for (const TPair<FGameplayTag, float>& Base : Weapon->BaseDamage)
	{
		float Value = Base.Value;
		for (const TPair<FGameplayTag, float>& Scale : Weapon->Scaling)
		{
			Value += Base.Value * Scale.Value * (GetAttr(Scale.Key) / 100.f);
		}
		if (!bMeetsRequirements)
		{
			Value *= 0.6f;
		}
		Rating.Add(Base.Key, Value);
	}
	return Rating;
}

FOSFDamageSpec UOSFCombatLibrary::MakeAttackDamageSpec(const AActor* Attacker, const UOSFWeaponData* Weapon, const FOSFAttackDefinition& Attack, bool bTwoHanded)
{
	FOSFDamageSpec Spec;
	const UOSFBuffComponent* Buffs = OSF::Get<UOSFBuffComponent>(Attacker);
	const float BuffMultiplier = Buffs ? Buffs->GetAttackPowerMultiplier() : 1.f;

	for (const TPair<FGameplayTag, float>& Pair : GetAttackRating(Attacker, Weapon, bTwoHanded))
	{
		Spec.Damage.Add(Pair.Key, Pair.Value * Attack.MotionValue * BuffMultiplier);
	}
	for (const TPair<FGameplayTag, float>& Pair : Attack.BonusDamage)
	{
		Spec.Damage.FindOrAdd(Pair.Key) += Pair.Value * BuffMultiplier;
	}

	const float BasePoise = Weapon ? Weapon->PoiseDamage : 6.f;
	Spec.PoiseDamage = BasePoise * Attack.PoiseDamageMultiplier * (bTwoHanded ? 1.2f : 1.f);
	if (Weapon)
	{
		Spec.StatusBuildup = Weapon->StatusBuildup;
	}
	Spec.bCanBeGuarded = Attack.bCanBeGuarded;
	Spec.bCanBeParried = Attack.bCanBeParried;
	Spec.SourceTags.AddTag(OSFTags::Source_Melee);
	return Spec;
}

uint16 UOSFCombatLibrary::ApplyMovementBurst(ACharacter* Character, const FVector& Direction, float Distance, float Duration, FName InstanceName)
{
	if (!Character || Duration <= 0.f || Distance <= 0.f)
	{
		return 0;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return 0;
	}
	TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = InstanceName;
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 10;
	Force->Force = Direction.GetSafeNormal2D() * (Distance / Duration);
	Force->Duration = Duration;
	Force->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Force->FinishVelocityParams.ClampVelocity = 100.f;
	return Movement->ApplyRootMotionSource(Force);
}

void UOSFCombatLibrary::StopMovementBurst(ACharacter* Character, uint16& InOutId)
{
	if (Character && InOutId != 0)
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSourceByID(InOutId);
		}
	}
	InOutId = 0;
}

bool UOSFCombatLibrary::HasStateTag(const AActor* Actor, FGameplayTag Tag)
{
	const UOSFActionComponent* Actions = OSF::Get<UOSFActionComponent>(Actor);
	return Actions && Actions->HasStateTag(Tag);
}

bool UOSFCombatLibrary::IsAlive(const AActor* Actor)
{
	const UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(Actor);
	return Combat && !Combat->IsDead();
}

FString UOSFCombatLibrary::GetScalingGrade(float Coefficient)
{
	if (Coefficient >= 1.75f) return TEXT("S");
	if (Coefficient >= 1.4f) return TEXT("A");
	if (Coefficient >= 0.9f) return TEXT("B");
	if (Coefficient >= 0.6f) return TEXT("C");
	if (Coefficient >= 0.25f) return TEXT("D");
	if (Coefficient > 0.f) return TEXT("E");
	return TEXT("-");
}
