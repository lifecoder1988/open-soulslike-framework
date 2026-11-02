// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSFTypes.h"
#include "OSFWeaponActor.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
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

	/** Used when the weapon data has a WeaponSkeletalMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UOSFWeaponData> WeaponData;

	/** Sockets on Mesh used for the blade trace. Falls back to Root -> Root + X * BladeLength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName TraceStartSocket = TEXT("TraceStart");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName TraceEndSocket = TEXT("TraceEnd");

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void InitializeWeapon(UOSFWeaponData* InData);

	/** MeshTransform authored in the socket bone's space: scale its offset and size with the holder mesh (UOSFWeaponData::bMeshTransformInBoneSpace). */
	void ApplyBoneSpaceScale(float HolderScale);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	void GetBladePoints(FVector& OutStart, FVector& OutEnd) const;

	/**
	 * Barrel / blade axis of the held mesh in world space: its longest side, pointing away from the hand (the actor's
	 * origin). The actor's +X without a mesh. What a gun points at, whatever the hand bone's axes are.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetBarrelDirection() const;

	/** Far end of the barrel in world space (socket "Muzzle" when the mesh has one). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	/** The barrel axis in the actor's (= attach bone's) space: what the aim pose turns onto the target. */
	FVector GetBarrelInActorSpace() const;

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

protected:
	/** Mesh-local barrel axis and muzzle point (CacheBarrel). */
	FVector BarrelLocal = FVector::ForwardVector;
	FVector MuzzleLocal = FVector::ZeroVector;
	bool bBarrelCached = false;
	void CacheBarrel();
};
