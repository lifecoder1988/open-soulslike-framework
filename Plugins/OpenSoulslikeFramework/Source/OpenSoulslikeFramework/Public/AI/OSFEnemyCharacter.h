// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OSFCharacterBase.h"
#include "OSFEnemyCharacter.generated.h"

class UOSFAICombatComponent;
class UOSFBossComponent;
class UOSFProgressComponent;
class UBehaviorTree;

/** Enemy / boss. Drops currency and loot, respawns when the player rests (bosses stay dead). */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFEnemyCharacter : public AOSFCharacterBase
{
	GENERATED_BODY()

public:
	AOSFEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual AActor* GetFocusTarget() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFAICombatComponent> AICombat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFBossComponent> Boss;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	FText EnemyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	int32 CurrencyReward = 100;

	/** Counted by Kill quest objectives ("Kill.<KillKey>" progress counter). Defaults to the class name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	FName KillKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	TArray<FOSFLootEntry> Loot;

	/** Unique id for persistence (bosses). Defaults to the actor name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	FName SaveId;

	/** World-space patrol points (looped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy", meta = (MakeEditWidget = true))
	TArray<FVector> PatrolPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	float SightRadius = 1600.f;

	/** Notices the player within this radius even from behind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	float ProximityAggroRadius = 350.f;

	/** Gives up and returns home past this distance from home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	float LeashRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	float TurnRate = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	bool bRespawnOnRest = true;

	/** Seconds the body stays after death before it is hidden; 0 keeps it until the next rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	float CorpseHideDelay = 4.f;

	/** Optional behavior tree. If set the built-in state machine is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Enemy")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UFUNCTION(BlueprintPure, Category = "OSF|Enemy")
	FName GetSaveId() const { return SaveId.IsNone() ? GetFName() : SaveId; }

	UFUNCTION(BlueprintPure, Category = "OSF|Enemy")
	FName GetDefeatedFlag() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Enemy")
	bool IsBoss() const;

	UFUNCTION(BlueprintPure, Category = "OSF|Enemy")
	bool IsBossFightActive() const;

	bool IsBossDefeated(const UOSFProgressComponent* Progress) const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Enemy")
	void BeginBossFight(AActor* Challenger);

	/** Respawn at home (resting / player death). Defeated bosses stay dead. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Enemy")
	virtual void ResetEnemy(const UOSFProgressComponent* PlayerProgress);

	UFUNCTION(BlueprintPure, Category = "OSF|Enemy")
	const FTransform& GetHomeTransform() const { return HomeTransform; }

protected:
	virtual void HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer) override;

	void HideCorpse();

	FTransform HomeTransform;
	FLinearColor OriginalColor;
	FTimerHandle CorpseTimer;
};
