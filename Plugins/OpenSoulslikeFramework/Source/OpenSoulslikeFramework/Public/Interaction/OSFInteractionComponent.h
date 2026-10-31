// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSFInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnFocusChanged, AActor*, NewFocus);

/** Interaction manager: tracks the best interactable around the owner and interacts with it. */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Interaction")
	float SearchRadius = 220.f;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Interaction")
	FOSFOnFocusChanged OnFocusChanged;

	UFUNCTION(BlueprintCallable, Category = "OSF|Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "OSF|Interaction")
	AActor* GetFocusedInteractable() const { return Focused.Get(); }

	UFUNCTION(BlueprintPure, Category = "OSF|Interaction")
	FText GetFocusedPrompt() const;

	UFUNCTION(BlueprintCallable, Category = "OSF|Interaction")
	void UpdateFocus();

protected:
	TWeakObjectPtr<AActor> Focused;
};
