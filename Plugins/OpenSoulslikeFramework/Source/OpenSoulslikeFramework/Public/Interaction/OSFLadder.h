// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/OSFInteractable.h"
#include "OSFLadder.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;

/**
 * Ladder. Place it against a wall with its forward vector pointing into the wall:
 * the climber stands in front of it (behind the forward vector) and climbs up to Height.
 */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFLadder : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFLadder();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = 100))
	float Height = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float ClimbSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float RungSpacing = 40.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UInstancedStaticMeshComponent> Rungs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<USphereComponent> TopVolume;

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetLadderHeight() const { return Height; }

	FVector GetClimbLocation(float AtHeight, float CapsuleHalfHeight) const;
	FRotator GetClimbRotation() const;
	FVector GetTopExitLocation() const;
	FVector GetBottomExitLocation() const;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;

protected:
	bool IsInteractorAtTop(const AActor* Interactor) const;
};
