// Copyright Open Soulslike Framework contributors. MIT License.

#include "OSFDemoGameMode.h"
#include "OSFDemoContent.h"
#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFAICombatComponent.h"
#include "AI/OSFBossComponent.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Combat/OSFCombatComponent.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Interaction/OSFLadder.h"
#include "Interaction/OSFNPC.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Save/OSFSaveSubsystem.h"
#include "Stats/OSFStatComponent.h"
#include "World/OSFTimeOfDay.h"
#include "World/OSFCompassMarkerComponent.h"
#include "OSFGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "OSFDemo"

namespace
{
	void SetStatMax(UOSFStatComponent* Stats, FGameplayTag Tag, float BaseMax)
	{
		for (FOSFStatDefinition& Def : Stats->StatDefinitions)
		{
			if (Def.Tag == Tag)
			{
				Def.BaseMax = BaseMax;
				Def.AttributeScaling.Reset();
			}
		}
	}

	void AddMarker(AActor* Actor, const FText& Label, const FLinearColor& Color, float MaxDistance = 0.f)
	{
		UOSFCompassMarkerComponent* Marker = NewObject<UOSFCompassMarkerComponent>(Actor);
		Marker->Label = Label;
		Marker->Color = Color;
		Marker->MaxDistance = MaxDistance;
		Marker->SetupAttachment(Actor->GetRootComponent());
		Marker->RegisterComponent();
	}

	FOSFAIAbility MakeAbility(const TCHAR* Name, float Windup, float Active, float Recovery, float Motion, float MinRange, float MaxRange, float Cooldown, float Weight, float Reach, float Radius)
	{
		FOSFAIAbility Ability;
		Ability.Name = Name;
		Ability.Attack.Timing.Windup = Windup;
		Ability.Attack.Timing.Active = Active;
		Ability.Attack.Timing.Recovery = Recovery;
		Ability.Attack.MotionValue = Motion;
		Ability.Attack.Reach = Reach;
		Ability.Attack.TraceRadius = Radius;
		Ability.MinRange = MinRange;
		Ability.MaxRange = MaxRange;
		Ability.Cooldown = Cooldown;
		Ability.Weight = Weight;
		return Ability;
	}
}

AOSFDemoGameMode::AOSFDemoGameMode()
{
}

void AOSFDemoGameMode::StartPlay()
{
	BuildWorld();
	Super::StartPlay();
}

AActor* AOSFDemoGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Players log in before StartPlay: build the demo (which adds a PlayerStart if needed) first.
	BuildWorld();
	return Super::ChoosePlayerStart_Implementation(Player);
}

AActor* AOSFDemoGameMode::SpawnBlock(const FVector& Location, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation)
{
	static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!Block)
	{
		return nullptr;
	}
	UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Cube);
	Block->SetActorScale3D(Size / 100.f);
	if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Block;
}

AOSFEnemyCharacter* AOSFDemoGameMode::SpawnSoldier(const FVector& Location, float Yaw, bool bHeavy)
{
	UOSFDemoContent* Content = UOSFDemoContent::Get();
	const FTransform Transform(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, 100.f));
	AOSFEnemyCharacter* Enemy = GetWorld()->SpawnActorDeferred<AOSFEnemyCharacter>(AOSFEnemyCharacter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->Equipment->RightHand[0] = Content->EnemySword;
	if (bHeavy)
	{
		Enemy->EnemyName = LOCTEXT("Knight", "Courtyard Knight");
		Enemy->BodyColor = FLinearColor(0.25f, 0.25f, 0.3f);
		Enemy->Equipment->LeftHand[0] = Content->Shield;
		Enemy->CurrencyReward = 450;
		Enemy->AICombat->GuardChance = 0.55f;
		Enemy->AICombat->DodgeChance = 0.f;
		Enemy->VisualScale = 1.15f;
		SetStatMax(Enemy->Stats, OSFTags::Stat_Health, 750.f);
		SetStatMax(Enemy->Stats, OSFTags::Stat_Poise, 70.f);
		Enemy->Combat->NaturalNegation.Add(OSFTags::Damage_Physical, 20.f);
		Enemy->Loot.Add({ Content->RuneShard, 1, 1.f });
		Enemy->Loot.Add({ Content->Chestplate, 1, 0.25f });
	}
	else
	{
		Enemy->EnemyName = LOCTEXT("Soldier", "Hollow Soldier");
		Enemy->CurrencyReward = 160;
		SetStatMax(Enemy->Stats, OSFTags::Stat_Health, 320.f);
		SetStatMax(Enemy->Stats, OSFTags::Stat_Poise, 30.f);
		Enemy->Loot.Add({ Content->Herb, 1, 0.6f });
		Enemy->Loot.Add({ Content->Bone, 1, 0.5f });
	}
	Enemy->SightRadius = 1200.f;
	Enemy->FinishSpawning(Transform);
	return Enemy;
}

