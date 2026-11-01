// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Actions/OSFAction.h"
#include "OSFActions.generated.h"

class UOSFWeaponData;
class UOSFItemData;

/**
 * Melee attack. Resolves its attack from the equipped weapon's moveset (combo step, running, jumping, skill),
 * runs the weapon trace during the Active window and chains combos through the Cancel window.
 */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Attack : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Attack();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EOSFAttackType BaseType = EOSFAttackType::Light;

	/** A new attack of the same type within this time after the previous one ended continues the combo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float ComboResetTime = 0.3f;

	/** Used when no weapon (or an empty moveset) is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	FOSFAttackDefinition FallbackAttack;

	/** When set (AI abilities, scripted attacks), this definition is used instead of the weapon moveset. */
	TOptional<FOSFAttackDefinition> AttackOverride;

	UFUNCTION(BlueprintPure, Category = "Attack")
	int32 GetComboIndex() const { return ComboIndex; }

	UFUNCTION(BlueprintPure, Category = "Attack")
	EOSFAttackType GetCurrentType() const { return CurrentType; }

	const FOSFAttackDefinition& GetCurrentAttack() const { return CurrentAttack; }

	virtual UOSFAction* GetRedirect() override;
	virtual bool CanActivate_Implementation() const override;

protected:
	virtual void OnPreStart() override;
	virtual void OnStarted() override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnEnded(bool bInterrupted) override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual void OnPhaseChanged(EOSFActionPhase NewPhase) override;

	virtual UAnimMontage* GetMontageToPlay() const override { return CurrentAttack.Montage; }
	virtual FName GetMontageSection() const override { return CurrentAttack.MontageSection; }
	virtual float GetMontagePlayRate() const override { return CurrentAttack.MontagePlayRate; }
	virtual const FOSFActionTiming& GetTiming() const override { return CurrentAttack.Timing; }
	virtual const FOSFAnimClip* GetClipToPlay() const override { return CurrentAttack.Clip.IsValid() ? &CurrentAttack.Clip : Super::GetClipToPlay(); }
	virtual float GetStaminaCost() const override;
	virtual float GetFPCost() const override;

	/** Which weapon performs the attack (right hand by default; left-hand skills can override). */
	virtual UOSFWeaponData* GetAttackingWeapon(bool& bOutRightHand) const;

	const FOSFAttackDefinition* ResolveAttack(EOSFAttackType& OutType, int32& OutComboIndex) const;

	UPROPERTY(Transient)
	TObjectPtr<UOSFWeaponData> CurrentWeapon = nullptr;

	FOSFAttackDefinition CurrentAttack;
	EOSFAttackType CurrentType = EOSFAttackType::Light;
	int32 ComboIndex = 0;
	bool bRightHand = true;
	uint16 LungeRootMotionId = 0;
};

UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_LightAttack : public UOSFAction_Attack
{
	GENERATED_BODY()
public:
	UOSFAction_LightAttack();
};

UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_HeavyAttack : public UOSFAction_Attack
{
	GENERATED_BODY()
public:
	UOSFAction_HeavyAttack();
};

/** Weapon skill: the weapon's SkillAttack, or a parry when a parrying shield is held in the left hand. */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_WeaponSkill : public UOSFAction_Attack
{
	GENERATED_BODY()
public:
	UOSFAction_WeaponSkill();
	virtual UOSFAction* GetRedirect() override;
};

/** Attack driven entirely by AttackOverride - used by the AI ability system. */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_AIAbility : public UOSFAction_Attack
{
	GENERATED_BODY()
public:
	UOSFAction_AIAbility();
	virtual UOSFAction* GetRedirect() override { return nullptr; }
};

/** Roll / backstep with i-frames. Distance and i-frames depend on equip load. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Dodge : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Dodge();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> RollMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> BackstepMontage = nullptr;

	/** Distance per equip load (Light / Medium / Heavy). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	FVector RollDistance = FVector(480.f, 400.f, 260.f);

	/** I-frame duration per equip load (Light / Medium / Heavy). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	FVector RollIFrames = FVector(0.40f, 0.33f, 0.24f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	float BackstepDistance = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	float BackstepIFrames = 0.2f;

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsBackstep() const { return bBackstep; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	FVector GetDodgeDirection() const { return Direction; }

	virtual bool CanActivate_Implementation() const override;

protected:
	virtual void OnPreStart() override;
	virtual void OnStarted() override;
	virtual void OnEnded(bool bInterrupted) override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual UAnimMontage* GetMontageToPlay() const override { return bBackstep ? BackstepMontage : RollMontage; }
	virtual const FOSFActionTiming& GetTiming() const override { return CurrentTiming; }

	FOSFActionTiming CurrentTiming;
	FVector Direction = FVector::ForwardVector;
	float Distance = 0.f;
	bool bBackstep = false;
	uint16 RootMotionId = 0;
};

/** Hold to guard. The first PerfectGuardWindow seconds count as a perfect guard. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Guard : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Guard();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard")
	float PerfectGuardWindow = 0.18f;

	UFUNCTION(BlueprintPure, Category = "Guard")
	bool IsPerfectGuardWindow() const { return IsRunning() && GetElapsed() <= PerfectGuardWindow; }
};

/** Parry: a short window that turns an incoming parryable hit into a riposte opportunity. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Parry : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Parry();

protected:
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
};

/** Uses the active quick item (flask, consumable...). */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_UseItem : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_UseItem();

	virtual bool CanActivate_Implementation() const override;

	/** Explicit item to use next (otherwise the active quick item). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Item")
	TObjectPtr<UOSFItemData> ItemOverride = nullptr;

	UFUNCTION(BlueprintPure, Category = "Item")
	UOSFItemData* GetItemToUse() const;

protected:
	virtual void OnPreStart() override;
	virtual void OnEnded(bool bInterrupted) override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual UAnimMontage* GetMontageToPlay() const override;
	virtual const FOSFActionTiming& GetTiming() const override { return CurrentTiming; }

	UPROPERTY(Transient)
	TObjectPtr<UOSFItemData> UsingItem = nullptr;

	FOSFActionTiming CurrentTiming;
	bool bConsumed = false;
};

/** Riposte / backstab finisher. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFAction_Execute : public UOSFAction
{
	GENERATED_BODY()

public:
	UOSFAction_Execute();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execute")
	float MotionValue = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execute")
	float RiposteDistance = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execute")
	float BackstabDistance = 100.f;

	void SetTarget(AActor* InTarget, bool bInBackstab) { Target = InTarget; bBackstab = bInBackstab; }

	UFUNCTION(BlueprintPure, Category = "Execute")
	AActor* GetTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintPure, Category = "Execute")
	bool IsBackstab() const { return bBackstab; }

	virtual bool CanActivate_Implementation() const override;

protected:
	virtual void OnPreStart() override;
	virtual void OnEnded(bool bInterrupted) override;
	virtual void OnWindowChanged(EOSFActionWindow Window, bool bOpen) override;
	virtual UAnimMontage* GetMontageToPlay() const override;

	TWeakObjectPtr<AActor> Target;
	bool bBackstab = false;
	bool bDamageDealt = false;
};
