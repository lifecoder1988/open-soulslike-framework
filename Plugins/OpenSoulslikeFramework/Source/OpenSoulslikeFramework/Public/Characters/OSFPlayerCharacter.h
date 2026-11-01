// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OSFCharacterBase.h"
#include "OSFPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UOSFLockOnComponent;
class UOSFInteractionComponent;
class UOSFProgressComponent;
class UOSFQuestComponent;
class UOSFInputConfig;
class AOSFRestingPoint;
class AOSFNPC;
class AOSFEnemyCharacter;
class AOSFBloodstain;
class AOSFPlayerController;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EOSFNotifyType : uint8
{
	Banner,	// big centered text ("YOU DIED", "GREAT ENEMY FELLED")
	Toast,	// item / small event list
	Hint	// short centered hint ("Locked")
};

/** Third-person soulslike player: camera, lock-on, input, resting, death & respawn, interaction. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFPlayerCharacter : public AOSFCharacterBase
{
	GENERATED_BODY()

public:
	AOSFPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual AActor* GetFocusTarget() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFLockOnComponent> LockOn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFProgressComponent> Progress;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFQuestComponent> Quests;

	/** Optional input assets. When null a default layout is generated at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OSF|Input")
	TObjectPtr<UOSFInputConfig> InputConfig;

	/** Hold the dodge button longer than this to sprint instead of rolling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Input")
	float SprintHoldThreshold = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Input")
	float LookSensitivity = 1.f;

	// --- Soulslike flow ---

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void RestAt(AOSFRestingPoint* RestingPoint);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void StopResting();

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void BeginDialogue(AOSFNPC* NPC);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void Notify(const FText& Text, EOSFNotifyType Type);

	/** Procedural camera shake: Amplitude cm of jitter decaying over Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "OSF")
	void AddCameraShake(float Amplitude, float Duration);

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void SetActiveBoss(AOSFEnemyCharacter* Boss);

	UFUNCTION(BlueprintPure, Category = "OSF")
	AOSFEnemyCharacter* GetActiveBoss() const { return ActiveBoss.Get(); }

	UFUNCTION(BlueprintCallable, Category = "OSF")
	void SetRespawnPoint(const FTransform& Transform, FName RestingPointId);

	UFUNCTION(BlueprintPure, Category = "OSF")
	const FTransform& GetRespawnTransform() const { return RespawnTransform; }

	UFUNCTION(BlueprintPure, Category = "OSF")
	FName GetLastRestingPoint() const { return LastRestingPointId; }

	void OnBloodstainRecovered(AOSFBloodstain* Bloodstain);

	AOSFPlayerController* GetOSFController() const;

	/** Debug / automation: perform a gameplay action by tag (same path as player input). */
	UFUNCTION(BlueprintCallable, Category = "OSF")
	bool PerformAction(FGameplayTag ActionTag);

protected:
	virtual void HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer) override;

	UFUNCTION()
	void HandleItemAdded(UOSFInventoryComponent* InInventory, UOSFItemData* Item, int32 Count);

	UFUNCTION()
	void HandleLockOnChanged(AActor* NewTarget);

	void AddMappingContext();
	bool IsInMenu() const;

	void InputMove(const FInputActionValue& Value);
	void InputMoveCompleted(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputJump();
	void InputDodgePressed();
	void InputDodgeReleased();
	void InputLight();
	void InputHeavy();
	void InputGuardPressed();
	void InputGuardReleased();
	void InputSkill();
	void InputLockOn();
	void InputUseItem();
	void InputInteract();
	void InputCycleRight();
	void InputCycleLeft();
	void InputCycleQuick();
	void InputTwoHand();
	void InputFire();
	void InputFireReleased();
	void InputReload();
	void InputCycleAmmo();
	void InputMenu();
	void InputMenuUp();
	void InputMenuDown();
	void InputMenuLeft();
	void InputMenuRight();
	void InputMenuConfirm();
	void InputMenuBack();

	UPROPERTY(Transient)
	TObjectPtr<UOSFInputConfig> ActiveInputConfig;

	TWeakObjectPtr<AOSFEnemyCharacter> ActiveBoss;
	FTransform RespawnTransform;
	FName LastRestingPointId;
	double DodgePressTime = -1.0;
	float ShakeAmplitude = 0.f;
	float ShakeTime = 0.f;
	FVector CameraBaseLocation = FVector::ZeroVector;
	bool bFireHeld = false;
	bool bDodgeHeld = false;
	double LastTargetSwitchTime = 0.0;
	FTimerHandle RespawnTimer;
};