AOSFEnemyCharacter* AOSFDemoGameMode::SpawnBoss(const FVector& Location)
{
	UOSFDemoContent* Content = UOSFDemoContent::Get();
	const FTransform Transform(FRotator(0.f, 180.f, 0.f), Location + FVector(0.f, 0.f, 160.f));
	AOSFEnemyCharacter* Boss = GetWorld()->SpawnActorDeferred<AOSFEnemyCharacter>(AOSFEnemyCharacter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Boss)
	{
		return nullptr;
	}
	Boss->SaveId = TEXT("AshenWarden");
	Boss->EnemyName = LOCTEXT("BossName", "Ashen Warden");
	Boss->BodyColor = FLinearColor(0.2f, 0.18f, 0.16f);
	Boss->VisualScale = 1.65f;
	Boss->GetCapsuleComponent()->SetCapsuleSize(70.f, 152.f);
	Boss->VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -152.f));
	Boss->CurrencyReward = 5000;
	Boss->WalkSpeed = 330.f;
	Boss->SprintSpeed = 600.f;
	Boss->TurnRate = 220.f;
	Boss->Equipment->RightHand[0] = Content->BossHalberd;
	Boss->Loot.Add({ Content->BossHalberd, 1, 1.f });
	Boss->Loot.Add({ Content->VigorTalisman, 1, 1.f });
	SetStatMax(Boss->Stats, OSFTags::Stat_Health, 2600.f);
	SetStatMax(Boss->Stats, OSFTags::Stat_Poise, 140.f);
	Boss->Combat->NaturalNegation.Add(OSFTags::Damage_Physical, 15.f);
	Boss->Combat->bCanBeBackstabbed = false;
	Boss->Combat->StaggerDuration = 0.4f;

	Boss->Boss->bIsBoss = true;
	Boss->Boss->BossName = LOCTEXT("BossBarName", "Ashen Warden, Keeper of the Gate");
	FOSFBossPhase Phase2;
	Phase2.HealthThreshold = 0.5f;
	Phase2.PhaseBuff = Content->BossRage;
	Phase2.PhaseColor = FLinearColor(0.6f, 0.15f, 0.05f);
	Boss->Boss->Phases.Add(Phase2);

	UOSFAICombatComponent* AI = Boss->AICombat;
	AI->PreferredDistance = 420.f;
	AI->AttackInterval = FVector2D(0.5f, 1.6f);
	AI->GuardChance = 0.f;
	AI->DodgeChance = 0.1f;

	FOSFAIAbility Sweep = MakeAbility(TEXT("Sweep"), 0.6f, 0.25f, 0.7f, 1.f, 0.f, 360.f, 1.5f, 3.f, 340.f, 70.f);
	Sweep.Attack.SwingYaw = -150.f;
	Sweep.FollowUp = 1;
	Sweep.FollowUpChance = 0.6f;
	FOSFAIAbility BackSweep = MakeAbility(TEXT("BackSweep"), 0.35f, 0.25f, 0.8f, 1.1f, 0.f, 380.f, 1.5f, 0.5f, 340.f, 70.f);
	BackSweep.Attack.SwingYaw = 150.f;
	BackSweep.FollowUp = 2;
	BackSweep.FollowUpChance = 0.3f;
	FOSFAIAbility Slam = MakeAbility(TEXT("Slam"), 1.05f, 0.2f, 1.1f, 1.9f, 0.f, 330.f, 4.f, 1.5f, 320.f, 90.f);
	Slam.Attack.SwingYaw = 0.f;
	Slam.Attack.SwingPitch = -130.f;
	Slam.Attack.bHyperArmor = true;
	Slam.Attack.PoiseDamageMultiplier = 2.5f;
	Slam.Attack.bCanBeParried = false;
	FOSFAIAbility Thrust = MakeAbility(TEXT("Thrust"), 0.75f, 0.3f, 0.9f, 1.4f, 380.f, 950.f, 5.f, 2.f, 300.f, 60.f);
	Thrust.Attack.SwingYaw = 0.f;
	Thrust.Attack.SwingPitch = -20.f;
	Thrust.Attack.Lunge = 1300.f;
	FOSFAIAbility FireWave = MakeAbility(TEXT("FireWave"), 1.2f, 0.35f, 1.0f, 0.6f, 0.f, 600.f, 9.f, 2.5f, 80.f, 520.f);
	FireWave.Attack.SwingYaw = -360.f;
	FireWave.Attack.BonusDamage.Add(OSFTags::Damage_Fire, 140.f);
	FireWave.Attack.bHyperArmor = true;
	FireWave.Attack.bCanBeParried = false;
	FireWave.MinPhase = 1;
	AI->Abilities = { Sweep, BackSweep, Slam, Thrust, FireWave };

	Boss->FinishSpawning(Transform);
	AddMarker(Boss, LOCTEXT("BossMarker", "Warden"), FLinearColor(0.9f, 0.2f, 0.1f));
	return Boss;
}

