// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Actions/OSFAction.h"
#include "GameFramework/Actor.h"
#include "OSFRangedActions.generated.h"

class UOSFWeaponData;
class UOSFAmmoData;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Fires the active ranged weapon (hitscan). Aims at the lock-on / AI focus target, else along the player's view
 * (with a small aim-assist cone), applies spread per pellet and resolves hits through UOSFCombatComponent with the
 * ammo's damage and source tags. An empty magazine redirects to a reload.
 */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Shoot : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Shoot();

	virtual bool CanActivate_Implementation() const override;
	virtual UOSFAction* GetRedirect() override;

	/** Fires immediately with the given weapon (AI / scripted shots use it too). Returns the number of pellets that hit a character. */
	int32 FireWeapon(UOSFWeaponData* Weapon);

	/** Aim point used for the next shot (lock-on target, AI target, or the view trace). */
	FVector ComputeAimPoint(const UOSFWeaponData* Weapon, const FVector& Muzzle) const;

	FVector GetMuzzleLocation(const UOSFWeaponData* Weapon) const;

protected:
	virtual void OnPreStart() override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual const FOSFActionTiming& GetTiming() const override { return CurrentTiming; }
	virtual const FOSFAnimClip* GetClipToPlay() const override;

	UPROPERTY(Transient)
	TObjectPtr<UOSFWeaponData> Weapon = nullptr;

	FOSFActionTiming CurrentTiming;
};

/** Reloads the active ranged weapon from the inventory. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Reload : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Reload();

	virtual bool CanActivate_Implementation() const override;

protected:
	virtual void OnPreStart() override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual const FOSFActionTiming& GetTiming() const override { return CurrentTiming; }
	virtual const FOSFAnimClip* GetClipToPlay() const override;

	UPROPERTY(Transient)
	TObjectPtr<UOSFWeaponData> Weapon = nullptr;

	FOSFActionTiming CurrentTiming;
};

/** Short-lived bullet tracer + muzzle flash built from engine basic shapes (no assets needed). */
UCLASS(NotBlueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFTracer : public AActor
{
	GENERATED_BODY()

public:
	AOSFTracer();

	virtual void Tick(float DeltaSeconds) override;

	static AOSFTracer* Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bImpact, UMaterialInterface* Material = nullptr);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UStaticMeshComponent> Beam;

	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UStaticMeshComponent> Spark;

	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UPointLightComponent> Flash;

	float Life = 0.f;
	float MaxLife = 0.09f;
	FVector BeamScale = FVector::OneVector;
};
