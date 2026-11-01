// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "OSFTypes.h"
#include "OSFCharacterBase.generated.h"

class UOSFStatComponent;
class UOSFActionComponent;
class UOSFCombatComponent;
class UOSFStatusEffectComponent;
class UOSFBuffComponent;
class UOSFInventoryComponent;
class UOSFEquipmentComponent;
class UOSFWeaponTraceComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UOSFClipAnimInstance;
class AOSFLadder;

/**
 * Base character for players, enemies and bosses. Bundles the OSF components and owns the state tags.
 *
 * When no skeletal mesh is assigned the character renders a simple procedural body (basic shapes) and animates
 * attacks, dodges, guard, stagger and death procedurally - so the framework is fully playable with zero art.
 * Assign a skeletal mesh + anim BP + montages to switch to the animated pipeline.
 */
UCLASS(Abstract)
class OPENSOULSLIKEFRAMEWORK_API AOSFCharacterBase : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AOSFCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFStatComponent> Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFActionComponent> ActionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFStatusEffectComponent> StatusEffects;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFBuffComponent> Buffs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFEquipmentComponent> Equipment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF")
	TObjectPtr<UOSFWeaponTraceComponent> WeaponTrace;

	// --- Procedural presentation ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF|Procedural")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF|Procedural")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF|Procedural")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF|Procedural")
	TObjectPtr<USceneComponent> RightHandPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSF|Procedural")
	TObjectPtr<USceneComponent> LeftHandPivot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	FLinearColor BodyColor = FLinearColor(0.35f, 0.35f, 0.4f);

	/** Uniform scale of the procedural body (bosses are bigger). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	float VisualScale = 1.f;

	UFUNCTION(BlueprintPure, Category = "OSF|Procedural")
	bool UsesProceduralVisuals() const { return bProceduralVisuals; }

	/**
	 * Optional static mesh used as the procedural body instead of the capsule shapes (e.g. an unrigged scan or
	 * generated model). It gets the same procedural motion (lean, stagger, roll, death fall).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	TObjectPtr<UStaticMesh> BodyStaticMesh = nullptr;

	/** Transform of BodyStaticMesh relative to the feet (VisualRoot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Procedural")
	FTransform BodyStaticMeshTransform;

	// --- Clip animation (skeletal mesh without an anim blueprint) ---

	/** With a skeletal mesh and no anim class, these clips drive a UOSFClipAnimInstance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip IdleClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip WalkClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip RunClip;

	/** Optional hit reaction clip (stagger / stance break). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip HitReactClip;

	/** Ground speeds at which the walk / run clips play at their own rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FVector2D ClipLocomotionSpeeds = FVector2D(220.f, 520.f);

	UFUNCTION(BlueprintPure, Category = "OSF|Animation")
	UOSFClipAnimInstance* GetClipAnimInstance() const;

	/**
	 * Assigns a skeletal mesh at runtime: scales it to TargetHeight (cm), puts its feet on the capsule bottom and
	 * applies YawOffset (meshes facing +Y need -90). Hides the procedural body. Call before or after BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "OSF|Animation")
	void SetupSkeletalMesh(USkeletalMesh* NewMesh, float TargetHeight, float YawOffset = -90.f);

	/** Attack swing offset applied to the right hand pivot (procedural mode). */
	void SetProceduralSwing(float Yaw, float Pitch, float Roll = 0.f);

	USceneComponent* GetProceduralHand(bool bRightHand) const { return bRightHand ? RightHandPivot : LeftHandPivot; }

	// --- Movement ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Movement")
	float WalkSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Movement")
	float SprintSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Movement")
	float GuardWalkSpeed = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Movement")
	float SprintStaminaPerSecond = 14.f;

	UFUNCTION(BlueprintCallable, Category = "OSF|Movement")
	void SetSprinting(bool bNewSprinting);

	UFUNCTION(BlueprintPure, Category = "OSF|Movement")
	bool IsSprinting() const;

	/** Direction the character wants to face (input, AI target...). Used by actions to steer during windups. */
	UFUNCTION(BlueprintPure, Category = "OSF|Movement")
	virtual FVector GetDesiredFacingDirection() const;

	/** World-space movement intent (set by player input or AI). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Movement")
	void SetDesiredMoveDirection(const FVector& Direction) { DesiredMoveDirection = Direction; }

	UFUNCTION(BlueprintPure, Category = "OSF|Movement")
	FVector GetDesiredMoveDirection() const { return DesiredMoveDirection; }

	/** Movement input multiplier imposed by the running action. */
	UFUNCTION(BlueprintPure, Category = "OSF|Movement")
	float GetActionMovementScale() const;

	/** Optional focus target (lock-on for players, combat target for AI). */
	UFUNCTION(BlueprintPure, Category = "OSF|Movement")
	virtual AActor* GetFocusTarget() const { return nullptr; }

	// --- Ladder ---

	UFUNCTION(BlueprintCallable, Category = "OSF|Ladder")
	void EnterLadder(AOSFLadder* Ladder, bool bFromTop);

	UFUNCTION(BlueprintCallable, Category = "OSF|Ladder")
	void ExitLadder(bool bAtTop);

	UFUNCTION(BlueprintCallable, Category = "OSF|Ladder")
	void SetLadderInput(float Value) { LadderInput = Value; }

	UFUNCTION(BlueprintPure, Category = "OSF|Ladder")
	AOSFLadder* GetCurrentLadder() const { return CurrentLadder; }

	// --- Life cycle ---

	UFUNCTION(BlueprintPure, Category = "OSF")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "OSF")
	bool HasStateTag(FGameplayTag Tag) const;

	/** Teleport + revive + restore stats. */
	UFUNCTION(BlueprintCallable, Category = "OSF")
	virtual void ResetCharacter(const FTransform& Transform);

	UFUNCTION(BlueprintImplementableEvent, Category = "OSF")
	void K2_OnDeath(AActor* Killer);

protected:
	UFUNCTION()
	virtual void HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer);

	UFUNCTION()
	virtual void HandleDamaged(UOSFCombatComponent* InCombat, const FOSFHitInfo& HitInfo);

	UFUNCTION()
	virtual void HandleStateTagChanged(FGameplayTag Tag, bool bAdded);

	virtual void UpdateMovementSpeed();
	virtual void UpdateProceduralVisuals(float DeltaSeconds);
	/** Lean / stagger / death fall applied to the skeletal mesh (clip mode has no death or stagger clips by default). */
	virtual void UpdateSkeletalPose(float DeltaSeconds);
	void InitClipAnimation();
	virtual void UpdateStaminaRegen();
	virtual void UpdateLadder(float DeltaSeconds);

	FVector DesiredMoveDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AOSFLadder> CurrentLadder;

	float LadderInput = 0.f;
	float LadderHeight = 0.f;
	bool bProceduralVisuals = true;
	float HitFlash = 0.f;
	float DeathFall = 0.f;
	FRotator SwingOffset = FRotator::ZeroRotator;
	FTransform MeshBaseTransform;
	bool bMeshBaseCaptured = false;
	FRotator SmoothedSwing = FRotator::ZeroRotator;
};
