// Copyright Open Soulslike Framework contributors. MIT License.

#include "Animation/OSFClipAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "Animation/AttributesRuntime.h"
#include "GameFramework/Pawn.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"

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
	ActionElapsed = 0.f;
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
		ActionElapsed += DeltaSeconds;
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
	return ActionClip.IsValid() ? FOSFClipSample{ ActionClip.Sequence, ActionTime, ActionWeight, ActionClip.bUpperBody } : FOSFClipSample();
}

namespace
{
	bool NameHas(const FString& Name, const TCHAR* Part) { return Name.Contains(Part, ESearchCase::IgnoreCase); }

	/** Component-space transforms of Bones at Time (root on the reference pose, like playback). */
	void SampleComponentSpace(const UAnimSequence* Sequence, const FReferenceSkeleton& Ref, const TArray<int32>& Bones, double Time, TMap<int32, FTransform>& Out)
	{
		Out.Reset();
		TFunction<FTransform(int32)> Resolve = [&](int32 Index) -> FTransform
		{
			if (const FTransform* Done = Out.Find(Index))
			{
				return *Done;
			}
			FTransform Local = Ref.GetRefBonePose()[Index];
			if (Index != 0)
			{
				Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(Time, false), false);
			}
			const int32 Parent = Ref.GetParentIndex(Index);
			const FTransform Result = Parent == INDEX_NONE ? Local : Local * Resolve(Parent);
			Out.Add(Index, Result);
			return Result;
		};
		for (const int32 Bone : Bones)
		{
			Resolve(Bone);
		}
	}
}

float UOSFClipAnimInstance::FindImpactTime(const FOSFAnimClip& Clip, const FVector& ComponentForward)
{
	const UAnimSequence* Sequence = Clip.Sequence;
	const float End = Clip.GetEnd();
	if (!Sequence || End <= Clip.Start)
	{
		return Clip.Start;
	}
	if (Clip.ImpactRule == EOSFClipImpact::Fixed && Clip.ImpactTime >= 0.f)
	{
		return FMath::Clamp(Clip.ImpactTime, Clip.Start, End);
	}
	const FString Key = FString::Printf(TEXT("%s|%.3f|%.3f|%d"), *Sequence->GetPathName(), Clip.Start, End, static_cast<int32>(Clip.ImpactRule));
	if (const float* Cached = ImpactCache.Find(Key))
	{
		return *Cached;
	}
	const USkeleton* Skeleton = Sequence->GetSkeleton();
	if (!Skeleton)
	{
		return (Clip.Start + End) * 0.5f;
	}
	// Skeleton bone indices are the sequence's track indices (FSkeletonPoseBoneIndex).
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	int32 Hip = INDEX_NONE;
	TArray<int32> Hands, Feet;
	for (int32 Index = 0; Index < Ref.GetNum(); ++Index)
	{
		const FString Name = Ref.GetBoneName(Index).ToString();
		if (Hip == INDEX_NONE && (Name.Equals(TEXT("hip"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("hips"), ESearchCase::IgnoreCase) || NameHas(Name, TEXT("pelvis"))))
		{
			Hip = Index;
		}
		const bool bDigit = NameHas(Name, TEXT("thumb")) || NameHas(Name, TEXT("index")) || NameHas(Name, TEXT("middle")) || NameHas(Name, TEXT("ring")) || NameHas(Name, TEXT("pinky")) || NameHas(Name, TEXT("finger"));
		if ((NameHas(Name, TEXT("hand")) || NameHas(Name, TEXT("wrist"))) && !bDigit)
		{
			Hands.Add(Index);
		}
		if ((NameHas(Name, TEXT("foot")) || NameHas(Name, TEXT("ankle"))) && !NameHas(Name, TEXT("toe")))
		{
			Feet.Add(Index);
		}
	}
	const float Step = 1.f / 30.f;
	float Best = (Clip.Start + End) * 0.5f;
	const bool bKick = NameHas(Sequence->GetName(), TEXT("kick"));
	const TArray<int32>& Limbs = (bKick && Feet.Num() > 0) ? Feet : Hands;
	if (Clip.ImpactRule == EOSFClipImpact::Reach && Hip != INDEX_NONE && Limbs.Num() > 0)
	{
		// The frame where a fist / foot reaches furthest forward from the hips.
		TArray<int32> Needed = Limbs;
		Needed.Add(Hip);
		TMap<int32, FTransform> Pose;
		float Reach = -TNumericLimits<float>::Max();
		const FVector Forward = ComponentForward.GetSafeNormal();
		for (float Time = Clip.Start; Time <= End; Time += Step)
		{
			SampleComponentSpace(Sequence, Ref, Needed, Time, Pose);
			const FVector Origin = Pose[Hip].GetLocation();
			for (const int32 Limb : Limbs)
			{
				const float Forwardness = FVector::DotProduct(Pose[Limb].GetLocation() - Origin, Forward);
				if (Forwardness > Reach)
				{
					Reach = Forwardness;
					Best = Time;
				}
			}
		}
	}
	else
	{
		// Energy: summed frame-to-frame joint rotation change, smoothed over +-4 samples.
		TArray<float> Energy;
		TArray<FQuat> Previous;
		Previous.SetNum(Ref.GetNum());
		bool bFirst = true;
		for (float Time = Clip.Start; Time <= End; Time += Step)
		{
			float Sum = 0.f;
			for (int32 Index = 1; Index < Ref.GetNum(); ++Index)
			{
				FTransform Local = Ref.GetRefBonePose()[Index];
				Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(static_cast<double>(Time), false), false);
				if (!bFirst)
				{
					Sum += static_cast<float>(Previous[Index].AngularDistance(Local.GetRotation()));
				}
				Previous[Index] = Local.GetRotation();
			}
			bFirst = false;
			Energy.Add(Sum);
		}
		float Peak = -1.f;
		for (int32 Index = 0; Index < Energy.Num(); ++Index)
		{
			float Smooth = 0.f;
			int32 Count = 0;
			for (int32 Near = FMath::Max(0, Index - 4); Near < FMath::Min(Energy.Num(), Index + 5); ++Near)
			{
				Smooth += Energy[Near];
				++Count;
			}
			Smooth /= FMath::Max(Count, 1);
			if (Smooth > Peak)
			{
				Peak = Smooth;
				Best = Clip.Start + Index * Step;
			}
		}
	}
	ImpactCache.Add(Key, Best);
	return Best;
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
		bInPlaceHips = Instance->bInPlaceHips;
		UpperBodyRootBone = Instance->UpperBodyRootBone;
	}
}

