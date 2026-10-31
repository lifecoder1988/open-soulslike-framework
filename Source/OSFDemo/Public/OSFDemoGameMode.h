// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Game/OSFGameMode.h"
#include "OSFDemoGameMode.generated.h"

class AOSFEnemyCharacter;

/**
 * Demo game mode: if the level contains no OSF content it builds a small soulslike area at runtime out of
 * engine basic shapes - a resting point, a merchant, soldiers, a locked gate, a ladder, loot and a two-phase
 * boss behind a fog gate. Any empty level works, which keeps the repository free of binary map data.
 */
UCLASS()
class OSFDEMO_API AOSFDemoGameMode : public AOSFGameMode
{
	GENERATED_BODY()

public:
	AOSFDemoGameMode();

	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	bool bBuildDemoContent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	bool bSpawnLighting = true;

protected:
	virtual void OnPlayerSpawned(AOSFPlayerCharacter* Player) override;

	void BuildWorld();
	AActor* SpawnBlock(const FVector& Location, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator);
	AOSFEnemyCharacter* SpawnSoldier(const FVector& Location, float Yaw, bool bHeavy);
	AOSFEnemyCharacter* SpawnBoss(const FVector& Location);

	bool bWorldBuilt = false;
};
