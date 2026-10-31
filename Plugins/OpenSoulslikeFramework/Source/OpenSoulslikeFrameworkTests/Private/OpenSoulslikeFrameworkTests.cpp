// Copyright Open Soulslike Framework contributors. MIT License.

#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/StaticMeshActor.h"
#include "RenderingThread.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#include "OSFGameplayTags.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "AI/OSFAICombatComponent.h"
#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFBossComponent.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFCombatLibrary.h"
#include "Interaction/OSFNPC.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFItemRegistry.h"
#include "Progression/OSFProgressComponent.h"
#include "Save/OSFSaveGame.h"
#include "Save/OSFSaveSubsystem.h"
#include "Stats/OSFBuffComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFStatusEffectComponent.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, OpenSoulslikeFrameworkTests)

#if WITH_DEV_AUTOMATION_TESTS

namespace OSFTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** Minimal game world: no game mode, no map - just actors that tick. */
	struct FWorld
	{
		UWorld* World = nullptr;

		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("OSFTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			// Floor so AI-controlled characters don't fall forever.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, Params);
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->SetActorScale3D(FVector(100.f, 100.f, 1.f));
		}

		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		template <typename T>
		T* Spawn(const FVector& Location, float Yaw = 0.f)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(Location, FRotator(0.f, Yaw, 0.f), Params);
		}

		void Tick(float Seconds, float Step = 1.f / 60.f)
		{
			for (float Time = 0.f; Time < Seconds; Time += Step)
			{
				// The tick manager skips functions already queued this frame: advance the frame like the engine loop does.
				++GFrameCounter;
				World->Tick(LEVELTICK_All, Step);
				if ((GFrameCounter % 30) == 0)
				{
					FlushRenderingCommands();
				}
			}
		}
	};

	FOSFDamageEvent MakeHit(AActor* Instigator, float Physical, float Poise = 10.f)
	{
		FOSFDamageEvent Event;
		Event.Instigator = Instigator;
		Event.Spec.Damage.Add(OSFTags::Damage_Physical, Physical);
		Event.Spec.PoiseDamage = Poise;
		return Event;
	}

	/** Places Attacker in front of Victim, both facing each other. */
	void FaceOff(AActor* Victim, AActor* Attacker, float Distance)
	{
		Victim->SetActorLocationAndRotation(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
		Attacker->SetActorLocationAndRotation(FVector(Distance, 0.f, 100.f), FRotator(0.f, 180.f, 0.f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFStatsTest, "OSF.Stats.AttributesLevelAndModifiers", OSFTest::Flags)
bool FOSFStatsTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	UOSFStatComponent* Stats = Player->Stats;

	TestEqual(TEXT("Starts at level 1"), Stats->GetLevel(), 1);
	TestEqual(TEXT("Vigor 10 -> 550 HP"), Stats->GetMax(OSFTags::Stat_Health), 550.f);
	TestEqual(TEXT("Endurance 10 -> 100 stamina"), Stats->GetMax(OSFTags::Stat_Stamina), 100.f);

	// Level up spends currency
	TMap<FGameplayTag, int32> Increments;
	Increments.Add(OSFTags::Attribute_Vigor, 2);
	TestFalse(TEXT("Cannot level without runes"), Stats->LevelUp(Increments));
	Player->Inventory->AddCurrency(100000);
	const int32 Cost = Stats->GetCostForPoints(2);
	TestTrue(TEXT("Level up succeeds"), Stats->LevelUp(Increments));
	TestEqual(TEXT("Level 3"), Stats->GetLevel(), 3);
	TestEqual(TEXT("Runes spent"), Player->Inventory->GetCurrency(), 100000 - Cost);
	TestEqual(TEXT("HP scaled"), Stats->GetMax(OSFTags::Stat_Health), 600.f);

	// Modifiers
	Stats->SetModifiers(Player, TEXT("Test"), { { OSFTags::Stat_Health, 100.f, 1.f }, { OSFTags::Attribute_Endurance, 5.f, 1.f } });
	TestEqual(TEXT("Additive HP modifier"), Stats->GetMax(OSFTags::Stat_Health), 700.f);
	TestEqual(TEXT("Attribute modifier feeds stamina"), Stats->GetMax(OSFTags::Stat_Stamina), 120.f);
	Stats->RemoveModifiers(Player, TEXT("Test"));
	TestEqual(TEXT("Modifiers removed"), Stats->GetMax(OSFTags::Stat_Health), 600.f);

	// Stamina regen after delay
	Stats->TryConsume(OSFTags::Stat_Stamina, 50.f);
	TestEqual(TEXT("Stamina consumed"), Stats->GetCurrent(OSFTags::Stat_Stamina), 50.f);
	TestWorld.Tick(0.5f);
	TestEqual(TEXT("No regen during delay"), Stats->GetCurrent(OSFTags::Stat_Stamina), 50.f);
	TestWorld.Tick(1.5f);
	TestTrue(TEXT("Regenerates after delay"), Stats->GetCurrent(OSFTags::Stat_Stamina) > 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFCombatResolutionTest, "OSF.Combat.HitGuardPerfectGuardIFrames", OSFTest::Flags)
bool FOSFCombatResolutionTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Victim = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Attacker = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(200.f, 0.f, 100.f), 180.f);
	OSFTest::FaceOff(Victim, Attacker, 200.f);
	TestWorld.Tick(0.1f);

	// Clean hit
	const float HP = Victim->Stats->GetCurrent(OSFTags::Stat_Health);
	FOSFHitInfo Info = Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f, 5.f));
	TestEqual(TEXT("Clean hit"), Info.Result, EOSFHitResult::Hit);
	TestEqual(TEXT("Damage applied"), Victim->Stats->GetCurrent(OSFTags::Stat_Health), HP - 100.f);

	// Same team is ignored
	Attacker->Combat->TeamId = 0;
	TestEqual(TEXT("Friendly fire ignored"), Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f)).Result, EOSFHitResult::Ignored);
	Attacker->Combat->TeamId = 1;

	// Perfect guard right after raising the guard
	TestTrue(TEXT("Guard starts"), Victim->ActionComponent->TryExecuteAction(OSFTags::Action_Guard));
	TestWorld.Tick(1.f / 60.f);
	Info = Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f));
	TestEqual(TEXT("Perfect guard"), Info.Result, EOSFHitResult::PerfectGuarded);

	// Regular guard later: chip damage + stamina damage
	TestWorld.Tick(0.4f);
	const float HPBeforeGuard = Victim->Stats->GetCurrent(OSFTags::Stat_Health);
	const float StaminaBefore = Victim->Stats->GetCurrent(OSFTags::Stat_Stamina);
	Info = Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f));
	TestEqual(TEXT("Guarded"), Info.Result, EOSFHitResult::Guarded);
	TestTrue(TEXT("Stamina damage"), Victim->Stats->GetCurrent(OSFTags::Stat_Stamina) < StaminaBefore);
	TestTrue(TEXT("Unarmed guard lets chip damage through"), Victim->Stats->GetCurrent(OSFTags::Stat_Health) < HPBeforeGuard);

	// Guard break when out of stamina
	Victim->Stats->SetCurrent(OSFTags::Stat_Stamina, 1.f);
	Info = Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f));
	TestEqual(TEXT("Guard broken"), Info.Result, EOSFHitResult::GuardBroken);
	TestTrue(TEXT("Guard break staggers"), Victim->HasStateTag(OSFTags::State_Staggered));
	TestWorld.Tick(2.f);
	TestFalse(TEXT("Stagger wears off"), Victim->HasStateTag(OSFTags::State_Staggered));

	// Guard does not cover the back
	Victim->ActionComponent->TryExecuteAction(OSFTags::Action_Guard);
	TestWorld.Tick(0.3f);
	Victim->SetActorRotation(FRotator(0.f, 180.f, 0.f));
	TestEqual(TEXT("Hit from behind ignores guard"), Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 10.f, 0.f)).Result, EOSFHitResult::Hit);
	Victim->ActionComponent->StopAction(OSFTags::Action_Guard);
	TestWorld.Tick(0.1f);

	// Dodge i-frames
	Victim->Stats->RestoreAll();
	TestTrue(TEXT("Dodge starts"), Victim->ActionComponent->TryExecuteAction(OSFTags::Action_Dodge));
	TestWorld.Tick(0.15f);
	TestTrue(TEXT("Invincible mid roll"), Victim->HasStateTag(OSFTags::State_Invincible));
	TestEqual(TEXT("I-frames ignore hits"), Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100.f)).Result, EOSFHitResult::Ignored);
	TestWorld.Tick(1.f);
	TestFalse(TEXT("Roll finished"), Victim->HasStateTag(OSFTags::State_Dodging));

	// Death
	Victim->Combat->ReceiveDamage(OSFTest::MakeHit(Attacker, 100000.f));
	TestTrue(TEXT("Dead"), Victim->IsDead());
	TestTrue(TEXT("Dead tag"), Victim->HasStateTag(OSFTags::State_Dead));
	Victim->Combat->Revive();
	TestFalse(TEXT("Revived"), Victim->IsDead());
	TestEqual(TEXT("Full HP after revive"), Victim->Stats->GetPercent(OSFTags::Stat_Health), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFParryRiposteTest, "OSF.Combat.ParryPoiseBreakRiposteBackstab", OSFTest::Flags)
