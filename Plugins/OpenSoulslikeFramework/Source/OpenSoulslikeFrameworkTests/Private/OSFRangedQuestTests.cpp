// Copyright Open Soulslike Framework contributors. MIT License.
// Tests for ranged weapons, perilous counters, long-frame action windows and quests.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "RenderingThread.h"

#include "OSFGameplayTags.h"
#include "Actions/OSFActionComponent.h"
#include "AI/OSFAICombatComponent.h"
#include "AI/OSFEnemyCharacter.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Combat/OSFCombatComponent.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Progression/OSFProgressComponent.h"
#include "Progression/OSFQuestComponent.h"
#include "Stats/OSFStatComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OSFRangedTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** Same manual-tick world as OpenSoulslikeFrameworkTests.cpp (no game mode, ticked by the test). */
	struct FWorld
	{
		UWorld* World = nullptr;
		uint64 CachedFrameCounter = 0;

		FWorld()
		{
			CachedFrameCounter = GFrameCounter;
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("OSFRangedTestWorld"));
			World->SetShouldTick(false);
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, Params);
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->SetActorScale3D(FVector(100.f, 100.f, 1.f));
		}

		~FWorld()
		{
			World->BeginTearingDown();
			World->EndPlay(EEndPlayReason::Quit);
			FlushRenderingCommands();
			World->RemoveFromRoot();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GFrameCounter = CachedFrameCounter;
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
				++GFrameCounter;
				World->Tick(LEVELTICK_All, Step);
				FlushRenderingCommands();
			}
		}
	};

	/** A rifle with two ammo types; the player carries Reserve rounds of each. */
	UOSFWeaponData* MakeRifle(AOSFPlayerCharacter* Player, UOSFAmmoData*& OutWhite, UOSFAmmoData*& OutRed, int32 Reserve)
	{
		OutWhite = NewObject<UOSFAmmoData>(GetTransientPackage());
		OutWhite->ItemId = TEXT("TestWhiteAmmo");
		OutWhite->Damage.Reset();
		OutWhite->Damage.Add(OSFTags::Damage_Physical, 40.f);
		OutWhite->PoiseDamage = 5.f;
		OutRed = NewObject<UOSFAmmoData>(GetTransientPackage());
		OutRed->ItemId = TEXT("TestRedAmmo");
		OutRed->Damage.Reset();
		OutRed->Damage.Add(OSFTags::Damage_Fire, 80.f);
		UOSFWeaponData* Rifle = NewObject<UOSFWeaponData>(GetTransientPackage());
		Rifle->ItemId = TEXT("TestRifle");
		Rifle->Ranged.bIsRanged = true;
		Rifle->Ranged.MagazineSize = 10;
		Rifle->Ranged.Ammo = { OutWhite, OutRed };
		Rifle->Ranged.SpreadDegrees = 0.f;
		Rifle->Ranged.AimAssistDegrees = 0.f;
		Rifle->Ranged.FireInterval = 0.1f;
		Rifle->Ranged.ReloadTime = 0.5f;
		Player->Inventory->AddItem(OutWhite, Reserve, false);
		Player->Inventory->AddItem(OutRed, Reserve, false);
		Player->Equipment->EquipWeapon(Rifle, true, 0);
		return Rifle;
	}

	FOSFDamageEvent MakeHit(AActor* Instigator, float Physical, float Poise, FGameplayTag Source)
	{
		FOSFDamageEvent Event;
		Event.Instigator = Instigator;
		Event.Spec.Damage.Add(OSFTags::Damage_Physical, Physical);
		Event.Spec.PoiseDamage = Poise;
		Event.Spec.SourceTags.AddTag(Source);
		return Event;
	}

	void Place(AActor* Actor, float X, float Y, float Yaw)
	{
		Actor->SetActorLocationAndRotation(FVector(X, Y, 100.f), FRotator(0.f, Yaw, 0.f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFRangedTest, "OSF.Ranged.MagazineReloadAmmoNegation", OSFRangedTest::Flags)
bool FOSFRangedTest::RunTest(const FString& Parameters)
{
	OSFRangedTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Enemy = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(600.f, 0.f, 100.f), 180.f);
	Enemy->AICombat->Abilities.Reset();
	Enemy->Combat->NaturalNegation.Add(OSFTags::Damage_Physical, 80.f);
	UOSFAmmoData* White = nullptr;
	UOSFAmmoData* Red = nullptr;
	UOSFWeaponData* Rifle = OSFRangedTest::MakeRifle(Player, White, Red, 25);
	TestWorld.Tick(0.1f);
	OSFRangedTest::Place(Player, 0.f, 0.f, 0.f);
	OSFRangedTest::Place(Enemy, 600.f, 0.f, 180.f);
	UOSFEquipmentComponent* Equipment = Player->Equipment;

	TestEqual(TEXT("Active ranged weapon"), Equipment->GetActiveRangedWeapon(), Rifle);
	TestEqual(TEXT("First ammo type loaded by default"), Equipment->GetMagazine(Rifle).Ammo.Get(), White);
	TestFalse(TEXT("Cannot shoot with an empty magazine"), Player->ActionComponent->FindAction(OSFTags::Action_Shoot)->CanActivate());

	// Shooting an empty gun redirects to a reload that moves reserve rounds into the magazine.
	TestTrue(TEXT("Shoot -> reload"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_Shoot));
	TestTrue(TEXT("Reloading"), Player->HasStateTag(OSFTags::State_Reloading));
	TestWorld.Tick(0.7f);
	TestEqual(TEXT("Magazine full"), Equipment->GetMagazine(Rifle).Rounds, 10);
	TestEqual(TEXT("Reserve 25 - 10"), Equipment->GetReserveRounds(Rifle), 15);

	// One shot: 40 physical vs 80% negation = 8 damage, reported as a mostly negated ("ricochet") hit.
	FOSFHitInfo LastHit;
	const float HP = Enemy->Stats->GetCurrent(OSFTags::Stat_Health);
	TestTrue(TEXT("Shoot"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_Shoot));
	TestWorld.Tick(0.3f);
	TestEqual(TEXT("One round spent"), Equipment->GetMagazine(Rifle).Rounds, 9);
	TestEqual(TEXT("White round damage after negation"), HP - Enemy->Stats->GetCurrent(OSFTags::Stat_Health), 8.f, 0.1f);
	LastHit = Enemy->Combat->ReceiveDamage(OSFRangedTest::MakeHit(Player, 40.f, 0.f, OSFTags::Source_Projectile));
	TestEqual(TEXT("Negated fraction"), LastHit.NegatedFraction, 0.8f, 0.01f);

	// Cycling ammo returns loaded rounds to the inventory and selects the next type.
	TestTrue(TEXT("Cycle to red"), Equipment->CycleAmmo(Rifle));
	TestEqual(TEXT("Red selected"), Equipment->GetMagazine(Rifle).Ammo.Get(), Red);
	TestEqual(TEXT("White rounds back in reserve"), Player->Inventory->GetItemCount(White), 24);
	TestEqual(TEXT("Red reload"), Equipment->Reload(Rifle), 10);
	const float HP2 = Enemy->Stats->GetCurrent(OSFTags::Stat_Health);
	TestWorld.Tick(0.2f);
	TestTrue(TEXT("Shoot red"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_Shoot));
	TestWorld.Tick(0.3f);
	TestEqual(TEXT("Red round ignores physical negation"), HP2 - Enemy->Stats->GetCurrent(OSFTags::Stat_Health), 80.f, 0.1f);

	// A trigger volume in the line of fire doesn't stop bullets.
	AOSFItemPickup* Pickup = TestWorld.Spawn<AOSFItemPickup>(FVector(300.f, 0.f, 50.f));
	const float HP3 = Enemy->Stats->GetCurrent(OSFTags::Stat_Health);
	TestWorld.Tick(0.2f);
	TestTrue(TEXT("Shoot through trigger"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_Shoot));
	TestWorld.Tick(0.3f);
	TestTrue(TEXT("Bullet passed the interaction volume"), Enemy->Stats->GetCurrent(OSFTags::Stat_Health) < HP3);
	Pickup->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFPerilousTest, "OSF.Combat.PerilousCounterAndLongFrames", OSFRangedTest::Flags)
bool FOSFPerilousTest::RunTest(const FString& Parameters)
{
	OSFRangedTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Enemy = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(500.f, 0.f, 100.f), 180.f);
	FOSFAIAbility Cleave;
	Cleave.Name = TEXT("Cleave");
	Cleave.Attack.Timing.Windup = 1.f;
	Cleave.Attack.Timing.Active = 0.2f;
	Cleave.Attack.Timing.Recovery = 0.5f;
	Cleave.Attack.bHyperArmor = true;
	Cleave.Attack.bPerilous = true;
	Cleave.Attack.CounterTags.AddTag(OSFTags::Source_Projectile);
	Cleave.Attack.CounterPoiseDamage = 1000.f;
	Cleave.MaxRange = 5000.f;
	Enemy->AICombat->Abilities = { Cleave };
	TestWorld.Tick(0.1f);
	OSFRangedTest::Place(Enemy, 500.f, 0.f, 180.f);

	TestTrue(TEXT("Perilous attack starts"), Enemy->AICombat->ExecuteAbility(0));
	TestWorld.Tick(0.2f);
	TestTrue(TEXT("Perilous during windup"), Enemy->HasStateTag(OSFTags::State_Perilous));

	// A melee-sourced hit doesn't counter (hyper armor holds).
	TestFalse(TEXT("Melee does not counter"), Enemy->Combat->ReceiveDamage(OSFRangedTest::MakeHit(Player, 10.f, 5.f, OSFTags::Source_Melee)).bCountered);
	TestTrue(TEXT("Still attacking"), Enemy->HasStateTag(OSFTags::State_Attacking));

	// A projectile counters: attack cancelled, stance broken (CounterPoiseDamage >= poise), open to a riposte.
	const FOSFHitInfo Info = Enemy->Combat->ReceiveDamage(OSFRangedTest::MakeHit(Player, 10.f, 5.f, OSFTags::Source_Projectile));
	TestTrue(TEXT("Projectile counters"), Info.bCountered);
	TestEqual(TEXT("Stance broken"), Info.Reaction, EOSFHitReaction::PoiseBreak);
	TestFalse(TEXT("Attack cancelled"), Enemy->HasStateTag(OSFTags::State_Attacking));
	TestFalse(TEXT("No longer perilous"), Enemy->HasStateTag(OSFTags::State_Perilous));

	// Long frames: one 0.5 s step jumps over the shot's short active window - it must still open (shots fire, blades trace).
	Enemy->SetActorLocation(FVector(0.f, 3000.f, 100.f));
	AOSFEnemyCharacter* Target = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(400.f, 0.f, 100.f), 180.f);
	Target->AICombat->Abilities.Reset();
	UOSFAmmoData* White = nullptr;
	UOSFAmmoData* Red = nullptr;
	UOSFWeaponData* Rifle = OSFRangedTest::MakeRifle(Player, White, Red, 20);
	TestWorld.Tick(0.1f);
	Player->Equipment->Reload(Rifle);
	OSFRangedTest::Place(Player, 0.f, 0.f, 0.f);
	OSFRangedTest::Place(Target, 400.f, 0.f, 180.f);
	const float HP = Target->Stats->GetCurrent(OSFTags::Stat_Health);
	TestTrue(TEXT("Shoot"), Player->ActionComponent->TryExecuteAction(OSFTags::Action_Shoot));
	TestWorld.Tick(0.5f, 0.5f);
	TestEqual(TEXT("Round fired despite the long frame"), Player->Equipment->GetMagazine(Rifle).Rounds, 9);
	TestTrue(TEXT("And it hit"), Target->Stats->GetCurrent(OSFTags::Stat_Health) < HP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSFQuestTest, "OSF.Progress.QuestObjectives", OSFRangedTest::Flags)
