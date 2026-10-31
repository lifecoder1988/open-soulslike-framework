// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OSFGameMode.generated.h"

class AOSFPlayerCharacter;
class AOSFBloodstain;
class UOSFProgressComponent;

/** Soulslike game flow: world reset on rest, death -> bloodstain -> respawn, save application. */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API AOSFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOSFGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float RespawnDelay = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	TSubclassOf<AOSFBloodstain> BloodstainClass;

	/** Respawns enemies and refreshes every interactable from the player's progress. */
	UFUNCTION(BlueprintCallable, Category = "OSF")
	virtual void ResetWorld(UOSFProgressComponent* PlayerProgress);

	/** Refreshes interactables only (after loading a save). */
	UFUNCTION(BlueprintCallable, Category = "OSF")
	void RefreshInteractables(UOSFProgressComponent* PlayerProgress);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	virtual void HandlePlayerDeath(AOSFPlayerCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	virtual void RespawnPlayer(AOSFPlayerCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	AOSFBloodstain* SpawnBloodstain(const FVector& Location, int32 Currency);

	void ClearBloodstain();

	UFUNCTION(BlueprintPure, Category = "OSF")
	AOSFBloodstain* GetBloodstain() const { return Bloodstain.Get(); }

protected:
	/** Hook for demo / game modes that build content at runtime before the player spawns. */
	virtual void OnPlayerSpawned(AOSFPlayerCharacter* Player);

	TWeakObjectPtr<AOSFBloodstain> Bloodstain;
	FTimerHandle RespawnTimer;
};
