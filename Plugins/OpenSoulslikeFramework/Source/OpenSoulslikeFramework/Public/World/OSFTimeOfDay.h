// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSFTimeOfDay.generated.h"

class ADirectionalLight;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnHourChanged, int32, Hour);

/** Example time passing / time of day system: rotates the sun, supports "pass time" at resting points. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFTimeOfDay : public AActor
{
	GENERATED_BODY()

public:
	AOSFTimeOfDay();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Hours 0..24. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	float TimeOfDay = 15.f;

	/** Real minutes for a full day. 0 = frozen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	float DayLengthMinutes = 30.f;

	/** Sun light to drive. Found automatically when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(BlueprintAssignable, Category = "Time")
	FOSFOnHourChanged OnHourChanged;

	UFUNCTION(BlueprintCallable, Category = "Time")
	void PassTime(float Hours);

	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsNight() const { return TimeOfDay < 6.f || TimeOfDay > 19.f; }

	static AOSFTimeOfDay* Find(const UObject* WorldContext);

protected:
	void ApplySun();
	int32 LastHour = -1;
};
