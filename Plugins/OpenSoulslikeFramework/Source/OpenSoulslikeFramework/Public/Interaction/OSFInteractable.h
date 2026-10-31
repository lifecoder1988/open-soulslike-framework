// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "OSFInteractable.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UOSFProgressComponent;
class UMaterialInstanceDynamic;

UINTERFACE(MinimalAPI, BlueprintType)
class UOSFInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can interact with (resting points, doors, chests, ladders, NPCs, pickups...). */
class OPENSOULSLIKEFRAMEWORK_API IOSFInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSF|Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSF|Interaction")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSF|Interaction")
	FText GetInteractionPrompt(AActor* Interactor) const;
};

/**
 * Convenience base for interactables: interaction volume, a mesh, a prompt and persistence
 * through the player's progress flags ("<Prefix>.<SaveId>").
 */
UCLASS(Abstract, Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFInteractableBase : public AActor, public IOSFInteractable
{
	GENERATED_BODY()

public:
	AOSFInteractableBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USphereComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

	/** Unique id used for persistence. Defaults to the actor name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FName SaveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FLinearColor Color = FLinearColor(0.5f, 0.4f, 0.3f);

	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return true; }
	virtual void Interact_Implementation(AActor* Interactor) override {}
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override { return Prompt; }

	/** Re-reads persistent state from the player's progress (after loading a save / respawn). */
	virtual void RefreshFromProgress(UOSFProgressComponent* Progress) {}

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FName GetPersistentFlag() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FName GetSaveId() const;

protected:
	/** Prefix of the persistent flag, e.g. "Chest". */
	virtual FString GetFlagPrefix() const { return TEXT("Interactable"); }

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMaterial;

	void SetMeshColor(const FLinearColor& InColor);
	static UOSFProgressComponent* GetProgress(AActor* Interactor);
};
