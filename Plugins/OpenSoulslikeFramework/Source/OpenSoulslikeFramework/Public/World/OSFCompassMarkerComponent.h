// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OSFCompassMarkerComponent.generated.h"

/** Radar / compass element: actors with this component appear on the HUD compass. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFCompassMarkerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOSFCompassMarkerComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass")
	FLinearColor Color = FLinearColor(1.f, 0.8f, 0.3f);

	/** Hidden beyond this distance (0 = always visible). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass")
	float MaxDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass")
	bool bShowOnCompass = true;

	static const TArray<TWeakObjectPtr<UOSFCompassMarkerComponent>>& GetAllMarkers();

private:
	static TArray<TWeakObjectPtr<UOSFCompassMarkerComponent>> Markers;
};
