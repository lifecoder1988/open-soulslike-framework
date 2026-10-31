// Copyright Open Soulslike Framework contributors. MIT License.

#include "Game/OSFGameMode.h"
#include "Game/OSFPlayerController.h"
#include "Game/OSFHUD.h"
#include "Characters/OSFPlayerCharacter.h"
#include "AI/OSFEnemyCharacter.h"
#include "Interaction/OSFInteractable.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Items/OSFInventoryComponent.h"
#include "Save/OSFSaveSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

AOSFGameMode::AOSFGameMode()
{
	DefaultPawnClass = AOSFPlayerCharacter::StaticClass();
	PlayerControllerClass = AOSFPlayerController::StaticClass();
	HUDClass = AOSFHUD::StaticClass();
	BloodstainClass = AOSFBloodstain::StaticClass();
}

void AOSFGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (AOSFPlayerCharacter* Player = NewPlayer ? Cast<AOSFPlayerCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		OnPlayerSpawned(Player);
	}
}

void AOSFGameMode::OnPlayerSpawned(AOSFPlayerCharacter* Player)
{
	if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this))
	{
		Save->ApplyPendingLoad(Player);
	}
	RefreshInteractables(Player->Progress);
}

void AOSFGameMode::RefreshInteractables(UOSFProgressComponent* PlayerProgress)
{
	for (TActorIterator<AOSFInteractableBase> It(GetWorld()); It; ++It)
	{
		It->RefreshFromProgress(PlayerProgress);
	}
}

void AOSFGameMode::ResetWorld(UOSFProgressComponent* PlayerProgress)
{
	for (TActorIterator<AOSFEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->ResetEnemy(PlayerProgress);
	}
	RefreshInteractables(PlayerProgress);
}

AOSFBloodstain* AOSFGameMode::SpawnBloodstain(const FVector& Location, int32 Currency)
{
	ClearBloodstain();
	if (Currency <= 0 || !BloodstainClass)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOSFBloodstain* Stain = GetWorld()->SpawnActor<AOSFBloodstain>(BloodstainClass, FTransform(Location), Params);
	if (Stain)
	{
		Stain->Currency = Currency;
		Bloodstain = Stain;
	}
	return Stain;
}

void AOSFGameMode::ClearBloodstain()
{
	if (AOSFBloodstain* Stain = Bloodstain.Get())
	{
		if (!Stain->IsActorBeingDestroyed())
		{
			Stain->Destroy();
		}
	}
	Bloodstain.Reset();
}

void AOSFGameMode::HandlePlayerDeath(AOSFPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}
	// Lose the runes you carry; they wait where you fell. Dying again loses them for good.
	const FVector FeetLocation = Player->GetActorLocation() - FVector(0.f, 0.f, 85.f);
	SpawnBloodstain(FeetLocation, Player->Inventory->GetCurrency());
	Player->Inventory->SetCurrency(0);

	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &AOSFGameMode::RespawnPlayer, Player);
	GetWorldTimerManager().SetTimer(RespawnTimer, Delegate, RespawnDelay, false);
}

void AOSFGameMode::RespawnPlayer(AOSFPlayerCharacter* Player)
{
	if (!IsValid(Player))
	{
		return;
	}
	Player->ResetCharacter(Player->GetRespawnTransform());
	Player->Inventory->RefillFlasks();
	ResetWorld(Player->Progress);
}
