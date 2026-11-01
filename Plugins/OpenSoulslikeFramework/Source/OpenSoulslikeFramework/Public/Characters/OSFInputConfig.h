// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OSFInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Input actions used by the player character. Assign your own assets, or leave the config empty and
 * the framework builds a complete keyboard/mouse + gamepad setup at runtime (see BuildDefault).
 */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> DodgeSprint;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> LightAttack;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> HeavyAttack;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Guard;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> WeaponSkill;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> LockOn;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> UseItem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Interact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> CycleRight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> CycleLeft;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> CycleQuick;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> TwoHand;
	/** Ranged weapons (optional: unmapped by default, add keys in your own mapping). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Fire;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Reload;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> CycleAmmo;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> Menu;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuUp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuDown;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuLeft;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuRight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuConfirm;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") TObjectPtr<UInputAction> MenuBack;

	/** Creates actions + a mapping context with the default soulslike layout. */
	static UOSFInputConfig* BuildDefault(UObject* Outer);
};