bool FOSFParryRiposteTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Enemy = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(150.f, 0.f, 100.f), 180.f);
	OSFTest::FaceOff(Player, Enemy, 150.f);
	TestWorld.Tick(0.1f);

	// Give the player a parrying shield so the weapon skill becomes a parry.
	UOSFWeaponData* Shield = NewObject<UOSFWeaponData>(GetTransientPackage());
	Shield->ItemId = TEXT("TestShield");
	Shield->Category = EOSFItemCategory::Shield;
	Shield->bCanParry = true;
	Player->Equipment->EquipWeapon(Shield, false, 0);

	TestTrue(TEXT("Parry starts"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_WeaponSkill));
	TestWorld.Tick(0.15f);
	TestTrue(TEXT("Parry window open"), Player->HasStateTag(OSFTags::State_Parrying));
	FOSFHitInfo Info = Player->Combat->ReceiveDamage(OSFTest::MakeHit(Enemy, 100.f));
	TestEqual(TEXT("Parried"), Info.Result, EOSFHitResult::Parried);
	TestTrue(TEXT("Attacker open to riposte"), Enemy->Combat->CanBeRiposted());

	// Riposte: a light attack becomes an execution.
	TestWorld.Tick(0.8f);
	const float EnemyHP = Enemy->Stats->GetCurrent(OSFTags::Stat_Health);
	TestTrue(TEXT("Riposte starts"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_LightAttack));
	TestEqual(TEXT("Execute action running"), Player->ActionComponent->GetCurrentAction() ? Player->ActionComponent->GetCurrentAction()->ActionTag : FGameplayTag(), OSFTags::Action_Execute.GetTag());
	TestTrue(TEXT("Victim being executed"), Enemy->HasStateTag(OSFTags::State_BeingExecuted));
	TestWorld.Tick(0.8f);
	TestTrue(TEXT("Critical damage dealt"), Enemy->Stats->GetCurrent(OSFTags::Stat_Health) < EnemyHP);
	TestWorld.Tick(2.5f);

	// Poise break opens a riposte on enemies.
	Enemy->Combat->Revive();
	Enemy->ActionComponent->CancelCurrentAction();
	TestWorld.Tick(1.f);
	for (int32 Hit = 0; Hit < 10 && !Enemy->Combat->CanBeRiposted(); ++Hit)
	{
		Enemy->Combat->ReceiveDamage(OSFTest::MakeHit(Player, 5.f, 15.f));
	}
	TestTrue(TEXT("Poise break -> riposte window"), Enemy->Combat->CanBeRiposted());
	TestWorld.Tick(3.f);
	TestFalse(TEXT("Riposte window closes"), Enemy->Combat->CanBeRiposted());

	// Backstab: stand behind the enemy facing its back.
	Enemy->Combat->Revive();
	Enemy->SetActorLocationAndRotation(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
	Player->SetActorLocationAndRotation(FVector(-110.f, 0.f, 100.f), FRotator::ZeroRotator);
	bool bBackstab = false;
	TestEqual(TEXT("Backstab target found"), Player->Combat->FindFinisherTarget(bBackstab), static_cast<AActor*>(Enemy));
	TestTrue(TEXT("Is backstab"), bBackstab);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFComboBufferTest, "OSF.Actions.ComboChainAndInputBuffer", OSFTest::Flags)
