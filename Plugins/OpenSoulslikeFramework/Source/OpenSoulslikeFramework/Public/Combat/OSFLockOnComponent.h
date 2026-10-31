// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFLockOnComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnLockOnChanged, AActor*, NewTarget);

/** Target lock: acquire the best target in view, switch with the stick, auto-release on distance / line of sight / death. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFLockOnComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|LockOn")
	float AcquireRadius = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|LockOn")
	float BreakDistance = 2600.f;

	/** Seconds without line of sight before the lock breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|LockOn")
	float LostSightTimeout = 2.f;

	/** Camera rotation interpolation speed towards the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|LockOn")
	float CameraInterpSpeed = 9.f;

	/** Camera pitch offset (degrees, negative looks down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|LockOn")
	float CameraPitchOffset = -12.f;

	UPROPERTY(BlueprintAssignable, Category = "OSF|LockOn")
	FOSFOnLockOnChanged OnLockOnChanged;

	UFUNCTION(BlueprintCallable, Category = "OSF|LockOn")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "OSF|LockOn")
	bool LockOn(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "OSF|LockOn")
	void ClearLockOn();

	/** Switch target to the left (-1) or right (+1) of the current one on screen. */
	UFUNCTION(BlueprintCallable, Category = "OSF|LockOn")
	void SwitchTarget(float Direction);

	UFUNCTION(BlueprintPure, Category = "OSF|LockOn")
	AActor* GetTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintPure, Category = "OSF|LockOn")
	bool IsLockedOn() const { return Target.IsValid(); }

	/** World location the camera / HUD reticle should aim at. */
	UFUNCTION(BlueprintPure, Category = "OSF|LockOn")
	FVector GetTargetLocation() const;

protected:
	TWeakObjectPtr<AActor> Target;
	float LostSightTime = 0.f;

	AActor* FindBestTarget(const AActor* Exclude, float SideBias) const;
	void GetCandidates(TArray<AActor*>& OutCandidates) const;
	bool HasLineOfSight(const AActor* Candidate) const;
	FVector GetViewLocation(FRotator& OutRotation) const;
};
