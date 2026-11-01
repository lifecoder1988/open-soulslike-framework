// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSFTypes.h"
#include "OSFWeaponActor.generated.h"

class UStaticMeshComponent;
class UOSFWeaponData;

/** Visual representation of an equipped weapon. Provides the blade points used by the weapon trace. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AOSFWeaponActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UOSFWeaponData> WeaponData;

	/** Sockets on Mesh used for the blade trace. Falls back to Root -> Root + X * BladeLength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName TraceStartSocket = TEXT("TraceStart");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName TraceEndSocket = TEXT("TraceEnd");

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void InitializeWeapon(UOSFWeaponData* InData);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	void GetBladePoints(FVector& OutStart, FVector& OutEnd) const;

	/** Called when this weapon lands a hit. */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnWeaponHit(AActor* Victim, const FVector& Location, const FOSFHitInfo& HitInfo);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnTraceStateChanged(bool bTracing);

	/**
	 * Keep the weapon pointing along the owner's facing (plus FacingOffset) while it follows the hand's position.
	 * For imported rigs whose hand bone axes are unknown (set by UOSFEquipmentComponent::bAlignWeaponsToOwner).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAlignToOwnerFacing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FRotator FacingOffset = FRotator::ZeroRotator;

	virtual void Tick(float DeltaSeconds) override;
};