bool FOSFComboBufferTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	UOSFWeaponData* Sword = NewObject<UOSFWeaponData>(GetTransientPackage());
	Sword->ItemId = TEXT("TestSword");
	Player->Equipment->EquipWeapon(Sword, true, 0);
	TestWorld.Tick(0.1f);

	UOSFActionComponent* Actions = Player->ActionComponent;
	TestTrue(TEXT("First attack"), Actions->TryExecuteAction(OSFTags::Action_LightAttack));
	UOSFAction_Attack* Attack = Cast<UOSFAction_Attack>(Actions->GetCurrentAction());
	TestNotNull(TEXT("Attack action"), Attack);
	TestEqual(TEXT("Combo step 0"), Attack ? Attack->GetComboIndex() : -1, 0);
	TestTrue(TEXT("Attacking tag"), Player->HasStateTag(OSFTags::State_Attacking));

	// Press again immediately: buffered, fires in the cancel window.
	TestFalse(TEXT("Second press buffered"), Actions->TryExecuteAction(OSFTags::Action_LightAttack));
	TestEqual(TEXT("Buffered"), Actions->GetBufferedAction(), OSFTags::Action_LightAttack.GetTag());
	TestWorld.Tick(0.7f);
	TestEqual(TEXT("Combo step 1 from buffer"), Attack ? Attack->GetComboIndex() : -1, 1);

	// Stamina was consumed for two attacks.
	TestTrue(TEXT("Stamina used"), Player->Stats->GetCurrent(OSFTags::Stat_Stamina) < Player->Stats->GetMax(OSFTags::Stat_Stamina) - 20.f);

	// Dodge cancels an attack at any time.
	TestTrue(TEXT("Dodge cancels attack"), Actions->TryExecuteAction(OSFTags::Action_Dodge));
	TestEqual(TEXT("Dodge running"), Actions->GetCurrentAction() ? Actions->GetCurrentAction()->ActionTag : FGameplayTag(), OSFTags::Action_Dodge.GetTag());
	TestWorld.Tick(1.5f);
	TestFalse(TEXT("Idle"), Actions->IsBusy());

	// Combo resets after a pause.
	Actions->TryExecuteAction(OSFTags::Action_LightAttack);
	TestEqual(TEXT("Combo reset"), Attack ? Attack->GetComboIndex() : -1, 0);
	TestWorld.Tick(1.5f);

	// Empty stamina blocks attacks.
	Player->Stats->SetCurrent(OSFTags::Stat_Stamina, 0.f);
	Player->Stats->BlockRegen(OSFTags::Stat_Stamina, 10.f);
	TestFalse(TEXT("No stamina, no attack"), Actions->TryExecuteAction(OSFTags::Action_LightAttack, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFWeaponTraceTest, "OSF.Combat.WeaponTraceIntegration", OSFTest::Flags)
bool FOSFWeaponTraceTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Enemy = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(150.f, 0.f, 100.f), 180.f);
	Enemy->AICombat->Abilities.Reset();
	UOSFWeaponData* Sword = NewObject<UOSFWeaponData>(GetTransientPackage());
	Sword->ItemId = TEXT("TraceSword");
	Player->Equipment->EquipWeapon(Sword, true, 0);
	TestWorld.Tick(0.1f);
	OSFTest::FaceOff(Enemy, Player, 150.f);

	const float HP = Enemy->Stats->GetCurrent(OSFTags::Stat_Health);
	TestTrue(TEXT("Attack"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_LightAttack));
	TestWorld.Tick(1.2f);
	TestTrue(TEXT("Weapon trace hit the enemy"), Enemy->Stats->GetCurrent(OSFTags::Stat_Health) < HP);

	// Expected damage: AR = 110 * (1 + 0.7*12/100 + 0.7*12/100) = 128.48, minus 0 negation.
	const float Expected = 110.f * (1.f + 0.7f * 0.12f * 2.f);
	TestEqual(TEXT("Attack rating"), UOSFCombatLibrary::GetAttackRating(Player, Sword, false).FindRef(OSFTags::Damage_Physical), Expected, 0.1f);
	TestEqual(TEXT("Damage matches AR x motion value"), HP - Enemy->Stats->GetCurrent(OSFTags::Stat_Health), Expected, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFStatusTest, "OSF.Status.BleedPoisonFrostbite", OSFTest::Flags)
