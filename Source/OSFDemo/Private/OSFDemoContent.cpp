// Copyright Open Soulslike Framework contributors. MIT License.

#include "OSFDemoContent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFItemRegistry.h"
#include "Interaction/OSFNPC.h"
#include "Stats/OSFBuffComponent.h"
#include "OSFGameplayTags.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "OSFDemo"

UOSFDemoContent* UOSFDemoContent::Instance = nullptr;

UOSFDemoContent* UOSFDemoContent::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UOSFDemoContent>(GetTransientPackage(), TEXT("OSFDemoContent"));
		Instance->AddToRoot();
		Instance->Build();
	}
	return Instance;
}

namespace
{
	template <typename T>
	T* MakeItem(UObject* Outer, const TCHAR* Id, const FText& Name, const FText& Description, const FLinearColor& Tint)
	{
		T* Item = NewObject<T>(Outer, Id);
		Item->ItemId = Id;
		Item->DisplayName = Name;
		Item->Description = Description;
		Item->Tint = Tint;
		UOSFItemRegistry::Get()->RegisterItem(Item);
		return Item;
	}

	FOSFItemEffect Effect(EOSFItemEffectType Type, FGameplayTag Tag, float Magnitude, bool bPercent = false, UOSFBuffData* Buff = nullptr)
	{
		FOSFItemEffect Result;
		Result.Type = Type;
		Result.Tag = Tag;
		Result.Magnitude = Magnitude;
		Result.bPercent = bPercent;
		Result.Buff = Buff;
		return Result;
	}

	UStaticMesh* CubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	void SetBlade(UOSFWeaponData* Weapon, float Length, float Thickness)
	{
		Weapon->StaticMesh = CubeMesh();
		Weapon->BladeLength = Length;
		Weapon->MeshTransform = FTransform(FRotator::ZeroRotator, FVector(Length * 0.5f, 0.f, 0.f), FVector(Length / 100.f, Thickness, Thickness * 0.5f));
	}
}

