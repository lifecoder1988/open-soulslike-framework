// Copyright Open Soulslike Framework contributors. MIT License.

#include "OSFGameplayTags.h"

namespace OSFTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Health, "OSF.Stat.Health", "Hit points");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_FP, "OSF.Stat.FP", "Focus points used by weapon skills and spells");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Stamina, "OSF.Stat.Stamina", "Consumed by attacks, dodges, guarding and sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Poise, "OSF.Stat.Poise", "Hidden stagger meter");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_EquipLoad, "OSF.Stat.EquipLoad", "Maximum equip load");

	UE_DEFINE_GAMEPLAY_TAG(Attribute_Vigor, "OSF.Attribute.Vigor");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Mind, "OSF.Attribute.Mind");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Endurance, "OSF.Attribute.Endurance");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Strength, "OSF.Attribute.Strength");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Dexterity, "OSF.Attribute.Dexterity");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Intelligence, "OSF.Attribute.Intelligence");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Faith, "OSF.Attribute.Faith");
	UE_DEFINE_GAMEPLAY_TAG(Attribute_Arcane, "OSF.Attribute.Arcane");

	UE_DEFINE_GAMEPLAY_TAG(Action_LightAttack, "OSF.Action.LightAttack");
	UE_DEFINE_GAMEPLAY_TAG(Action_HeavyAttack, "OSF.Action.HeavyAttack");
	UE_DEFINE_GAMEPLAY_TAG(Action_Dodge, "OSF.Action.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Action_Guard, "OSF.Action.Guard");
	UE_DEFINE_GAMEPLAY_TAG(Action_WeaponSkill, "OSF.Action.WeaponSkill");
	UE_DEFINE_GAMEPLAY_TAG(Action_Parry, "OSF.Action.Parry");
	UE_DEFINE_GAMEPLAY_TAG(Action_UseItem, "OSF.Action.UseItem");
	UE_DEFINE_GAMEPLAY_TAG(Action_Execute, "OSF.Action.Execute");
	UE_DEFINE_GAMEPLAY_TAG(Action_AIAbility, "OSF.Action.AIAbility");
	UE_DEFINE_GAMEPLAY_TAG(Action_Shoot, "OSF.Action.Shoot");
	UE_DEFINE_GAMEPLAY_TAG(Action_Reload, "OSF.Action.Reload");

	UE_DEFINE_GAMEPLAY_TAG(State_Busy, "OSF.State.Busy");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "OSF.State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "OSF.State.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Guarding, "OSF.State.Guarding");
	UE_DEFINE_GAMEPLAY_TAG(State_Parrying, "OSF.State.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_Staggered, "OSF.State.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_PoiseBroken, "OSF.State.PoiseBroken");
	UE_DEFINE_GAMEPLAY_TAG(State_Invincible, "OSF.State.Invincible");
	UE_DEFINE_GAMEPLAY_TAG(State_HyperArmor, "OSF.State.HyperArmor");
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "OSF.State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "OSF.State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_UsingItem, "OSF.State.UsingItem");
	UE_DEFINE_GAMEPLAY_TAG(State_Executing, "OSF.State.Executing");
	UE_DEFINE_GAMEPLAY_TAG(State_BeingExecuted, "OSF.State.BeingExecuted");
	UE_DEFINE_GAMEPLAY_TAG(State_OnLadder, "OSF.State.OnLadder");
	UE_DEFINE_GAMEPLAY_TAG(State_InMenu, "OSF.State.InMenu");
	UE_DEFINE_GAMEPLAY_TAG(State_ComboWindow, "OSF.State.ComboWindow");
	UE_DEFINE_GAMEPLAY_TAG(State_TwoHanding, "OSF.State.TwoHanding");
	UE_DEFINE_GAMEPLAY_TAG(State_Resting, "OSF.State.Resting");
	UE_DEFINE_GAMEPLAY_TAG(State_Perilous, "OSF.State.Perilous");
	UE_DEFINE_GAMEPLAY_TAG(State_Shooting, "OSF.State.Shooting");
	UE_DEFINE_GAMEPLAY_TAG(State_Reloading, "OSF.State.Reloading");

	UE_DEFINE_GAMEPLAY_TAG(Source_Melee, "OSF.Source.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Source_Projectile, "OSF.Source.Projectile");
	UE_DEFINE_GAMEPLAY_TAG(Source_Explosion, "OSF.Source.Explosion");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Physical, "OSF.Damage.Physical");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Magic, "OSF.Damage.Magic");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Fire, "OSF.Damage.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Lightning, "OSF.Damage.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Holy, "OSF.Damage.Holy");

	UE_DEFINE_GAMEPLAY_TAG(Status_Bleed, "OSF.Status.Bleed");
	UE_DEFINE_GAMEPLAY_TAG(Status_Poison, "OSF.Status.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Status_Rot, "OSF.Status.Rot");
	UE_DEFINE_GAMEPLAY_TAG(Status_Frostbite, "OSF.Status.Frostbite");

	UE_DEFINE_GAMEPLAY_TAG(Progress, "OSF.Progress");
	UE_DEFINE_GAMEPLAY_TAG(Progress_Boss, "OSF.Progress.Boss");
	UE_DEFINE_GAMEPLAY_TAG(Progress_RestingPoint, "OSF.Progress.RestingPoint");
	UE_DEFINE_GAMEPLAY_TAG(Progress_NPC, "OSF.Progress.NPC");
}
