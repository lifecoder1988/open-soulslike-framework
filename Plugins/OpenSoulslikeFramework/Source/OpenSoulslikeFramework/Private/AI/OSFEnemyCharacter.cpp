// Copyright Open Soulslike Framework contributors. MIT License.

#include "AI/OSFEnemyCharacter.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Animation/OSFClipAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/OSFQuestComponent.h"
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

void AOSFEnemyCharacter::BeginIdleBehavior()
{
	if (IsDead() || bOffDuty)
	{
		return;
	}
	const FOSFAnimClip* Clip = IdleBehavior == EOSFIdleBehavior::Rest ? &RestClip : IdleBehavior == EOSFIdleBehavior::Chat ? &ChatClip : nullptr;
	if (UOSFClipAnimInstance* Anim = GetClipAnimInstance(); Anim && Clip && Clip->IsValid())
	{
		FOSFAnimClip Loop = *Clip;
		Loop.bLoop = true;
		Loop.BlendIn = 0.4f;
		Anim->PlayActionClip(Loop, -1.f, false);
	}
	if (IdleBehavior == EOSFIdleBehavior::Chat && ChatPartner.IsValid())
	{
		const FVector To = (ChatPartner->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}
	bOffDuty = IdleBehavior != EOSFIdleBehavior::Stand || IdleBarkSound != nullptr;
	NextBarkTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(IdleBarkInterval.X, IdleBarkInterval.Y);
}

void AOSFEnemyCharacter::EndIdleBehavior()
{
	if (!bOffDuty)
	{
		return;
	}
	bOffDuty = false;
	if (UOSFClipAnimInstance* Anim = GetClipAnimInstance(); Anim && (IdleBehavior == EOSFIdleBehavior::Rest || IdleBehavior == EOSFIdleBehavior::Chat))
	{
		Anim->StopActionClip(0.25f);
	}
}

void AOSFEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOffDuty && IdleBarkSound && !IsDead() && GetWorld()->GetTimeSeconds() >= NextBarkTime)
	{
		// Two chatting enemies take turns.
		const AOSFEnemyCharacter* Partner = ChatPartner.Get();
		if (!Partner || GetWorld()->GetTimeSeconds() - Partner->NextBarkTime > 1.2 || Partner->NextBarkTime > GetWorld()->GetTimeSeconds() + 1.5)
		{
			UGameplayStatics::PlaySoundAtLocation(this, IdleBarkSound, GetActorLocation() + FVector(0.f, 0.f, 60.f), 0.9f, FMath::FRandRange(0.9f, 1.1f));
		}
		NextBarkTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(IdleBarkInterval.X, IdleBarkInterval.Y);
	}
}

void AOSFEnemyCharacter::HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer)
{
	EndIdleBehavior();
	Super::HandleDeath(InCombat, Killer);

	AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Killer);
	if (Player)
	{
		if (UOSFQuestComponent* Quests = Player->FindComponentByClass<UOSFQuestComponent>())
		{
			Quests->NotifyKill(KillKey.IsNone() ? GetClass()->GetFName() : KillKey);
		}
		const int32 Reward = CurrencyReward;
		Player->Inventory->AddCurrency(Reward);
		int32 Dropped = 0;
		for (const FOSFLootEntry& Entry : Loot)
		{
			if (!Entry.Item || FMath::FRand() > Entry.Chance)
			{
				continue;
			}
			if (!bDropLootAsPickups)
			{
				Player->Inventory->AddItem(Entry.Item, Entry.Count);
				continue;
			}
			// On the ground beside the body, for the player to walk over to.
			const FVector Offset = FRotator(0.f, Dropped * 97.f + FMath::FRandRange(0.f, 60.f), 0.f).Vector() * LootScatter * (Dropped == 0 ? 0.5f : 1.f);
			FVector At = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) + Offset;
			FHitResult Ground;
			if (GetWorld()->LineTraceSingleByChannel(Ground, At + FVector(0.f, 0.f, 150.f), At - FVector(0.f, 0.f, 300.f), ECC_Visibility))
			{
				At = Ground.Location;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AOSFItemPickup* Pickup = GetWorld()->SpawnActor<AOSFItemPickup>(At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params))
			{
				Pickup->Item = Entry.Item;
				Pickup->Count = Entry.Count;
				Pickup->bPersistent = false;
				Pickup->SetLifeSpan(180.f);
				++Dropped;
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
	if (CorpseHideDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(CorpseTimer, this, &AOSFEnemyCharacter::HideCorpse, CorpseHideDelay, false);
	}
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