bool FOSFQuestTest::RunTest(const FString& Parameters)
{
	OSFRangedTest::FWorld TestWorld;
	AOSFPlayerCharacter* Player = TestWorld.Spawn<AOSFPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	TestWorld.Tick(0.1f);
	UOSFItemData* Reward = NewObject<UOSFItemData>(GetTransientPackage());
	Reward->ItemId = TEXT("QuestReward");
	UOSFQuestData* Quest = NewObject<UOSFQuestData>(GetTransientPackage());
	Quest->QuestId = TEXT("TestQuest");
	Quest->bSequential = false;
	FOSFQuestObjective Kill;
	Kill.Type = EOSFQuestObjectiveType::Kill;
	Kill.Key = TEXT("Ghoul");
	Kill.Count = 3;
	FOSFQuestObjective Meet;
	Meet.Type = EOSFQuestObjectiveType::Flag;
	Meet.Key = TEXT("NPC.Smith.Met");
	Quest->Objectives = { Kill, Meet };
	Quest->RewardItems.Add({ Reward, 2 });
	Quest->RewardCurrency = 500;
	Quest->StartFlag = TEXT("Story.Start");

	UOSFQuestComponent* Quests = Player->Quests;
	Quests->Quests.Add(Quest);
	// Kills made before the quest starts don't count.
	Quests->NotifyKill(TEXT("Ghoul"));
	TestFalse(TEXT("Not started yet"), Quests->IsQuestActive(Quest));
	Player->Progress->AddFlag(TEXT("Story.Start"));
	Quests->Evaluate();
	TestTrue(TEXT("Started by its flag"), Quests->IsQuestActive(Quest));
	TestEqual(TEXT("Earlier kills ignored"), Quests->GetObjectiveProgress(Quest, 0), 0);

	// A kill through the enemy death path (KillKey), plus direct notifications.
	AOSFEnemyCharacter* Ghoul = TestWorld.Spawn<AOSFEnemyCharacter>(FVector(300.f, 0.f, 100.f), 180.f);
	Ghoul->KillKey = TEXT("Ghoul");
	TestWorld.Tick(0.1f);
	Ghoul->Combat->Kill(Player);
	TestEqual(TEXT("Enemy death counts"), Quests->GetObjectiveProgress(Quest, 0), 1);
	Quests->NotifyKill(TEXT("Other"));
	Quests->NotifyKill(TEXT("Ghoul"));
	TestEqual(TEXT("Two ghouls"), Quests->GetObjectiveProgress(Quest, 0), 2);
	Quests->NotifyKill(TEXT("Ghoul"));
	TestEqual(TEXT("Kill objective complete"), Quests->GetObjectiveProgress(Quest, 0), 3);
	TestEqual(TEXT("Current objective is the flag"), Quests->GetCurrentObjective(Quest), 1);
	TestFalse(TEXT("Not complete without the flag"), Quests->IsQuestComplete(Quest));

	const int32 Currency = Player->Inventory->GetCurrency();
	Player->Progress->AddFlag(TEXT("NPC.Smith.Met"));
	Quests->Evaluate();
	TestTrue(TEXT("Quest complete"), Quests->IsQuestComplete(Quest));
	TestEqual(TEXT("Reward items"), Player->Inventory->GetItemCount(Reward), 2);
	TestEqual(TEXT("Reward currency"), Player->Inventory->GetCurrency(), Currency + 500);
	TestTrue(TEXT("Done flag (saved with progress)"), Player->Progress->HasFlag(TEXT("Quest.TestQuest.Done")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