void UOSFDemoContent::Build()
{
	using namespace OSFTags;

	// --- Buffs ---
	GreaseBuff = NewObject<UOSFBuffData>(this, TEXT("GreaseBuff"));
	GreaseBuff->BuffTag = FGameplayTag::RequestGameplayTag(TEXT("OSF.Damage.Fire"));
	GreaseBuff->DisplayName = LOCTEXT("GreaseBuff", "Fire Grease");
	GreaseBuff->Duration = 45.f;
	GreaseBuff->AttackPowerMultiplier = 1.2f;
	GreaseBuff->Color = FLinearColor(1.f, 0.45f, 0.1f);

	BossRage = NewObject<UOSFBuffData>(this, TEXT("BossRage"));
	BossRage->DisplayName = LOCTEXT("BossRage", "Wrath");
	BossRage->Duration = -1.f;
	BossRage->AttackPowerMultiplier = 1.3f;
	BossRage->DamageTakenMultiplier = 0.9f;

	// --- Consumables ---
	Flask = MakeItem<UOSFItemData>(this, TEXT("FlaskOfCrimsonTears"), LOCTEXT("Flask", "Flask of Crimson Tears"),
		LOCTEXT("FlaskDesc", "Restores HP. Refilled when resting."), FLinearColor(0.9f, 0.15f, 0.1f));
	Flask->bIsFlask = true;
	Flask->MaxStack = 14;
	Flask->SellPrice = 0;
	Flask->UseEffects.Add(Effect(EOSFItemEffectType::RestoreStat, Stat_Health, 0.45f, true));

	FPFlask = MakeItem<UOSFItemData>(this, TEXT("FlaskOfCeruleanTears"), LOCTEXT("FPFlask", "Flask of Cerulean Tears"),
		LOCTEXT("FPFlaskDesc", "Restores FP. Refilled when resting."), FLinearColor(0.15f, 0.35f, 0.95f));
	FPFlask->bIsFlask = true;
	FPFlask->MaxStack = 14;
	FPFlask->SellPrice = 0;
	FPFlask->UseEffects.Add(Effect(EOSFItemEffectType::RestoreStat, Stat_FP, 0.6f, true));

	RuneShard = MakeItem<UOSFItemData>(this, TEXT("RuneShard"), LOCTEXT("RuneShard", "Golden Rune Shard"),
		LOCTEXT("RuneShardDesc", "Crush to obtain 300 runes."), FLinearColor(1.f, 0.8f, 0.3f));
	RuneShard->UseEffects.Add(Effect(EOSFItemEffectType::GrantCurrency, FGameplayTag(), 300.f));
	RuneShard->SellPrice = 50;

	Cure = MakeItem<UOSFItemData>(this, TEXT("CleansingBoluses"), LOCTEXT("Cure", "Cleansing Bolus"),
		LOCTEXT("CureDesc", "Cures poison, rot and bleed buildup."), FLinearColor(0.5f, 0.85f, 0.4f));
	Cure->UseEffects.Add(Effect(EOSFItemEffectType::CureStatus, FGameplayTag(), 0.f));
	Cure->SellPrice = 30;

	Herb = MakeItem<UOSFItemData>(this, TEXT("Herb"), LOCTEXT("Herb", "Herba"), LOCTEXT("HerbDesc", "Crafting material."), FLinearColor(0.3f, 0.7f, 0.3f));
	Herb->Category = EOSFItemCategory::Material;
	Herb->SellPrice = 5;

	Bone = MakeItem<UOSFItemData>(this, TEXT("Bone"), LOCTEXT("Bone", "Thin Beast Bones"), LOCTEXT("BoneDesc", "Crafting material."), FLinearColor(0.9f, 0.9f, 0.8f));
	Bone->Category = EOSFItemCategory::Material;
	Bone->SellPrice = 5;

	Grease = MakeItem<UOSFItemData>(this, TEXT("FireGrease"), LOCTEXT("Grease", "Fire Grease"), LOCTEXT("GreaseDesc", "Increases attack power for 45 seconds."), FLinearColor(1.f, 0.4f, 0.1f));
	Grease->UseEffects.Add(Effect(EOSFItemEffectType::ApplyBuff, FGameplayTag(), 0.f, false, GreaseBuff));
	Grease->SellPrice = 40;

	GateKey = MakeItem<UOSFItemData>(this, TEXT("GateKey"), LOCTEXT("Key", "Rusted Gate Key"), LOCTEXT("KeyDesc", "Opens the gate of the old courtyard."), FLinearColor(0.7f, 0.6f, 0.4f));
	GateKey->Category = EOSFItemCategory::KeyItem;
	GateKey->MaxStack = 1;
	GateKey->SellPrice = 0;

	// --- Weapons ---
	Longsword = MakeItem<UOSFWeaponData>(this, TEXT("Longsword"), LOCTEXT("Longsword", "Longsword"),
		LOCTEXT("LongswordDesc", "A reliable straight sword. Balanced reach and speed."), FLinearColor(0.75f, 0.75f, 0.8f));
	SetBlade(Longsword, 130.f, 0.08f);
	Longsword->SkillName = LOCTEXT("Spin", "Spinning Slash");
	Longsword->SellPrice = 200;

	Greatsword = MakeItem<UOSFWeaponData>(this, TEXT("Greatsword"), LOCTEXT("Greatsword", "Greatsword"),
		LOCTEXT("GreatswordDesc", "Heavy colossal blade. Slow, devastating, breaks stances."), FLinearColor(0.5f, 0.5f, 0.55f));
	SetBlade(Greatsword, 190.f, 0.16f);
	Greatsword->BaseDamage.Add(Damage_Physical, 165.f);
	Greatsword->Scaling.Reset();
	Greatsword->Scaling.Add(Attribute_Strength, 1.2f);
	Greatsword->Scaling.Add(Attribute_Dexterity, 0.3f);
	Greatsword->Requirements.Add(Attribute_Strength, 16);
	Greatsword->PoiseDamage = 26.f;
	Greatsword->Weight = 12.f;
	Greatsword->BuildDefaultMoveset(0.72f);
	for (FOSFAttackDefinition& Attack : Greatsword->LightCombo) { Attack.Reach = 280.f; Attack.StaminaCost = 24.f; Attack.TraceRadius = 60.f; }
	for (FOSFAttackDefinition& Attack : Greatsword->HeavyCombo) { Attack.Reach = 290.f; Attack.StaminaCost = 32.f; Attack.bHyperArmor = true; }
	Greatsword->SkillName = LOCTEXT("Stomp", "Lion's Claw");
	Greatsword->SellPrice = 500;

	BleedDagger = MakeItem<UOSFWeaponData>(this, TEXT("BloodDagger"), LOCTEXT("Dagger", "Bloodfang Dagger"),
		LOCTEXT("DaggerDesc", "Fast serrated dagger. Causes blood loss. Excellent critical hits."), FLinearColor(0.6f, 0.1f, 0.1f));
	SetBlade(BleedDagger, 70.f, 0.06f);
	BleedDagger->BaseDamage.Add(Damage_Physical, 70.f);
	BleedDagger->Scaling.Reset();
	BleedDagger->Scaling.Add(Attribute_Dexterity, 1.0f);
	BleedDagger->Scaling.Add(Attribute_Arcane, 0.6f);
	BleedDagger->StatusBuildup.Add(Status_Bleed, 38.f);
	BleedDagger->CriticalMultiplier = 4.5f;
	BleedDagger->PoiseDamage = 7.f;
	BleedDagger->Weight = 1.5f;
	BleedDagger->BuildDefaultMoveset(1.45f);
	for (FOSFAttackDefinition& Attack : BleedDagger->LightCombo) { Attack.Reach = 160.f; Attack.StaminaCost = 9.f; }
	BleedDagger->SkillName = LOCTEXT("Quickstep", "Quickstep Flurry");
	BleedDagger->SellPrice = 300;

	Shield = MakeItem<UOSFWeaponData>(this, TEXT("HeaterShield"), LOCTEXT("Shield", "Heater Shield"),
		LOCTEXT("ShieldDesc", "Medium shield. Skill: Parry."), FLinearColor(0.45f, 0.35f, 0.2f));
	Shield->Category = EOSFItemCategory::Shield;
	Shield->StaticMesh = CubeMesh();
	Shield->MeshTransform = FTransform(FRotator::ZeroRotator, FVector(8.f, 0.f, 0.f), FVector(0.08f, 0.6f, 0.8f));
	Shield->BladeLength = 20.f;
	Shield->GuardAbsorption.Add(Damage_Physical, 1.0f);
	Shield->GuardAbsorption.Add(Damage_Fire, 0.6f);
	Shield->GuardAbsorption.Add(Damage_Magic, 0.6f);
	Shield->GuardBoost = 0.55f;
	Shield->bCanParry = true;
	Shield->Weight = 4.f;
	Shield->SellPrice = 150;

	EnemySword = MakeItem<UOSFWeaponData>(this, TEXT("RustySword"), LOCTEXT("RustySword", "Rusty Sword"), LOCTEXT("RustyDesc", "A soldier's battered sword."), FLinearColor(0.5f, 0.35f, 0.25f));
	SetBlade(EnemySword, 115.f, 0.08f);
	EnemySword->BaseDamage.Add(Damage_Physical, 70.f);
	EnemySword->SellPrice = 60;

	BossHalberd = MakeItem<UOSFWeaponData>(this, TEXT("WardenHalberd"), LOCTEXT("Halberd", "Warden's Halberd"),
		LOCTEXT("HalberdDesc", "Halberd of the Ashen Gate's warden."), FLinearColor(0.3f, 0.3f, 0.35f));
	SetBlade(BossHalberd, 240.f, 0.12f);
	BossHalberd->BaseDamage.Add(Damage_Physical, 120.f);
	BossHalberd->BaseDamage.Add(Damage_Fire, 40.f);
	BossHalberd->PoiseDamage = 30.f;
	BossHalberd->SellPrice = 2000;

	// --- Armor ---
	auto MakeArmor = [this](const TCHAR* Id, const FText& Name, EOSFEquipSlot Slot, float Physical, float Poise, float Weight)
	{
		UOSFArmorData* Armor = MakeItem<UOSFArmorData>(this, Id, Name, LOCTEXT("ArmorDesc", "Knight's armor of the old courtyard."), FLinearColor(0.55f, 0.55f, 0.6f));
		Armor->Slot = Slot;
		Armor->Negation.Add(OSFTags::Damage_Physical, Physical);
		Armor->Negation.Add(OSFTags::Damage_Fire, Physical * 0.6f);
		Armor->Poise = Poise;
		Armor->Weight = Weight;
		Armor->SellPrice = 120;
		return Armor;
	};
	Helm = MakeArmor(TEXT("KnightHelm"), LOCTEXT("Helm", "Knight Helm"), EOSFEquipSlot::Head, 5.f, 5.f, 3.f);
	Chestplate = MakeArmor(TEXT("KnightArmor"), LOCTEXT("Chest", "Knight Armor"), EOSFEquipSlot::Chest, 12.f, 14.f, 8.f);
	Gauntlets = MakeArmor(TEXT("KnightGauntlets"), LOCTEXT("Gauntlets", "Knight Gauntlets"), EOSFEquipSlot::Arms, 4.f, 4.f, 2.5f);
	Greaves = MakeArmor(TEXT("KnightGreaves"), LOCTEXT("Greaves", "Knight Greaves"), EOSFEquipSlot::Legs, 6.f, 7.f, 4.f);

	// --- Talismans ---
	VigorTalisman = MakeItem<UOSFTalismanData>(this, TEXT("CrimsonAmber"), LOCTEXT("Amber", "Crimson Amber Medallion"),
		LOCTEXT("AmberDesc", "Raises maximum HP."), FLinearColor(0.9f, 0.2f, 0.2f));
	VigorTalisman->Modifiers.Add({ Stat_Health, 0.f, 1.06f });
	VigorTalisman->SellPrice = 400;

	StaminaTalisman = MakeItem<UOSFTalismanData>(this, TEXT("GreenTurtle"), LOCTEXT("Turtle", "Green Turtle Talisman"),
		LOCTEXT("TurtleDesc", "Raises maximum stamina and status resistance."), FLinearColor(0.3f, 0.8f, 0.4f));
	StaminaTalisman->Modifiers.Add({ Stat_Stamina, 25.f, 1.f });
	StaminaTalisman->Modifiers.Add({ Status_Bleed, 60.f, 1.f });
	StaminaTalisman->Modifiers.Add({ Status_Poison, 60.f, 1.f });
	StaminaTalisman->SellPrice = 400;

	// --- Crafting ---
	CureRecipe = NewObject<UOSFCraftingRecipe>(this, TEXT("Recipe_Cure"));
	CureRecipe->Ingredients.Add({ Herb, 2 });
	CureRecipe->Result = { Cure, 1 };
	GreaseRecipe = NewObject<UOSFCraftingRecipe>(this, TEXT("Recipe_Grease"));
	GreaseRecipe->Ingredients.Add({ Bone, 1 });
	GreaseRecipe->Ingredients.Add({ Herb, 1 });
	GreaseRecipe->Result = { Grease, 1 };

	// --- Merchant ---
	MerchantDialog = NewObject<UOSFDialogData>(this, TEXT("MerchantDialog"));
	{
		FOSFDialogEntry AfterBoss;
		AfterBoss.RequiredFlags.Add(TEXT("Boss.AshenWarden"));
		AfterBoss.Lines.Add(LOCTEXT("M_After1", "You... felled the Warden? Then the gate is open at last."));
		AfterBoss.Lines.Add(LOCTEXT("M_After2", "Take my finest wares. You've earned them, Tarnished."));
		AfterBoss.GrantFlags.Add(TEXT("Merchant.PraisedBoss"));
		MerchantDialog->Entries.Add(AfterBoss);

		FOSFDialogEntry Second;
		Second.RequiredFlags.Add(TEXT("Merchant.Met"));
		Second.Lines.Add(LOCTEXT("M_Again", "Back again? The Warden still guards the gate beyond the mist."));
		MerchantDialog->Entries.Add(Second);

		FOSFDialogEntry First;
		First.Lines.Add(LOCTEXT("M_First1", "Ah, a traveller. Not many come by these ruins any more."));
		First.Lines.Add(LOCTEXT("M_First2", "Rest at the flame, then deal with the soldiers. The chest by the wall holds a key."));
		First.Lines.Add(LOCTEXT("M_First3", "Here - take these. And if you have runes, I have goods."));
		First.GrantFlags.Add(TEXT("Merchant.Met"));
		First.GrantItems.Add({ Herb, 3 });
		MerchantDialog->Entries.Add(First);
	}
	MerchantShop = NewObject<UOSFVendorData>(this, TEXT("MerchantShop"));
	MerchantShop->Entries.Add({ Cure, 120, 5, {} });
	MerchantShop->Entries.Add({ Grease, 200, -1, {} });
	MerchantShop->Entries.Add({ BleedDagger, 1500, 1, {} });
	MerchantShop->Entries.Add({ Helm, 600, 1, {} });
	MerchantShop->Entries.Add({ StaminaTalisman, 1800, 1, {} });
	MerchantShop->SellMultiplier = 1.f;
}

#undef LOCTEXT_NAMESPACE
