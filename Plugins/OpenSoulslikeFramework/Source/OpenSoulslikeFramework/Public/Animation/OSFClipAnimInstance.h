// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "OSFTypes.h"
#include "OSFClipAnimInstance.generated.h"

/** Snapshot of one sampled clip, copied to the worker thread every frame. */
struct FOSFClipSample
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bUpperBody = false;
};

/** Evaluates the clip layers without an anim graph: idle / walk / run blended by speed, plus an action clip on top. */
struct OPENSOULSLIKEFRAMEWORK_API FOSFClipAnimInstanceProxy : public FAnimInstanceProxy
{
	FOSFClipAnimInstanceProxy() = default;
	explicit FOSFClipAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	TArray<FOSFClipSample, TInlineAllocator<4>> Locomotion;
	FOSFClipSample Action;
	bool bLockRoot = true;
	bool bInPlaceHips = true;
	FName UpperBodyRootBone;

private:
	static void Sample(const FOSFClipSample& Clip, FPoseContext& Output);
	void BuildUpperBodyMask(const FPoseContext& Output);

	/** Per compact bone: 1 = upper body. Rebuilt when the required bones change. */
	TArray<float> UpperMask;
	uint16 UpperMaskSerial = 0;
	FName UpperMaskRoot;
};

/**
 * Anim instance that plays trimmed clips (FOSFAnimClip) directly - the "no anim blueprint" path for imported
 * characters. Locomotion blends Idle / Walk / Run by ground speed; actions call PlayActionClip and the clip
 * blends in on top, then back out to locomotion. AOSFCharacterBase uses it automatically when a skeletal mesh
 * has no anim class and LocomotionClips are set.
 */
UCLASS(Transient, NotBlueprintable)
class OPENSOULSLIKEFRAMEWORK_API UOSFClipAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	FOSFAnimClip Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	FOSFAnimClip Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	FOSFAnimClip Run;

	/** Ground speed (cm/s) at which Walk plays at its own rate; also the idle -> walk blend end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	float WalkSpeed = 220.f;

	/** Ground speed at which Run is fully blended in and plays at its own rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	float RunSpeed = 520.f;

	/**
	 * Keeps the root bone on the reference pose. Clips are expected to be in place (movement is gameplay driven);
	 * this also drops per-clip root rotations, e.g. glTF imports whose root track still carries the source axis
	 * conversion or a facing offset, which would otherwise tip or turn the whole character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	bool bLockRootToRefPose = true;

	/**
	 * In-place clips: the bones directly under the root (hips) keep only their vertical motion, like the
	 * platform preview's stripRootMotion - stepping kicks and lunges don't slide the body off the capsule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	bool bInPlaceHips = true;

	/** First bone of the upper body for bUpperBody clips. None = first bone whose name contains "spine". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Clips")
	FName UpperBodyRootBone;

	/**
	 * Hit frame of a clip (sequence time) by its ImpactRule, from the skeleton's bone tracks:
	 * Reach = a hand / foot (kick clips) / held weapon furthest forward from the hips along ComponentForward,
	 * Energy = peak of summed joint rotation speed. Cached per clip.
	 */
	float FindImpactTime(const FOSFAnimClip& Clip, const FVector& ComponentForward);

	/** True while Sequence is the playing action clip. */
	bool IsPlayingSequence(const UAnimSequence* Sequence) const { return ActionClip.Sequence == Sequence && ActionWeightTarget > 0.f; }

	/** Seconds since the action clip started (real time). */
	float GetActionElapsed() const { return ActionElapsed; }
	/** Plays a one-shot (or looping) clip over locomotion. Duration > 0 retimes the clip to last that long. */
	UFUNCTION(BlueprintCallable, Category = "OSF|Clips")
	void PlayActionClip(const FOSFAnimClip& Clip, float Duration = -1.f);

	/** Blends the action clip out (BlendTime < 0 uses the clip's BlendOut). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Clips")
	void StopActionClip(float BlendTime = -1.f);

	/** Freezes the action clip on its current frame (death pose). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Clips")
	void HoldActionClip(bool bHold) { bHoldAction = bHold; }

	UFUNCTION(BlueprintPure, Category = "OSF|Clips")
	bool IsPlayingActionClip() const { return ActionClip.IsValid() && ActionWeightTarget > 0.f; }

	void SetLocomotion(const FOSFAnimClip& InIdle, const FOSFAnimClip& InWalk, const FOSFAnimClip& InRun);

	// Game-thread state read by the proxy.
	TArray<FOSFClipSample, TInlineAllocator<4>> GetLocomotionSamples() const;
	FOSFClipSample GetActionSample() const;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FOSFClipAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }

	static float Advance(const FOSFAnimClip& Clip, float Time, float DeltaSeconds, float RateScale);

	FOSFAnimClip ActionClip;
	float ActionTime = 0.f;
	float ActionElapsed = 0.f;
	TMap<FString, float> ImpactCache;
	float ActionRateScale = 1.f;
	float ActionWeight = 0.f;
	float ActionWeightTarget = 0.f;
	float ActionBlendOut = 0.2f;
	bool bHoldAction = false;

	float IdleTime = 0.f;
	float WalkTime = 0.f;
	float RunTime = 0.f;
	float Speed = 0.f;
};