bool FOSFStatusTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	TestWorld.Tick(0.1f);
	UOSFStatusEffectComponent* Status = Player->StatusEffects;
	UOSFStatComponent* Stats = Player->Stats;

	TestFalse(TEXT("Below threshold"), Status->AddBuildup(OSFTags::Status_Bleed, 60.f));
	TestEqual(TEXT("Buildup 60%"), Status->GetBuildupPercent(OSFTags::Status_Bleed), 0.6f, 0.01f);
	const float HP = Stats->GetCurrent(OSFTags::Stat_Health);
	TestTrue(TEXT("Bleed procs"), Status->AddBuildup(OSFTags::Status_Bleed, 50.f));
	TestEqual(TEXT("Bleed damage = 60 + 15% max"), HP - Stats->GetCurrent(OSFTags::Stat_Health), 60.f + 0.15f * 550.f, 0.5f);

	TestTrue(TEXT("Poison procs"), Status->AddBuildup(OSFTags::Status_Poison, 500.f));
	const float BeforeDot = Stats->GetCurrent(OSFTags::Stat_Health);
	TestWorld.Tick(2.f);
	TestTrue(TEXT("Poison ticks"), Stats->GetCurrent(OSFTags::Stat_Health) < BeforeDot - 10.f);
	Status->Cure(OSFTags::Status_Poison);
	TestFalse(TEXT("Cured"), Status->IsEffectActive(OSFTags::Status_Poison));

	TestTrue(TEXT("Frostbite procs"), Status->AddBuildup(OSFTags::Status_Frostbite, 500.f));
	TestTrue(TEXT("Frostbite debuff"), Player->Buffs->GetDamageTakenMultiplier() > 1.f);

	// Status resistance from equipment modifiers raises the threshold.
	Stats->SetModifiers(Player, TEXT("Resist"), { { OSFTags::Status_Bleed, 100.f, 1.f } });
	TestEqual(TEXT("Resistance"), Status->GetThreshold(OSFTags::Status_Bleed), 200.f);

	// Decay
	Status->AddBuildup(OSFTags::Status_Bleed, 50.f);
	TestWorld.Tick(6.f);
	TestTrue(TEXT("Buildup decays"), Status->GetBuildupPercent(OSFTags::Status_Bleed) < 0.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFItemsTest, "OSF.Items.InventoryEquipmentFlasksCrafting", OSFTest::Flags)
