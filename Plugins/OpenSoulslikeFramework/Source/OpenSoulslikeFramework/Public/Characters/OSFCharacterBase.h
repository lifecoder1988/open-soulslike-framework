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
class UMaterialInterface;
class USoundBase;
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

	/** Optional death clip, held on its last frame. Without one the body tips over at the feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip DeathClip;

	/**
	 * Optional ladder clips (loops). ClimbClip plays while climbing up and freezes while the climber hangs still;
	 * ClimbDownClip plays on the way down (without one, ClimbClip runs backwards). Its rate follows the ladder's ClimbSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip ClimbClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FOSFAnimClip ClimbDownClip;

	/** Ladder height (cm) one cycle of ClimbClip covers: sets the clip rate so the feet keep pace with the rungs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation", meta = (ClampMin = 10))
	float ClimbClipStride = 120.f;

	/**
	 * Re-picks the locomotion clips: the active right-hand weapon's stance clips (UOSFWeaponData::StanceIdleClip...)
	 * when they fit this mesh, else IdleClip / WalkClip / RunClip. Called on equipment changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "OSF|Animation")
	void RefreshLocomotionClips();

	/**
	 * Fallback walk / run ground speeds (cm/s) for clips whose stride can't be measured from foot bones
	 * (UOSFClipAnimInstance::bMeasureStrideSpeed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FVector2D ClipLocomotionSpeeds = FVector2D(220.f, 520.f);

	UFUNCTION(BlueprintPure, Category = "OSF|Animation")
	UOSFClipAnimInstance* GetClipAnimInstance() const;

	// --- Aim pose (clip animation + a held gun) ---

	/** Torso bone turned toward the aim direction while a gun is held on a target or has just fired (None = first "spine" bone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	FName AimRootBone;

	/** Largest torso turn (degrees) the aim pose applies; the rest is up to the character's facing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation", meta = (ClampMin = "0", ClampMax = "90"))
	float MaxAimAngle = 70.f;

	/** Seconds the aim pose keeps up after the last shot without a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	float AimHoldAfterShot = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	float AimBlendTime = 0.18f;

	/** Barrel kick (degrees up) per shot, settling back at RecoilRecovery per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	float RecoilKick = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Animation")
	float RecoilRecovery = 14.f;

	/** World direction the character aims a gun at: the focus target, else its facing (players: the control rotation). */
	virtual FVector GetAimDirection() const;

	/**
	 * Assigns a skeletal mesh at runtime: scales it to TargetHeight (cm), puts its feet on the capsule bottom and
	 * applies YawOffset (meshes facing +Y need -90). Hides the procedural body. Call before or after BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "OSF|Animation")
	void SetupSkeletalMesh(USkeletalMesh* NewMesh, float TargetHeight, float YawOffset = -90.f);

	// --- Hit feel ---

	/** Overlay material flashed on the visible body when hit (its "Color" / "Intensity" parameters are driven). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel")
	TObjectPtr<UMaterialInterface> HitFlashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel")
	FLinearColor HitFlashColor = FLinearColor(1.f, 0.28f, 0.18f);

	/** Peak "Intensity" of the hit flash. Hits landing while the last flash is still bright (automatic fire) flash at half
	 *  of it, so a body under sustained fire pulses instead of staying bleached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel", meta = (ClampMin = "0"))
	float HitFlashStrength = 1.f;

	/** Spark material (an emissive one); null = engine basic shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel")
	TObjectPtr<UMaterialInterface> HitSparkMaterial;

	/** Spark colour of this body (blood for flesh, gold for bronze...). Guarded hits use GuardSparkColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel")
	FLinearColor HitSparkColor = FLinearColor(1.f, 0.85f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|HitFeel")
	FLinearColor GuardSparkColor = FLinearColor(0.9f, 0.95f, 1.f);

	/** Played where this body is struck (flesh, bronze...); GuardSound when the hit is guarded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> GuardSound;

	/** Pain voice on unguarded hits (at most every HurtVoiceCooldown seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> HurtVoice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	float HurtVoiceCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> DodgeSound;

	/** Played at the feet every stride while moving on the ground (louder and shorter strides when running). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	TObjectPtr<USoundBase> FootstepSound;

	/** Ground covered per footstep (cm) at walking and at sprinting speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	FVector2D FootstepStride = FVector2D(72.f, 125.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Sound")
	float FootstepVolume = 0.8f;

	/** Freezes this character's animation and movement for Seconds (hit stop). */
	UFUNCTION(BlueprintCallable, Category = "OSF|HitFeel")
	void ApplyHitStop(float Seconds);
	double LastHurtVoiceTime = -100.0;

	/** Hit flash on the body + an impact spark at Location (Scale 0 = no spark). */
	UFUNCTION(BlueprintCallable, Category = "OSF|HitFeel")
	virtual void PlayHitImpact(const FVector& Location, float Scale, bool bGuarded);

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

	UFUNCTION()
	void HandleEquipmentChangedForStance(UOSFEquipmentComponent* InEquipment) { RefreshLocomotionClips(); }

	virtual void UpdateMovementSpeed();
	virtual void UpdateProceduralVisuals(float DeltaSeconds);
	/** Lean / stagger / death fall applied to the skeletal mesh (clip mode has no death or stagger clips by default). */
	virtual void UpdateSkeletalPose(float DeltaSeconds);
	void InitClipAnimation();
	virtual void UpdateStaminaRegen();
	virtual void UpdateLadder(float DeltaSeconds);
	/** Keeps the climb clip playing, paused or reversed to match LadderInput (clip animation only). */
	void UpdateClimbClip();
	/** Turns the torso so the held gun's forward settles on GetAimDirection (clip animation only). */
	virtual void UpdateAimPose(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);
	float StepDistance = 0.f;

	UFUNCTION()
	void HandleShotFiredForAim(UOSFEquipmentComponent* InEquipment, UOSFWeaponData* Weapon, int32 Hits);

	float AimWeight = 0.f;
	float Recoil = 0.f;
	double LastShotTime = -100.0;

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
	/** The death clip is playing: no procedural fall on top. */
	bool bDeathClipPlaying = false;
	/** Locomotion clips last handed to the anim instance (RefreshLocomotionClips skips unchanged sets). */
	const UAnimSequence* AppliedLocomotion[3] = { nullptr, nullptr, nullptr };
	FRotator SwingOffset = FRotator::ZeroRotator;
	FTransform MeshBaseTransform;
	bool bMeshBaseCaptured = false;
	double HitStopUntil = 0.0;
	float OverlayFlash = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMaterial;

	void UpdateHitFeel(float DeltaSeconds);
	FRotator SmoothedSwing = FRotator::ZeroRotator;
};
