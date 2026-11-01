// Copyright Open Soulslike Framework contributors. MIT License.

#include "Animation/OSFClipAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "Animation/AttributesRuntime.h"
#include "GameFramework/Pawn.h"

float FOSFAnimClip::GetEnd() const
{
	const float Length = Sequence ? Sequence->GetPlayLength() : 0.f;
	return (End > Start) ? FMath::Min(End, Length) : Length;
}

float FOSFAnimClip::GetDuration() const
{
	return FMath::Max(0.f, GetEnd() - Start) / FMath::Max(PlayRate, 0.05f);
}

// --- Game thread ------------------------------------------------------------------

void UOSFClipAnimInstance::SetLocomotion(const FOSFAnimClip& InIdle, const FOSFAnimClip& InWalk, const FOSFAnimClip& InRun)
{
	Idle = InIdle;
	Walk = InWalk;
	Run = InRun;
	IdleTime = Idle.Start;
	WalkTime = Walk.Start;
	RunTime = Run.Start;
}

void UOSFClipAnimInstance::PlayActionClip(const FOSFAnimClip& Clip, float Duration)
{
	if (!Clip.IsValid())
	{
		return;
	}
	ActionClip = Clip;
	ActionTime = Clip.Start;
	const float Natural = Clip.GetDuration();
	ActionRateScale = (Duration > KINDA_SMALL_NUMBER && Natural > KINDA_SMALL_NUMBER) ? Natural / Duration : 1.f;
	ActionWeightTarget = 1.f;
	ActionBlendOut = Clip.BlendOut;
	bHoldAction = false;
	if (Clip.BlendIn <= 0.f)
	{
		ActionWeight = 1.f;
	}
}

void UOSFClipAnimInstance::StopActionClip(float BlendTime)
{
	if (BlendTime >= 0.f)
	{
		ActionBlendOut = BlendTime;
	}
	ActionWeightTarget = 0.f;
	bHoldAction = false;
	if (ActionBlendOut <= 0.f)
	{
		ActionWeight = 0.f;
	}
}

float UOSFClipAnimInstance::Advance(const FOSFAnimClip& Clip, float Time, float DeltaSeconds, float RateScale)
{
	if (!Clip.IsValid())
	{
		return Time;
	}
	const float End = Clip.GetEnd();
	const float Length = End - Clip.Start;
	Time += DeltaSeconds * Clip.PlayRate * RateScale;
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return Clip.Start;
	}
	if (Clip.bLoop)
	{
		return Clip.Start + FMath::Fmod(FMath::Max(Time - Clip.Start, 0.f), Length);
	}
	return FMath::Clamp(Time, Clip.Start, End);
}

void UOSFClipAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;

	IdleTime = Advance(Idle, IdleTime, DeltaSeconds, 1.f);
	// Walk / run cycles follow the ground speed so feet don't slide too much.
	WalkTime = Advance(Walk, WalkTime, DeltaSeconds, FMath::Clamp(Speed / FMath::Max(WalkSpeed, 1.f), 0.6f, 1.6f));
	RunTime = Advance(Run, RunTime, DeltaSeconds, FMath::Clamp(Speed / FMath::Max(RunSpeed, 1.f), 0.7f, 1.4f));

	if (ActionClip.IsValid())
	{
		if (!bHoldAction)
		{
			ActionTime = Advance(ActionClip, ActionTime, DeltaSeconds, ActionRateScale);
			// One-shot clips blend out by themselves at their end.
			if (!ActionClip.bLoop && ActionTime >= ActionClip.GetEnd() - KINDA_SMALL_NUMBER && ActionWeightTarget > 0.f)
			{
				StopActionClip();
			}
		}
		const float BlendTime = ActionWeightTarget > ActionWeight ? ActionClip.BlendIn : ActionBlendOut;
		ActionWeight = BlendTime > 0.f ? FMath::FInterpConstantTo(ActionWeight, ActionWeightTarget, DeltaSeconds, 1.f / BlendTime) : ActionWeightTarget;
		if (ActionWeight <= 0.f && ActionWeightTarget <= 0.f)
		{
			ActionClip = FOSFAnimClip();
		}
	}
}

TArray<FOSFClipSample, TInlineAllocator<4>> UOSFClipAnimInstance::GetLocomotionSamples() const
{
	TArray<FOSFClipSample, TInlineAllocator<4>> Out;
	const float WalkAlpha = Walk.IsValid() ? FMath::Clamp(Speed / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f) : 0.f;
	const float RunAlpha = Run.IsValid() ? FMath::Clamp((Speed - WalkSpeed) / FMath::Max(RunSpeed - WalkSpeed, 1.f), 0.f, 1.f) : 0.f;
	if (Idle.IsValid() && WalkAlpha < 1.f)
	{
		Out.Add({ Idle.Sequence, IdleTime, 1.f - WalkAlpha });
	}
	if (Walk.IsValid() && WalkAlpha > 0.f && RunAlpha < 1.f)
	{
		Out.Add({ Walk.Sequence, WalkTime, WalkAlpha * (1.f - RunAlpha) });
	}
	if (Run.IsValid() && RunAlpha > 0.f)
	{
		Out.Add({ Run.Sequence, RunTime, WalkAlpha * RunAlpha });
	}
	return Out;
}

FOSFClipSample UOSFClipAnimInstance::GetActionSample() const
{
	return ActionClip.IsValid() ? FOSFClipSample{ ActionClip.Sequence, ActionTime, ActionWeight } : FOSFClipSample();
}

// --- Worker thread ----------------------------------------------------------------

void FOSFClipAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UOSFClipAnimInstance* Instance = Cast<UOSFClipAnimInstance>(InAnimInstance))
	{
		Locomotion = Instance->GetLocomotionSamples();
		Action = Instance->GetActionSample();
		bLockRoot = Instance->bLockRootToRefPose;
	}
}

void FOSFClipAnimInstanceProxy::Sample(const FOSFClipSample& Clip, FPoseContext& Output)
{
	FAnimationPoseData PoseData(Output);
	Clip.Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Clip.Time), false));
}

bool FOSFClipAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// Locomotion: weighted blend of up to three cycles.
	bool bHasPose = false;
	float AccumulatedWeight = 0.f;
	for (const FOSFClipSample& Clip : Locomotion)
	{
		if (!Clip.Sequence || Clip.Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (!bHasPose)
		{
			Sample(Clip, Output);
			bHasPose = true;
			AccumulatedWeight = Clip.Weight;
			continue;
		}
		FPoseContext Layer(Output);
		Sample(Clip, Layer);
		const float NewTotal = AccumulatedWeight + Clip.Weight;
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, FAnimationPoseData(Layer), AccumulatedWeight / NewTotal);
		AccumulatedWeight = NewTotal;
	}
	if (!bHasPose)
	{
		Output.ResetToRefPose();
	}

	// Action clip on top.
	if (Action.Sequence && Action.Weight > KINDA_SMALL_NUMBER)
	{
		if (Action.Weight >= 1.f - KINDA_SMALL_NUMBER)
		{
			Sample(Action, Output);
		}
		else
		{
			FPoseContext Layer(Output);
			Sample(Action, Layer);
			FAnimationPoseData OutData(Output);
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, FAnimationPoseData(Layer), 1.f - Action.Weight);
		}
	}
	if (bLockRoot && Output.Pose.GetNumBones() > 0)
	{
		const FCompactPoseBoneIndex Root(0);
		Output.Pose[Root] = Output.Pose.GetRefPose(Root);
	}
	return true;
}