bool FOSFItemsTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	TestWorld.Tick(0.1f);
	UOSFInventoryComponent* Inventory = Player->Inventory;

	UOSFItemData* Flask = NewObject<UOSFItemData>(GetTransientPackage());
	Flask->ItemId = TEXT("TestFlask");
	Flask->bIsFlask = true;
	Flask->UseDelay = 0.2f;
	Flask->UseDuration = 0.5f;
	FOSFItemEffect Heal;
	Heal.Type = EOSFItemEffectType::RestoreStat;
	Heal.Tag = OSFTags::Stat_Health;
	Heal.Magnitude = 0.5f;
	Heal.bPercent = true;
	Flask->UseEffects.Add(Heal);

	Inventory->AddItem(Flask, 3);
	TestEqual(TEXT("Three charges"), Inventory->GetItemCount(Flask), 3);
	Player->Equipment->SetQuickItem(Flask, 0);
	Player->Stats->SetCurrent(OSFTags::Stat_Health, 100.f);
	TestTrue(TEXT("Use flask"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_UseItem));
	TestWorld.Tick(0.8f);
	TestEqual(TEXT("Healed 50%"), Player->Stats->GetCurrent(OSFTags::Stat_Health), 375.f, 0.5f);
	TestEqual(TEXT("Charge used"), Inventory->GetItemCount(Flask), 2);
	Inventory->RefillFlasks();
	TestEqual(TEXT("Refilled"), Inventory->GetItemCount(Flask), 3);

	// Stack limits overflow to storage
	UOSFItemData* Material = NewObject<UOSFItemData>(GetTransientPackage());
	Material->ItemId = TEXT("TestMaterial");
	Material->Category = EOSFItemCategory::Material;
	Material->MaxStack = 5;
	TestEqual(TEXT("Carried up to stack"), Inventory->AddItem(Material, 8), 5);
	TestEqual(TEXT("Overflow stored"), Inventory->GetStorage().Num(), 1);

	// Crafting
	UOSFItemData* Result = NewObject<UOSFItemData>(GetTransientPackage());
	Result->ItemId = TEXT("TestCrafted");
	UOSFCraftingRecipe* Recipe = NewObject<UOSFCraftingRecipe>(GetTransientPackage());
	Recipe->Ingredients.Add({ Material, 2 });
	Recipe->Result = { Result, 1 };
	TestTrue(TEXT("Craft"), Inventory->Craft(Recipe, 2));
	TestEqual(TEXT("Crafted 2"), Inventory->GetItemCount(Result), 2);
	TestEqual(TEXT("Ingredients consumed"), Inventory->GetItemCount(Material), 1);
	TestFalse(TEXT("Not enough ingredients"), Inventory->CanCraft(Recipe));

	// Armor modifiers, poise and negation
	UOSFArmorData* Chest = NewObject<UOSFArmorData>(GetTransientPackage());
	Chest->ItemId = TEXT("TestChest");
	Chest->Slot = EOSFEquipSlot::Chest;
	Chest->Poise = 20.f;
	Chest->Negation.Add(OSFTags::Damage_Physical, 50.f);
	Chest->Weight = 10.f;
	const float PoiseBefore = Player->Stats->GetMax(OSFTags::Stat_Poise);
	Player->Equipment->EquipArmor(Chest);
	TestEqual(TEXT("Armor poise"), Player->Stats->GetMax(OSFTags::Stat_Poise), PoiseBefore + 20.f);
	TestEqual(TEXT("Negation"), Player->Combat->GetDamageNegation(OSFTags::Damage_Physical), 0.5f, 0.001f);
	const float HP = Player->Stats->GetCurrent(OSFTags::Stat_Health);
	Player->Combat->ReceiveDamage(OSFTest::MakeHit(nullptr, 100.f, 0.f));
	TestEqual(TEXT("Damage halved"), HP - Player->Stats->GetCurrent(OSFTags::Stat_Health), 50.f, 0.1f);
	TestEqual(TEXT("Equip load medium (10/45)"), Player->Equipment->GetEquipLoadCategory(), EOSFEquipLoad::Light);

	// Currency
	TestFalse(TEXT("Can't overspend"), Inventory->SpendCurrency(10));
	Inventory->AddCurrency(25);
	TestTrue(TEXT("Spend"), Inventory->SpendCurrency(10));
	TestEqual(TEXT("Currency"), Inventory->GetCurrency(), 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFSaveTest, "OSF.Save.CaptureAndApplyRoundTrip", OSFTest::Flags)
