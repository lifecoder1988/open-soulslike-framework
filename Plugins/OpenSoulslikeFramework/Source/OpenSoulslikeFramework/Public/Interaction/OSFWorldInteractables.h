// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/OSFInteractable.h"
#include "OSFTypes.h"
#include "OSFWorldInteractables.generated.h"

class UPointLightComponent;
class UMaterialInterface;
class UOSFItemData;
class AOSFEnemyCharacter;
class UOSFCombatComponent;

/** Bonfire / Site of Grace: rest, level up, refill flasks, respawn enemies, set the respawn point. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFRestingPoint : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFRestingPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resting Point")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resting Point")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resting Point")
	TObjectPtr<UStaticMeshComponent> Flame;

	UFUNCTION(BlueprintPure, Category = "Resting Point")
	bool IsDiscovered() const { return bDiscovered; }

	UFUNCTION(BlueprintCallable, Category = "Resting Point")
	void Discover(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category = "Resting Point")
	FTransform GetRespawnTransform() const;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("Rest"); }
	void UpdateVisuals();
	bool bDiscovered = false;
};

/** Door: optionally locked by a key item / progress, optionally one-way (opens only from the front). */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFDoor : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFDoor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	TObjectPtr<UOSFItemData> RequiredKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bConsumeKey = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FGameplayTagContainer RequiredProgress;

	/** Only openable from the side the actor's forward vector points to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bOneWay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FText LockedMessage;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void Open(bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bOpen; }

	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return !bOpen; }
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("Door"); }
	bool bOpen = false;
	float OpenAlpha = 0.f;
};

/** Chest / container with loot. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFContainer : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFContainer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
	TArray<FOSFLootEntry> Loot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
	int32 Currency = 0;

	UFUNCTION(BlueprintPure, Category = "Container")
	bool IsOpened() const { return bOpened; }

	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return !bOpened; }
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) override;

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("Chest"); }
	void SetOpenedVisual(bool bInOpened);
	bool bOpened = false;
};

/** Item lying in the world. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFItemPickup : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFItemPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TObjectPtr<UOSFItemData> Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	int32 Count = 1;

	/** Persistent pickups don't come back after being collected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	bool bPersistent = true;

	/** Small light so a drop reads on dark ground. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Glow;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return !bCollected && Item; }
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("Pickup"); }
	void SetCollected(bool bInCollected);
	bool bCollected = false;
	float BaseZ = 0.f;
};

/** Currency dropped on death. Touch it again to recover it - die before that and it is lost. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFBloodstain : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFBloodstain();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bloodstain")
	int32 Currency = 0;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void Tick(float DeltaSeconds) override;
};

/** Fog wall in front of a boss arena. Walking through it starts the boss fight; it disappears once the boss is dead. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFFogGate : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFFogGate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Gate")
	TObjectPtr<AOSFEnemyCharacter> Boss;

	/** Mist material (translucent); its "Color" parameter gets the gate Color. Defaults to the basic shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Gate")
	TObjectPtr<UMaterialInterface> GateMaterial;

	virtual void BeginPlay() override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) override;

	UFUNCTION(BlueprintCallable, Category = "Fog Gate")
	void SetGateActive(bool bActive);

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("FogGate"); }

	UFUNCTION()
	void HandleBossDeath(UOSFCombatComponent* InCombat, AActor* Killer);
};
