// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OSFDemoContent.generated.h"

class UOSFItemData;
class UOSFWeaponData;
class UOSFArmorData;
class UOSFTalismanData;
class UOSFCraftingRecipe;
class UOSFDialogData;
class UOSFVendorData;
class UOSFBuffData;

/**
 * All demo data (items, recipes, dialogue, shop) created in code so the demo needs no binary assets.
 * In a real project these are Data Assets created in the editor.
 */
UCLASS()
class OSFDEMO_API UOSFDemoContent : public UObject
{
	GENERATED_BODY()

public:
	static UOSFDemoContent* Get();

	UPROPERTY() TObjectPtr<UOSFItemData> Flask;
	UPROPERTY() TObjectPtr<UOSFItemData> FPFlask;
	UPROPERTY() TObjectPtr<UOSFItemData> RuneShard;
	UPROPERTY() TObjectPtr<UOSFItemData> Cure;
	UPROPERTY() TObjectPtr<UOSFItemData> Herb;
	UPROPERTY() TObjectPtr<UOSFItemData> Bone;
	UPROPERTY() TObjectPtr<UOSFItemData> Grease;
	UPROPERTY() TObjectPtr<UOSFItemData> GateKey;

	UPROPERTY() TObjectPtr<UOSFWeaponData> Longsword;
	UPROPERTY() TObjectPtr<UOSFWeaponData> Greatsword;
	UPROPERTY() TObjectPtr<UOSFWeaponData> BleedDagger;
	UPROPERTY() TObjectPtr<UOSFWeaponData> Shield;
	UPROPERTY() TObjectPtr<UOSFWeaponData> EnemySword;
	UPROPERTY() TObjectPtr<UOSFWeaponData> BossHalberd;

	UPROPERTY() TObjectPtr<UOSFArmorData> Helm;
	UPROPERTY() TObjectPtr<UOSFArmorData> Chestplate;
	UPROPERTY() TObjectPtr<UOSFArmorData> Gauntlets;
	UPROPERTY() TObjectPtr<UOSFArmorData> Greaves;

	UPROPERTY() TObjectPtr<UOSFTalismanData> VigorTalisman;
	UPROPERTY() TObjectPtr<UOSFTalismanData> StaminaTalisman;

	UPROPERTY() TObjectPtr<UOSFBuffData> GreaseBuff;
	UPROPERTY() TObjectPtr<UOSFBuffData> BossRage;

	UPROPERTY() TObjectPtr<UOSFCraftingRecipe> CureRecipe;
	UPROPERTY() TObjectPtr<UOSFCraftingRecipe> GreaseRecipe;

	UPROPERTY() TObjectPtr<UOSFDialogData> MerchantDialog;
	UPROPERTY() TObjectPtr<UOSFVendorData> MerchantShop;

private:
	void Build();
	static UOSFDemoContent* Instance;
};
