// Copyright Open Soulslike Framework contributors. MIT License.

#include "Stats/OSFStatComponent.h"
#include "OSFGameplayTags.h"
#include "Items/OSFInventoryComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"

UOSFStatComponent::UOSFStatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	StatDefinitions = MakeDefaultStatDefinitions();
	Attributes = MakeDefaultAttributes();
}

TArray<FOSFStatDefinition> UOSFStatComponent::MakeDefaultStatDefinitions()
{
	TArray<FOSFStatDefinition> Defs;

	auto Add = [&Defs](FGameplayTag Tag, float BaseMax, float Regen, float Delay, FGameplayTag Attr, float PerPoint)
	{
		FOSFStatDefinition& Def = Defs.AddDefaulted_GetRef();
		Def.Tag = Tag;
		Def.BaseMax = BaseMax;
		Def.RegenPerSecond = Regen;
		Def.RegenDelay = Delay;
		if (Attr.IsValid())
		{
			FOSFAttributeScaling& Scaling = Def.AttributeScaling.AddDefaulted_GetRef();
			Scaling.Attribute = Attr;
			Scaling.PerPoint = PerPoint;
		}
	};

	Add(OSFTags::Stat_Health, 300.f, 0.f, 0.f, OSFTags::Attribute_Vigor, 25.f);
	Add(OSFTags::Stat_FP, 50.f, 0.f, 0.f, OSFTags::Attribute_Mind, 5.f);
	Add(OSFTags::Stat_Stamina, 60.f, 45.f, 0.8f, OSFTags::Attribute_Endurance, 4.f);
	Add(OSFTags::Stat_Poise, 30.f, 15.f, 3.f, FGameplayTag(), 0.f);
	Add(OSFTags::Stat_EquipLoad, 30.f, 0.f, 0.f, OSFTags::Attribute_Endurance, 1.5f);
	return Defs;
}

TMap<FGameplayTag, int32> UOSFStatComponent::MakeDefaultAttributes()
{
	TMap<FGameplayTag, int32> Attr;
	Attr.Add(OSFTags::Attribute_Vigor, 10);
	Attr.Add(OSFTags::Attribute_Mind, 10);
	Attr.Add(OSFTags::Attribute_Endurance, 10);
	Attr.Add(OSFTags::Attribute_Strength, 12);
	Attr.Add(OSFTags::Attribute_Dexterity, 12);
	Attr.Add(OSFTags::Attribute_Intelligence, 9);
	Attr.Add(OSFTags::Attribute_Faith, 9);
	Attr.Add(OSFTags::Attribute_Arcane, 8);
	return Attr;
}

void UOSFStatComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeStats();
}

void UOSFStatComponent::InitializeStats()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;
	for (const FOSFStatDefinition& Def : StatDefinitions)
	{
		FOSFStatValue& Value = Stats.FindOrAdd(Def.Tag);
		Value.Max = ComputeMax(Def);
		Value.Current = Value.Max;
	}
	OnStatsRecalculated.Broadcast(this);
}

double UOSFStatComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

const FOSFStatDefinition* UOSFStatComponent::FindDefinition(FGameplayTag Stat) const
{
	return StatDefinitions.FindByPredicate([Stat](const FOSFStatDefinition& Def) { return Def.Tag == Stat; });
}

float UOSFStatComponent::ComputeMax(const FOSFStatDefinition& Def) const
{
	float Value = Def.BaseMax;
	for (const FOSFAttributeScaling& Scaling : Def.AttributeScaling)
	{
		const float AttrValue = static_cast<float>(GetAttribute(Scaling.Attribute));
		Value += Scaling.Curve ? Scaling.Curve->GetFloatValue(AttrValue) : AttrValue * Scaling.PerPoint;
	}
	Value += GetModifierAdditive(Def.Tag);
	Value *= GetModifierMultiplier(Def.Tag);
	return FMath::Max(0.f, Value);
}

void UOSFStatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialized)
	{
		InitializeStats();
	}

	const double Now = GetWorldTime();
	for (const FOSFStatDefinition& Def : StatDefinitions)
	{
		if (Def.RegenPerSecond <= 0.f)
		{
			continue;
		}
		FOSFStatValue* Value = Stats.Find(Def.Tag);
		if (!Value || Value->Current >= Value->Max || Now < Value->RegenBlockedUntil)
		{
			continue;
		}
		const float* Multiplier = RegenMultipliers.Find(Def.Tag);
		const float Rate = Def.RegenPerSecond * (Multiplier ? *Multiplier : 1.f);
		if (Rate > 0.f)
		{
			ModifyCurrent(Def.Tag, Rate * DeltaTime, false);
		}
	}
}

float UOSFStatComponent::GetCurrent(FGameplayTag Stat) const
{
	const FOSFStatValue* Value = Stats.Find(Stat);
	return Value ? Value->Current : 0.f;
}

float UOSFStatComponent::GetMax(FGameplayTag Stat) const
{
	const FOSFStatValue* Value = Stats.Find(Stat);
	return Value ? Value->Max : 0.f;
}

float UOSFStatComponent::GetPercent(FGameplayTag Stat) const
{
	const FOSFStatValue* Value = Stats.Find(Stat);
	return (Value && Value->Max > 0.f) ? Value->Current / Value->Max : 0.f;
}

float UOSFStatComponent::ModifyCurrent(FGameplayTag Stat, float Delta, bool bBlockRegen)
{
	if (!bInitialized)
	{
		InitializeStats();
	}
	FOSFStatValue* Value = Stats.Find(Stat);
	if (!Value)
	{
		return 0.f;
	}

	const float Old = Value->Current;
	Value->Current = FMath::Clamp(Value->Current + Delta, 0.f, Value->Max);
	const float Applied = Value->Current - Old;

	if (bBlockRegen && Delta < 0.f)
	{
		if (const FOSFStatDefinition* Def = FindDefinition(Stat))
		{
			Value->RegenBlockedUntil = FMath::Max(Value->RegenBlockedUntil, GetWorldTime() + Def->RegenDelay);
		}
	}

	if (!FMath::IsNearlyZero(Applied))
	{
		OnStatChanged.Broadcast(this, Stat, Value->Current, Applied);
		if (Value->Current <= 0.f && Old > 0.f)
		{
			OnStatDepleted.Broadcast(this, Stat);
		}
	}
	return Applied;
}

void UOSFStatComponent::SetCurrent(FGameplayTag Stat, float NewValue)
{
	ModifyCurrent(Stat, NewValue - GetCurrent(Stat));
}

bool UOSFStatComponent::TryConsume(FGameplayTag Stat, float Amount, bool bRequireFullAmount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	const float Current = GetCurrent(Stat);
	if (Current <= 0.f || (bRequireFullAmount && Current < Amount))
	{
		return false;
	}
	ModifyCurrent(Stat, -Amount, true);
	return true;
}

void UOSFStatComponent::RestoreAll()
{
	for (TPair<FGameplayTag, FOSFStatValue>& Pair : Stats)
	{
		ModifyCurrent(Pair.Key, Pair.Value.Max - Pair.Value.Current);
		Pair.Value.RegenBlockedUntil = 0.0;
	}
}

void UOSFStatComponent::BlockRegen(FGameplayTag Stat, float Seconds)
{
	if (FOSFStatValue* Value = Stats.Find(Stat))
	{
		Value->RegenBlockedUntil = FMath::Max(Value->RegenBlockedUntil, GetWorldTime() + Seconds);
	}
}

int32 UOSFStatComponent::GetBaseAttribute(FGameplayTag Attribute) const
{
	const int32* Value = Attributes.Find(Attribute);
	return Value ? *Value : 0;
}

int32 UOSFStatComponent::GetAttribute(FGameplayTag Attribute) const
{
	const float Value = (GetBaseAttribute(Attribute) + GetModifierAdditive(Attribute)) * GetModifierMultiplier(Attribute);
	return FMath::Max(0, FMath::RoundToInt(Value));
}

void UOSFStatComponent::SetBaseAttribute(FGameplayTag Attribute, int32 Value)
{
	const int32 OldLevel = GetLevel();
	Attributes.Add(Attribute, FMath::Clamp(Value, 1, 99));
	RecalculateStats();
	if (GetLevel() != OldLevel)
	{
		OnLevelChanged.Broadcast(this, GetLevel());
	}
}