void FOSFClipAnimInstanceProxy::BuildUpperBodyMask(const FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (UpperMask.Num() == Output.Pose.GetNumBones() && UpperMaskSerial == Bones.GetSerialNumber() && UpperMaskRoot == UpperBodyRootBone)
	{
		return;
	}
	UpperMaskSerial = Bones.GetSerialNumber();
	UpperMaskRoot = UpperBodyRootBone;
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	int32 Root = UpperBodyRootBone.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(UpperBodyRootBone);
	for (int32 Index = 0; Root == INDEX_NONE && Index < Ref.GetNum(); ++Index)
	{
		if (Ref.GetBoneName(Index).ToString().Contains(TEXT("spine"), ESearchCase::IgnoreCase))
		{
			Root = Index;
		}
	}
	UpperMask.Init(0.f, Output.Pose.GetNumBones());
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		for (int32 Mesh = Bones.MakeMeshPoseIndex(Index).GetInt(); Mesh != INDEX_NONE && Root != INDEX_NONE; Mesh = Ref.GetParentIndex(Mesh))
		{
			if (Mesh == Root)
			{
				UpperMask[Index.GetInt()] = 1.f;
				break;
			}
		}
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

	// Action clip on top (upper body clips: per bone, everything below the spine keeps locomotion).
	if (Action.Sequence && Action.Weight > KINDA_SMALL_NUMBER && Action.bUpperBody)
	{
		BuildUpperBodyMask(Output);
		FPoseContext Layer(Output);
		Sample(Action, Layer);
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			const float Weight = UpperMask[Index.GetInt()] * Action.Weight;
			if (Weight > 0.f)
			{
				Output.Pose[Index].BlendWith(Layer.Pose[Index], Weight);
			}
		}
	}
	else if (Action.Sequence && Action.Weight > KINDA_SMALL_NUMBER)
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
	if (bInPlaceHips && Output.Pose.GetNumBones() > 1)
	{
		// Keep only the vertical part of the hips' motion: component "up" expressed in the root bone's frame.
		const FCompactPoseBoneIndex Root(0);
		const FVector Up = Output.Pose[Root].GetRotation().Inverse().RotateVector(FVector::UpVector);
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			if (Index.GetInt() == 0 || Output.Pose.GetParentBoneIndex(Index) != Root)
			{
				continue;
			}
			const FVector RefLocation = Output.Pose.GetRefPose(Index).GetLocation();
			const FVector Delta = Output.Pose[Index].GetLocation() - RefLocation;
			Output.Pose[Index].SetLocation(RefLocation + Up * FVector::DotProduct(Delta, Up));
		}
	}
	return true;
}