bool FOSFSaveTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* A = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFPlayerCharacter* B = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(500.f, 0.f, 100.f));
	TestWorld.Tick(0.1f);

	UOSFWeaponData* Sword = NewObject<UOSFWeaponData>(GetTransientPackage());
	Sword->ItemId = TEXT("SaveSword");
	UOSFItemRegistry::Get()->RegisterItem(Sword);
	UOSFItemData* Herb = NewObject<UOSFItemData>(GetTransientPackage());
	Herb->ItemId = TEXT("SaveHerb");
	UOSFItemRegistry::Get()->RegisterItem(Herb);

	A->Inventory->AddItem(Herb, 7);
	A->Inventory->AddItem(Sword, 1);
	A->Inventory->AddCurrency(1234);
	A->Equipment->EquipWeapon(Sword, true, 1);
	A->Stats->SetBaseAttribute(OSFTags::Attribute_Strength, 30);
	A->Progress->AddFlag(TEXT("Door.Test"));
	A->Progress->AddProgress(OSFTags::Progress_Boss);
	A->Progress->AddCounter(TEXT("Vendor.X.0"), 2);

	UOSFSaveGame* Save = NewObject<UOSFSaveGame>();
	UOSFSaveSubsystem::CapturePlayer(A, Save);
	UOSFSaveSubsystem::ApplyToPlayer(B, Save, false);

	TestEqual(TEXT("Items"), B->Inventory->GetItemCount(Herb), 7);
	TestEqual(TEXT("Currency"), B->Inventory->GetCurrency(), 1234);
	TestEqual(TEXT("Equipped slot"), B->Equipment->RightHand[1].Get(), Sword);
	TestEqual(TEXT("Active slot"), B->Equipment->GetActiveSlot(true), 1);
	TestEqual(TEXT("Attribute"), B->Stats->GetBaseAttribute(OSFTags::Attribute_Strength), 30);
	TestEqual(TEXT("Level"), B->Stats->GetLevel(), A->Stats->GetLevel());
	TestTrue(TEXT("Flag"), B->Progress->HasFlag(TEXT("Door.Test")));
	TestTrue(TEXT("Tag"), B->Progress->HasProgress(OSFTags::Progress_Boss));
	TestEqual(TEXT("Counter"), B->Progress->GetCounter(TEXT("Vendor.X.0")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFDialogTest, "OSF.Progress.DialogSelectionAndVendor", OSFTest::Flags)
bool FOSFDialogTest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFNPC* NPC = TestWorld.Spawn<AOSFNPC>(FVector(200.f, 0.f, 0.f));
	TestWorld.Tick(0.1f);

	UOSFDialogData* Dialog = NewObject<UOSFDialogData>(GetTransientPackage());
	FOSFDialogEntry Later;
	Later.RequiredFlags.Add(TEXT("Met"));
	Later.Lines.Add(FText::FromString(TEXT("Again?")));
	FOSFDialogEntry First;
	First.Lines.Add(FText::FromString(TEXT("Hello")));
	First.GrantFlags.Add(TEXT("Met"));
	Dialog->Entries = { Later, First };
	NPC->Dialog = Dialog;

	TestEqual(TEXT("First meeting"), Dialog->SelectEntry(Player->Progress), 1);
	NPC->CompleteDialog(Player, Dialog->Entries[1]);
	TestEqual(TEXT("Progress driven"), Dialog->SelectEntry(Player->Progress), 0);

	UOSFItemData* Ware = NewObject<UOSFItemData>(GetTransientPackage());
	Ware->ItemId = TEXT("Ware");
	Ware->SellPrice = 40;
	UOSFVendorData* Vendor = NewObject<UOSFVendorData>(GetTransientPackage());
	Vendor->Entries.Add({ Ware, 100, 2, {} });
	NPC->Vendor = Vendor;
	Player->Inventory->AddCurrency(250);
	TestTrue(TEXT("Buy 1"), NPC->Buy(Player, 0));
	TestTrue(TEXT("Buy 2"), NPC->Buy(Player, 0));
	TestFalse(TEXT("Out of stock"), NPC->Buy(Player, 0));
	TestEqual(TEXT("Paid"), Player->Inventory->GetCurrency(), 50);
	TestTrue(TEXT("Sell"), NPC->Sell(Player, Ware));
	TestEqual(TEXT("Sell price"), Player->Inventory->GetCurrency(), 90);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFAITest, "OSF.AI.AbilitySelectionAndBossPhases", OSFTest::Flags)
