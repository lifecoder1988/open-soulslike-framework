// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFItemData.h"
#include "OSFGameplayTags.h"

bool UOSFItemData::IsEquippable() const
{
	switch (Category)
	{
	case EOSFItemCategory::Weapon:
	case EOSFItemCategory::Shield:
	case EOSFItemCategory::Armor:
	case EOSFItemCategory::Talisman:
		return true;
	case EOSFItemCategory::Consumable:
		return IsUsable();
	default:
		return false;
	}
}

FPrimaryAssetId UOSFItemData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("OSFItem"), ItemId.IsNone() ? GetFName() : ItemId);
}

UOSFWeaponData::UOSFWeaponData()
{
	Category = EOSFItemCategory::Weapon;
	MaxStack = 1;
	Weight = 4.f;
	BaseDamage.Add(OSFTags::Damage_Physical, 110.f);
	Scaling.Add(OSFTags::Attribute_Strength, 0.7f);
	Scaling.Add(OSFTags::Attribute_Dexterity, 0.7f);
	GuardAbsorption.Add(OSFTags::Damage_Physical, 0.5f);
	BuildDefaultMoveset();
}

void UOSFWeaponData::BuildDefaultMoveset(float Speed)
{
	const float S = FMath::Max(0.1f, Speed);
	auto Make = [S](float Windup, float Active, float Recovery, float Motion, float Stamina, float Yaw, float Lunge)
	{
		FOSFAttackDefinition Def;
		Def.Timing.Windup = Windup / S;
		Def.Timing.Active = Active / S;
		Def.Timing.Recovery = Recovery / S;
		Def.MotionValue = Motion;
		Def.StaminaCost = Stamina;
		Def.SwingYaw = Yaw;
		Def.Lunge = Lunge;
		return Def;
	};

	LightCombo.Reset();
	LightCombo.Add(Make(0.30f, 0.18f, 0.45f, 1.0f, 14.f, -110.f, 200.f));
	LightCombo.Add(Make(0.26f, 0.18f, 0.45f, 1.05f, 14.f, 110.f, 200.f));
	LightCombo.Add(Make(0.40f, 0.22f, 0.60f, 1.3f, 18.f, -160.f, 300.f));

	HeavyCombo.Reset();
	FOSFAttackDefinition Heavy1 = Make(0.65f, 0.22f, 0.70f, 1.8f, 26.f, 0.f, 250.f);
	Heavy1.SwingPitch = -120.f;
	Heavy1.PoiseDamageMultiplier = 2.f;
	FOSFAttackDefinition Heavy2 = Heavy1;
	Heavy2.MotionValue = 2.0f;
	HeavyCombo.Add(Heavy1);
	HeavyCombo.Add(Heavy2);

	RunningAttack = Make(0.35f, 0.22f, 0.55f, 1.3f, 20.f, 0.f, 450.f);
	RunningAttack.SwingPitch = -100.f;

	JumpingAttack = Make(0.30f, 0.25f, 0.55f, 1.6f, 22.f, 0.f, 150.f);
	JumpingAttack.SwingPitch = -140.f;
	JumpingAttack.PoiseDamageMultiplier = 1.8f;

	SkillAttack = Make(0.55f, 0.35f, 0.70f, 2.2f, 20.f, -330.f, 150.f);
	SkillAttack.FPCost = 12.f;
	SkillAttack.bHyperArmor = true;
	SkillAttack.Reach = 300.f;
	SkillAttack.TraceRadius = 90.f;
	SkillAttack.PoiseDamageMultiplier = 2.5f;
}

const FOSFAttackDefinition* UOSFWeaponData::GetAttack(EOSFAttackType Type, int32 ComboIndex) const
{
	switch (Type)
	{
	case EOSFAttackType::Light:
		return LightCombo.Num() > 0 ? &LightCombo[ComboIndex % LightCombo.Num()] : nullptr;
	case EOSFAttackType::Heavy:
		return HeavyCombo.Num() > 0 ? &HeavyCombo[ComboIndex % HeavyCombo.Num()] : nullptr;
	case EOSFAttackType::Running:
		return &RunningAttack;
	case EOSFAttackType::Jumping:
		return &JumpingAttack;
	case EOSFAttackType::Skill:
		return &SkillAttack;
	}
	return nullptr;
}

int32 UOSFWeaponData::GetComboLength(EOSFAttackType Type) const
{
	switch (Type)
	{
	case EOSFAttackType::Light: return LightCombo.Num();
	case EOSFAttackType::Heavy: return HeavyCombo.Num();
	default: return 1;
	}
}

UOSFArmorData::UOSFArmorData()
{
	Category = EOSFItemCategory::Armor;
	MaxStack = 1;
	Weight = 5.f;
	Negation.Add(OSFTags::Damage_Physical, 8.f);
}

UOSFTalismanData::UOSFTalismanData()
{
	Category = EOSFItemCategory::Talisman;
	MaxStack = 1;
	Weight = 0.5f;
}

UOSFAmmoData::UOSFAmmoData()
{
	Category = EOSFItemCategory::Ammo;
	MaxStack = 999;
	SellPrice = 2;
	Damage.Add(OSFTags::Damage_Physical, 40.f);
	SourceTags.AddTag(OSFTags::Source_Projectile);
}