void AOSFDemoGameMode::BuildWorld()
{
	if (bWorldBuilt || !bBuildDemoContent)
	{
		return;
	}
	bWorldBuilt = true;
	UWorld* World = GetWorld();
	for (TActorIterator<AOSFInteractableBase> It(World); It; ++It)
	{
		return; // the level already contains authored OSF content
	}
	UOSFDemoContent* Content = UOSFDemoContent::Get();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// --- Lighting ---
	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It) { bHasSun = true; break; }
	if (bSpawnLighting && !bHasSun)
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 1000.f), FRotator(-40.f, 35.f, 0.f), Params);
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunComponent->SetAtmosphereSunLight(true);
			SunComponent->SetIntensity(6.f);
		}
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, Params);
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->GetLightComponent()->SetIntensity(1.2f);
		World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	AOSFTimeOfDay* Time = World->SpawnActor<AOSFTimeOfDay>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Time->TimeOfDay = 10.f;
	Time->DayLengthMinutes = 120.f;

	// --- Terrain ---
	const FLinearColor Ground(0.18f, 0.17f, 0.15f);
	const FLinearColor Stone(0.32f, 0.3f, 0.28f);
	SpawnBlock(FVector(3000.f, 0.f, -50.f), FVector(12000.f, 8000.f, 100.f), Ground);

	bool bHasStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It) { bHasStart = true; break; }
	if (!bHasStart)
	{
		World->SpawnActor<APlayerStart>(FVector(-200.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	}

	// --- Start area: resting point, merchant, chest ---
	AOSFRestingPoint* FirstRest = World->SpawnActor<AOSFRestingPoint>(FVector(250.f, 300.f, 0.f), FRotator(0.f, -90.f, 0.f), Params);
	FirstRest->SaveId = TEXT("RuinsEntrance");
	FirstRest->DisplayName = LOCTEXT("Rest1", "Ruins Entrance");
	AddMarker(FirstRest, FirstRest->DisplayName, FLinearColor(1.f, 0.7f, 0.3f));

	AOSFNPC* Merchant = World->SpawnActor<AOSFNPC>(FVector(450.f, -450.f, 0.f), FRotator(0.f, 150.f, 0.f), Params);
	Merchant->SaveId = TEXT("Kale");
	Merchant->NPCName = LOCTEXT("Merchant", "Merchant Kale");
	Merchant->Dialog = Content->MerchantDialog;
	Merchant->Vendor = Content->MerchantShop;

	AOSFContainer* Chest = World->SpawnActor<AOSFContainer>(FVector(1300.f, 1100.f, 0.f), FRotator(0.f, -90.f, 0.f), Params);
	Chest->SaveId = TEXT("CourtyardChest");
	Chest->Loot.Add({ Content->GateKey, 1, 1.f });
	Chest->Loot.Add({ Content->RuneShard, 2, 1.f });
	Chest->Currency = 200;

	AOSFItemPickup* Pickup1 = World->SpawnActor<AOSFItemPickup>(FVector(900.f, -300.f, 0.f), FRotator::ZeroRotator, Params);
	Pickup1->SaveId = TEXT("Pickup_Herbs");
	Pickup1->Item = Content->Herb;
	Pickup1->Count = 2;
	AOSFItemPickup* Pickup2 = World->SpawnActor<AOSFItemPickup>(FVector(1700.f, -900.f, 0.f), FRotator::ZeroRotator, Params);
	Pickup2->SaveId = TEXT("Pickup_Grease");
	Pickup2->Item = Content->Grease;
	Pickup2->Count = 2;

	// --- Soldiers in the courtyard ---
	SpawnSoldier(FVector(1900.f, 200.f, 0.f), 180.f, false);
	AOSFEnemyCharacter* Patroller = SpawnSoldier(FVector(2100.f, -500.f, 0.f), 180.f, false);
	if (Patroller)
	{
		Patroller->PatrolPoints = { FVector(2100.f, -500.f, 100.f), FVector(2100.f, 600.f, 100.f) };
	}
	SpawnSoldier(FVector(2400.f, 900.f, 0.f), 200.f, false);

	// --- Ladder to a watchtower with loot ---
	SpawnBlock(FVector(1200.f, -1900.f, 300.f), FVector(500.f, 500.f, 600.f), Stone);
	AOSFLadder* Ladder = World->SpawnActorDeferred<AOSFLadder>(AOSFLadder::StaticClass(), FTransform(FRotator(0.f, -90.f, 0.f), FVector(1200.f, -1640.f, 0.f)));
	Ladder->Height = 600.f;
	Ladder->FinishSpawning(FTransform(FRotator(0.f, -90.f, 0.f), FVector(1200.f, -1640.f, 0.f)));
	AOSFItemPickup* TowerLoot = World->SpawnActor<AOSFItemPickup>(FVector(1200.f, -1900.f, 600.f), FRotator::ZeroRotator, Params);
	TowerLoot->SaveId = TEXT("Pickup_TowerArmor");
	TowerLoot->Item = Content->Helm;

	// --- Wall with a locked gate ---
	const float WallX = 3000.f;
	SpawnBlock(FVector(WallX, -2260.f, 250.f), FVector(100.f, 4300.f, 500.f), Stone);
	SpawnBlock(FVector(WallX, 2260.f, 250.f), FVector(100.f, 4300.f, 500.f), Stone);
	SpawnBlock(FVector(WallX, 0.f, 400.f), FVector(100.f, 300.f, 200.f), Stone);
	AOSFDoor* Gate = World->SpawnActor<AOSFDoor>(FVector(WallX, 0.f, 0.f), FRotator(0.f, 180.f, 0.f), Params);
	Gate->SaveId = TEXT("CourtyardGate");
	Gate->RequiredKey = Content->GateKey;
	Gate->LockedMessage = LOCTEXT("GateLocked", "The gate is locked. A key must be somewhere nearby.");

	// --- Inner yard: heavy knight, second resting point ---
	SpawnSoldier(FVector(3700.f, 600.f, 0.f), 180.f, true);
	AOSFItemPickup* Talisman = World->SpawnActor<AOSFItemPickup>(FVector(3600.f, -1200.f, 0.f), FRotator::ZeroRotator, Params);
	Talisman->SaveId = TEXT("Pickup_Gauntlets");
	Talisman->Item = Content->Gauntlets;

	AOSFRestingPoint* GateRest = World->SpawnActor<AOSFRestingPoint>(FVector(4300.f, -900.f, 0.f), FRotator(0.f, 90.f, 0.f), Params);
	GateRest->SaveId = TEXT("AshenGate");
	GateRest->DisplayName = LOCTEXT("Rest2", "Before the Ashen Gate");
	AddMarker(GateRest, GateRest->DisplayName, FLinearColor(1.f, 0.7f, 0.3f));

	// --- Boss arena ---
	const FVector Arena(6600.f, 0.f, 0.f);
	const float Half = 1500.f;
	SpawnBlock(Arena + FVector(-Half, -1050.f, 400.f), FVector(100.f, 1900.f, 800.f), Stone);
	SpawnBlock(Arena + FVector(-Half, 1050.f, 400.f), FVector(100.f, 1900.f, 800.f), Stone);
	SpawnBlock(Arena + FVector(-Half, 0.f, 650.f), FVector(100.f, 200.f, 300.f), Stone);
	SpawnBlock(Arena + FVector(Half, 0.f, 400.f), FVector(100.f, 3000.f, 800.f), Stone);
	SpawnBlock(Arena + FVector(0.f, -Half, 400.f), FVector(3000.f, 100.f, 800.f), Stone);
	SpawnBlock(Arena + FVector(0.f, Half, 400.f), FVector(3000.f, 100.f, 800.f), Stone);
	SpawnBlock(Arena + FVector(0.f, 0.f, -45.f), FVector(3000.f, 3000.f, 100.f), FLinearColor(0.12f, 0.1f, 0.1f));

	AOSFEnemyCharacter* Boss = SpawnBoss(Arena + FVector(600.f, 0.f, 0.f));
	AOSFFogGate* Fog = World->SpawnActorDeferred<AOSFFogGate>(AOSFFogGate::StaticClass(), FTransform(FRotator::ZeroRotator, Arena + FVector(-Half, 0.f, 0.f)));
	Fog->Boss = Boss;
	Fog->SaveId = TEXT("AshenGateFog");
	Fog->FinishSpawning(FTransform(FRotator::ZeroRotator, Arena + FVector(-Half, 0.f, 0.f)));
}

void AOSFDemoGameMode::OnPlayerSpawned(AOSFPlayerCharacter* Player)
{
	BuildWorld();
	UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this);
	if (Player && Player->Inventory->GetItems().Num() == 0 && !(Save && Save->HasPendingLoad()))
	{
		UOSFDemoContent* Content = UOSFDemoContent::Get();
		UOSFInventoryComponent* Inventory = Player->Inventory;
		Inventory->AddItem(Content->Flask, 4, false);
		Inventory->AddItem(Content->FPFlask, 2, false);
		Inventory->AddItem(Content->RuneShard, 2, false);
		Inventory->AddItem(Content->Cure, 1, false);
		Inventory->AddItem(Content->Greatsword, 1, false);
		Inventory->AddItem(Content->VigorTalisman, 1, false);
		Inventory->AddItem(Content->Greaves, 1, false);
		Inventory->KnownRecipes = { Content->CureRecipe, Content->GreaseRecipe };
		Inventory->SetCurrency(800);

		UOSFEquipmentComponent* Equipment = Player->Equipment;
		Equipment->EquipWeapon(Content->Longsword, true, 0);
		Equipment->EquipWeapon(Content->Greatsword, true, 1);
		Equipment->EquipWeapon(Content->Shield, false, 0);
		Equipment->SetActiveSlots(0, 0, 0, false);
		Inventory->AddItem(Content->Longsword, 1, false);
		Inventory->AddItem(Content->Shield, 1, false);
		Equipment->SetQuickItem(Content->Flask, 0);
		Equipment->SetQuickItem(Content->FPFlask, 1);
		Equipment->SetQuickItem(Content->Grease, 2);
		Equipment->EquipArmor(Content->Chestplate);
		Inventory->AddItem(Content->Chestplate, 1, false);
	}
	Super::OnPlayerSpawned(Player);
}

#undef LOCTEXT_NAMESPACE
