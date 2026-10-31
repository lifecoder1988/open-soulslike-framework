// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFAIController.h"
#include "AI/OSFAICombatComponent.h"
#include "AI/OSFBossComponent.h"
#include "Characters/OSFPlayerCharacter.h"
#include "Combat/OSFCombatComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Progression/OSFProgressComponent.h"
#include "Stats/OSFStatComponent.h"
#include "OSFGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "OSFEnemy"

AOSFEnemyCharacter::AOSFEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AICombat = CreateDefaultSubobject<UOSFAICombatComponent>(TEXT("AICombat"));
	Boss = CreateDefaultSubobject<UOSFBossComponent>(TEXT("Boss"));

	AIControllerClass = AOSFAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Combat->TeamId = 1;
	Combat->bPoiseBreakOpensRiposte = true;
	EnemyName = LOCTEXT("Enemy", "Hollow Soldier");
	BodyColor = FLinearColor(0.45f, 0.12f, 0.1f);
	WalkSpeed = 380.f;
	SprintSpeed = 520.f;
	GetCharacterMovement()->bUseRVOAvoidance = true;
}

void AOSFEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
	OriginalColor = BodyColor;
	AICombat->EnsureDefaultAbilities();
}

AActor* AOSFEnemyCharacter::GetFocusTarget() const
{
	const AOSFAIController* AI = Cast<AOSFAIController>(GetController());
	return AI ? AI->GetTarget() : nullptr;
}

FName AOSFEnemyCharacter::GetDefeatedFlag() const
{
	return FName(*FString::Printf(TEXT("Boss.%s"), *GetSaveId().ToString()));
}

bool AOSFEnemyCharacter::IsBoss() const
{
	return Boss && Boss->bIsBoss;
}

bool AOSFEnemyCharacter::IsBossFightActive() const
{
	return Boss && Boss->IsFightActive();
}

bool AOSFEnemyCharacter::IsBossDefeated(const UOSFProgressComponent* Progress) const
{
	if (!IsBoss())
	{
		return false;
	}
	if (Progress && Progress->HasFlag(GetDefeatedFlag()))
	{
		return true;
	}
	return IsDead();
}

void AOSFEnemyCharacter::BeginBossFight(AActor* Challenger)
{
	if (IsDead())
	{
		return;
	}
	if (Boss)
	{
		Boss->BeginFight();
	}
	if (AOSFAIController* AI = Cast<AOSFAIController>(GetController()))
	{
		AI->SetTarget(Challenger);
	}
	if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Challenger))
	{
		Player->SetActiveBoss(this);
	}
}

void AOSFEnemyCharacter::HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer)
{
	Super::HandleDeath(InCombat, Killer);

	AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Killer);
	if (Player)
	{
		const int32 Reward = CurrencyReward;
		Player->Inventory->AddCurrency(Reward);
		for (const FOSFLootEntry& Entry : Loot)
		{
			if (Entry.Item && FMath::FRand() <= Entry.Chance)
			{
				Player->Inventory->AddItem(Entry.Item, Entry.Count);
			}
		}
	}

	if (IsBoss())
	{
		Boss->EndFight();
		if (Player)
		{
			Player->Progress->AddFlag(GetDefeatedFlag());
			Player->Progress->AddProgress(Boss->DefeatedProgressTag);
			Player->Notify(LOCTEXT("GreatEnemyFelled", "GREAT ENEMY FELLED"), EOSFNotifyType::Banner);
			Player->SetActiveBoss(nullptr);
		}
	}
	else if (Player)
	{
		Player->Notify(LOCTEXT("Felled", "ENEMY FELLED"), EOSFNotifyType::Toast);
	}

	if (AOSFAIController* AI = Cast<AOSFAIController>(GetController()))
	{
		AI->HandlePawnDeath();
	}
	GetWorldTimerManager().SetTimer(CorpseTimer, this, &AOSFEnemyCharacter::HideCorpse, 4.f, false);
}

void AOSFEnemyCharacter::HideCorpse()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (Equipment)
	{
		Equipment->SetWeaponsHidden(true);
	}
}

void AOSFEnemyCharacter::ResetEnemy(const UOSFProgressComponent* PlayerProgress)
{
	if (IsBoss() && IsBossDefeated(PlayerProgress))
	{
		HideCorpse();
		return;
	}
	if (!bRespawnOnRest && IsDead())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CorpseTimer);
	if (Boss)
	{
		Boss->ResetBoss();
	}
	BodyColor = OriginalColor;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (Equipment)
	{
		Equipment->SetWeaponsHidden(false);
	}
	ResetCharacter(HomeTransform);
	if (AOSFAIController* AI = Cast<AOSFAIController>(GetController()))
	{
		AI->ResetAI();
	}
}

#undef LOCTEXT_NAMESPACE