bool FOSFAITest::RunTest(const FString& Parameters)
{
	OSFTest::FWorld TestWorld;
	AOSFEnemyCharacter* Enemy = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(0.f, 0.f, 100.f));
	TestWorld.Tick(0.1f);
	UOSFAICombatComponent* AI = Enemy->AICombat;
	TestTrue(TEXT("Default abilities"), AI->Abilities.Num() >= 3);

	// Close range never picks the lunge (MinRange 280), far range only picks the lunge.
	for (int32 Try = 0; Try < 20; ++Try)
	{
		const int32 Close = AI->SelectAbility(150.f, 0);
		TestTrue(TEXT("Close range ability"), Close == 0 || Close == 1);
	}
	TestEqual(TEXT("Lunge at range"), AI->SelectAbility(500.f, 0), 2);
	TestEqual(TEXT("Nothing out of range"), AI->SelectAbility(5000.f, 0), static_cast<int32>(INDEX_NONE));

	TestTrue(TEXT("Execute ability"), AI->ExecuteAbility(2));
	TestTrue(TEXT("On cooldown"), AI->IsOnCooldown(2));
	TestEqual(TEXT("Lunge unavailable while cooling down"), AI->SelectAbility(500.f, 0), static_cast<int32>(INDEX_NONE));

	// Boss phases
	Enemy->Boss->bIsBoss = true;
	FOSFBossPhase Phase;
	Phase.HealthThreshold = 0.5f;
	Enemy->Boss->Phases.Add(Phase);
	Enemy->Boss->BeginFight();
	TestTrue(TEXT("Fight active"), Enemy->IsBossFightActive());
	Enemy->Combat->ApplyDirectDamage(Enemy->Stats->GetMax(OSFTags::Stat_Health) * 0.6f, nullptr);
	TestEqual(TEXT("Phase 2"), Enemy->Boss->GetCurrentPhase(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