int32 UOSFStatComponent::GetLevel() const
{
	int32 Sum = 0;
	for (const TPair<FGameplayTag, int32>& Pair : Attributes)
	{
		Sum += Pair.Value;
	}
	return FMath::Max(1, Sum - LevelOffset);
}

int32 UOSFStatComponent::GetLevelUpCost_Implementation(int32 FromLevel) const
{
	// Elden-Ring-like curve, softened for the first levels.
	const float X = static_cast<float>(FromLevel + 1);
	const float Cost = 0.02f * X * X * X + 3.06f * X * X + 105.6f * X - 895.f;
	return FMath::Max(250, FMath::RoundToInt(Cost));
}

int32 UOSFStatComponent::GetCostForPoints(int32 Points) const
{
	int32 Total = 0;
	const int32 Level = GetLevel();
	for (int32 Index = 0; Index < Points; ++Index)
	{
		Total += GetLevelUpCost(Level + Index);
	}
	return Total;
}

bool UOSFStatComponent::LevelUp(const TMap<FGameplayTag, int32>& AttributeIncrements)
{
	int32 Points = 0;
	for (const TPair<FGameplayTag, int32>& Pair : AttributeIncrements)
	{
		if (Pair.Value < 0)
		{
			return false;
		}
		Points += Pair.Value;
	}
	if (Points == 0)
	{
		return false;
	}

	const int32 Cost = GetCostForPoints(Points);
	UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(GetOwner());
	if (!Inventory || !Inventory->SpendCurrency(Cost))
	{
		return false;
	}

	for (const TPair<FGameplayTag, int32>& Pair : AttributeIncrements)
	{
		Attributes.FindOrAdd(Pair.Key) += Pair.Value;
	}
	RecalculateStats();
	OnLevelChanged.Broadcast(this, GetLevel());
	return true;
}

void UOSFStatComponent::SetModifiers(UObject* Source, FName Key, const TArray<FOSFStatModifier>& Modifiers)
{
	ModifierSources.RemoveAll([Source, Key](const FOSFModifierSource& Entry)
	{
		return !Entry.Source.IsValid() || (Entry.Source.Get() == Source && Entry.Key == Key);
	});
	if (Modifiers.Num() > 0)
	{
		FOSFModifierSource& Entry = ModifierSources.AddDefaulted_GetRef();
		Entry.Source = Source;
		Entry.Key = Key;
		Entry.Modifiers = Modifiers;
	}
	RecalculateStats();
}

void UOSFStatComponent::RemoveModifiers(UObject* Source, FName Key)
{
	SetModifiers(Source, Key, {});
}

float UOSFStatComponent::GetModifierAdditive(FGameplayTag Tag) const
{
	float Sum = 0.f;
	for (const FOSFModifierSource& Entry : ModifierSources)
	{
		for (const FOSFStatModifier& Mod : Entry.Modifiers)
		{
			if (Mod.Tag == Tag)
			{
				Sum += Mod.Additive;
			}
		}
	}
	return Sum;
}

float UOSFStatComponent::GetModifierMultiplier(FGameplayTag Tag) const
{
	float Product = 1.f;
	for (const FOSFModifierSource& Entry : ModifierSources)
	{
		for (const FOSFStatModifier& Mod : Entry.Modifiers)
		{
			if (Mod.Tag == Tag)
			{
				Product *= Mod.Multiplier;
			}
		}
	}
	return Product;
}

void UOSFStatComponent::RecalculateStats()
{
	if (!bInitialized)
	{
		InitializeStats();
		return;
	}
	for (const FOSFStatDefinition& Def : StatDefinitions)
	{
		FOSFStatValue& Value = Stats.FindOrAdd(Def.Tag);
		const float NewMax = ComputeMax(Def);
		const float Delta = NewMax - Value.Max;
		Value.Max = NewMax;
		// Growing the max also grows the current value so "full" stays full; shrinking clamps.
		Value.Current = FMath::Clamp(Value.Current + FMath::Max(0.f, Delta), 0.f, Value.Max);
	}
	OnStatsRecalculated.Broadcast(this);
}
