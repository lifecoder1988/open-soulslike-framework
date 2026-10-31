// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFTypes.h"
#include "OSFWeaponTraceComponent.generated.h"

class AOSFWeaponActor;

/**
 * Weapon collision manager. While a trace is active it sweeps the weapon blade (sockets "TraceStart"/"TraceEnd"
 * on the weapon mesh, or the blade length of the weapon actor) between frames, or - without a weapon actor -
 * a sphere in front of the owner. Each victim is hit at most once per trace.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFWeaponTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFWeaponTraceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** Number of sample points along the blade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Trace")
	int32 BladeSamples = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Trace")
	bool bDrawDebug = false;

	UFUNCTION(BlueprintCallable, Category = "OSF|Trace")
	void BeginTrace(const FOSFDamageSpec& Spec, float Reach, float Radius, AOSFWeaponActor* Weapon);

	UFUNCTION(BlueprintCallable, Category = "OSF|Trace")
	void EndTrace();

	UFUNCTION(BlueprintPure, Category = "OSF|Trace")
	bool IsTracing() const { return bTracing; }

	/** Runs one trace step immediately (also used by AnimNotify-driven traces and tests). */
	void TraceStep();

	/** Delivers a hit to Victim if not already hit during this trace. */
	FOSFHitInfo DeliverHit(AActor* Victim, const FVector& HitLocation);

protected:
	FOSFDamageSpec ActiveSpec;
	TWeakObjectPtr<AOSFWeaponActor> ActiveWeapon;
	float ActiveReach = 200.f;
	float ActiveRadius = 40.f;
	bool bTracing = false;
	bool bHasPrevious = false;
	TArray<FVector> PreviousPoints;
	TSet<TWeakObjectPtr<AActor>> HitActors;

	void GatherBladePoints(TArray<FVector>& OutPoints) const;
};
