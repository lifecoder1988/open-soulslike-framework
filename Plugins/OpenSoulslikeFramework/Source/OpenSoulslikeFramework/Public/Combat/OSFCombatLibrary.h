// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSFTypes.h"
#include "OSFCombatLibrary.generated.h"

class UOSFWeaponData;
class ACharacter;

UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawns an effect at Location (or attached to AttachTo, Offset in its local space). Niagara gets the "Color" user parameter. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Effects", meta = (WorldContext = "WorldContext"))
	static void SpawnEffect(const UObject* WorldContext, const FOSFEffect& Effect, const FVector& Location, const FRotator& Rotation, AActor* AttachTo = nullptr);

	/** Weapon attack rating per damage type for an attacker (scaling, requirements, two-handing, buffs). */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	static TMap<FGameplayTag, float> GetAttackRating(const AActor* Attacker, const UOSFWeaponData* Weapon, bool bTwoHanded);

	/** Builds the outgoing damage of one attack. */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	static FOSFDamageSpec MakeAttackDamageSpec(const AActor* Attacker, const UOSFWeaponData* Weapon, const FOSFAttackDefinition& Attack, bool bTwoHanded);

	/** Applies a constant root-motion burst (dodges, lunges). Returns the root motion source id (0 on failure). */
	static uint16 ApplyMovementBurst(ACharacter* Character, const FVector& Direction, float Distance, float Duration, FName InstanceName);

	static void StopMovementBurst(ACharacter* Character, uint16& InOutId);

	/** Convenience: does the actor own the state tag (via its action component)? */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	static bool HasStateTag(const AActor* Actor, FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	static bool IsAlive(const AActor* Actor);

	/** Scaling grade letter (E..S) for a coefficient - used by the UI. */
	UFUNCTION(BlueprintPure, Category = "OSF|Combat")
	static FString GetScalingGrade(float Coefficient);
};
