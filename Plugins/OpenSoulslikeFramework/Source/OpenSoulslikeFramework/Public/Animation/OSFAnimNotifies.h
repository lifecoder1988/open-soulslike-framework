// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Actions/OSFAction.h"
#include "OSFAnimNotifies.generated.h"

/** Opens an action window for the duration of the notify state. */
UCLASS(Abstract)
class OPENSOULSLIKEFRAMEWORK_API UOSFAnimNotifyState_Window : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	EOSFActionWindow Window = EOSFActionWindow::Active;
};

/** Damage window: runs the weapon trace. */
UCLASS(meta = (DisplayName = "OSF Weapon Trace"))
class OPENSOULSLIKEFRAMEWORK_API UOSFANS_WeaponTrace : public UOSFAnimNotifyState_Window
{
	GENERATED_BODY()
public:
	UOSFANS_WeaponTrace() { Window = EOSFActionWindow::Active; }
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF Weapon Trace"); }
};

UCLASS(meta = (DisplayName = "OSF Invincibility Frames"))
class OPENSOULSLIKEFRAMEWORK_API UOSFANS_InvincibilityFrames : public UOSFAnimNotifyState_Window
{
	GENERATED_BODY()
public:
	UOSFANS_InvincibilityFrames() { Window = EOSFActionWindow::Invincible; }
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF I-Frames"); }
};

UCLASS(meta = (DisplayName = "OSF Hyper Armor"))
class OPENSOULSLIKEFRAMEWORK_API UOSFANS_HyperArmor : public UOSFAnimNotifyState_Window
{
	GENERATED_BODY()
public:
	UOSFANS_HyperArmor() { Window = EOSFActionWindow::HyperArmor; }
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF Hyper Armor"); }
};

/** Combo / input buffer window: buffered inputs fire, the action can be chained or cancelled. */
UCLASS(meta = (DisplayName = "OSF Input Buffer"))
class OPENSOULSLIKEFRAMEWORK_API UOSFANS_InputBuffer : public UOSFAnimNotifyState_Window
{
	GENERATED_BODY()
public:
	UOSFANS_InputBuffer() { Window = EOSFActionWindow::Cancel; }
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF Input Buffer"); }
};

UCLASS(meta = (DisplayName = "OSF Parry Window"))
class OPENSOULSLIKEFRAMEWORK_API UOSFANS_ParryWindow : public UOSFAnimNotifyState_Window
{
	GENERATED_BODY()
public:
	UOSFANS_ParryWindow() { Window = EOSFActionWindow::Parry; }
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF Parry Window"); }
};

/** Instantly adjusts a stat (e.g. consume FP mid-animation, heal on a specific frame). */
UCLASS(meta = (DisplayName = "OSF Adjust Stat"))
class OPENSOULSLIKEFRAMEWORK_API UOSFAN_AdjustStat : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF")
	float Delta = 0.f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("OSF Adjust Stat"); }
};
