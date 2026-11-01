// Copyright Open Soulslike Framework contributors. MIT License.

#include "Animation/OSFClipAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
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
	// A "run" that is the walk clip again would blend the cycle with itself at another rate (scrambled legs):
	// the walk alone speeds up instead.
	Run = (InRun.Sequence == InWalk.Sequence) ? FOSFAnimClip() : InRun;
	IdleTime = Idle.Start;
	WalkTime = Walk.Start;
	RunTime = Run.Start;
	WalkStride = bMeasureStrideSpeed ? MeasureStrideSpeed(Walk) : 0.f;
	RunStride = bMeasureStrideSpeed ? MeasureStrideSpeed(Run) : 0.f;
}

FVector2D UOSFClipAnimInstance::GetLocomotionSpeeds() const
{
	const USkeletalMeshComponent* Component = GetSkelMeshComponent();
	const float Scale = Component ? FMath::Abs(Component->GetComponentScale().X) : 1.f;
	const float WalkGround = WalkStride > 0.f ? WalkStride * Scale : WalkSpeed;
	float RunGround = RunStride > 0.f ? RunStride * Scale : RunSpeed;
	// Same clip (or a run no faster than the walk): keep a blend range so speed still picks a rate.
	if (RunGround < WalkGround * 1.2f)
	{
		RunGround = FMath::Max(RunSpeed, WalkGround * 1.6f);
	}
	return FVector2D(WalkGround, RunGround);
}

void UOSFClipAnimInstance::PlayActionClip(const FOSFAnimClip& Clip, float Duration, bool bHoldAtEnd)
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
	bHoldActionAtEnd = bHoldAtEnd && !Clip.bLoop;
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
	bHoldActionAtEnd = false;
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
	// Walk / run cycles play at the rate that matches the ground speed, so planted feet stay put.
	const FVector2D Ground = GetLocomotionSpeeds();
	WalkTime = Advance(Walk, WalkTime, DeltaSeconds, FMath::Clamp(Speed / FMath::Max(Ground.X, 1.f), 0.35f, 2.5f));
	RunTime = Advance(Run, RunTime, DeltaSeconds, FMath::Clamp(Speed / FMath::Max(Ground.Y, 1.f), 0.5f, 2.f));

	if (ActionClip.IsValid())
	{
		ActionElapsed += DeltaSeconds;
		if (!bHoldAction)
		{
			ActionTime = Advance(ActionClip, ActionTime, DeltaSeconds, ActionRateScale);
			// One-shot clips blend out by themselves at their end (or freeze there, e.g. a death).
			if (!ActionClip.bLoop && ActionTime >= ActionClip.GetEnd() - KINDA_SMALL_NUMBER && ActionWeightTarget > 0.f)
			{
				bHoldActionAtEnd ? HoldActionClip(true) : StopActionClip();
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
	const FVector2D Ground = GetLocomotionSpeeds();
	const float WalkAlpha = Walk.IsValid() ? FMath::Clamp(Speed / FMath::Max(Ground.X, 1.f), 0.f, 1.f) : 0.f;
	const float RunAlpha = Run.IsValid() ? FMath::Clamp((Speed - Ground.X) / FMath::Max(Ground.Y - Ground.X, 1.f), 0.f, 1.f) : 0.f;
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

float UOSFClipAnimInstance::MeasureStrideSpeed(const FOSFAnimClip& Clip)
{
	const UAnimSequence* Sequence = Clip.Sequence;
	const float End = Clip.GetEnd();
	const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!Skeleton || End - Clip.Start < 0.2f)
	{
		return 0.f;
	}
	static TMap<FString, float> Cache;
	const FString Key = FString::Printf(TEXT("%s|%.3f|%.3f"), *Sequence->GetPathName(), Clip.Start, End);
	if (const float* Cached = Cache.Find(Key))
	{
		return *Cached;
	}
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	int32 Hip = INDEX_NONE;
	TArray<int32> Feet;
	for (int32 Index = 1; Index < Ref.GetNum(); ++Index)
	{
		const FString Name = Ref.GetBoneName(Index).ToString();
		if (Hip == INDEX_NONE && Ref.GetParentIndex(Index) == 0)
		{
			Hip = Index;	// playback keeps only the vertical motion of the bones under the root
		}
		if ((NameHas(Name, TEXT("foot")) || NameHas(Name, TEXT("ankle"))) && !NameHas(Name, TEXT("toe")) && !NameHas(Name, TEXT("end")))
		{
			Feet.Add(Index);
		}
	}
	float Result = 0.f;
	// Clips with root motion (e.g. IK-retargeted Mixamo cycles) state their ground speed exactly: root travel / time.
	{
		FTransform RootStart = Ref.GetRefBonePose()[0], RootEnd = RootStart;
		Sequence->GetBoneTransform(RootStart, FSkeletonPoseBoneIndex(0), FAnimExtractContext(static_cast<double>(Clip.Start), false), false);
		Sequence->GetBoneTransform(RootEnd, FSkeletonPoseBoneIndex(0), FAnimExtractContext(static_cast<double>(End), false), false);
		const float RootSpeed = static_cast<float>(FVector::Dist2D(RootStart.GetLocation(), RootEnd.GetLocation())) / (End - Clip.Start);
		if (RootSpeed > 5.f)
		{
			Result = RootSpeed * FMath::Max(Clip.PlayRate, 0.05f);
		}
	}
	if (Result <= 0.f && Hip != INDEX_NONE && Feet.Num() > 0)
	{
		// In-place cycle: foot paths relative to the hips (horizontal), as the in-place playback shows them.
		const float Step = 1.f / 60.f;
		TArray<int32> Needed = Feet;
		Needed.Add(Hip);
		TArray<TArray<FVector>> Paths;
		Paths.SetNum(Feet.Num());
		TMap<int32, FTransform> Pose;
		for (float Time = Clip.Start; Time < End; Time += Step)
		{
			SampleComponentSpace(Sequence, Ref, Needed, Time, Pose);
			const FVector HipLocation = Pose[Hip].GetLocation();
			for (int32 Foot = 0; Foot < Feet.Num(); ++Foot)
			{
				const FVector Location = Pose[Feet[Foot]].GetLocation();
				Paths[Foot].Add(FVector(Location.X - HipLocation.X, Location.Y - HipLocation.Y, Location.Z));
			}
		}
		// Planted = the lowest fifth of the foot's height range; average the foot's slide speed there.
		double Sum = 0.0;
		int32 Count = 0;
		for (const TArray<FVector>& Path : Paths)
		{
			float MinZ = TNumericLimits<float>::Max();
			float MaxZ = -TNumericLimits<float>::Max();
			for (const FVector& P : Path) { MinZ = FMath::Min(MinZ, P.Z); MaxZ = FMath::Max(MaxZ, P.Z); }
			if (MaxZ - MinZ < 2.f)
			{
				continue;	// the foot never lifts: not a stepping cycle
			}
			const float Planted = MinZ + FMath::Max(0.2f * (MaxZ - MinZ), 1.f);
			for (int32 Index = 1; Index < Path.Num(); ++Index)
			{
				if (Path[Index].Z <= Planted && Path[Index - 1].Z <= Planted)
				{
					Sum += FVector::Dist2D(Path[Index], Path[Index - 1]) / Step;
					++Count;
				}
			}
		}
		// Per second of sequence time; the clip plays PlayRate sequence-seconds per second.
		Result = Count > 3 ? static_cast<float>(Sum / Count) * FMath::Max(Clip.PlayRate, 0.05f) : 0.f;
	}
	Cache.Add(Key, Result);
	return Result;
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
	// IK-retargeted clips can carry the pelvis on the root bone (root ~1 m up with the pelvis rotation, hips below it):
	// locking the root would sink the body and drop e.g. a death fall's tip-over, so the bones under the root keep
	// their component-space pose (the in-place pass below still strips their horizontal travel). Imported clips whose
	// root stays at ground height only get the root locked (their root rotation is an axis / facing artefact).
	if (bLockRoot && Output.Pose.GetNumBones() > 0)
	{
		const FCompactPoseBoneIndex Root(0);
		const FTransform AnimRoot = Output.Pose[Root];
		const FTransform& RefRoot = Output.Pose.GetRefPose(Root);
		Output.Pose[Root] = RefRoot;
		if (FMath::Abs(AnimRoot.GetLocation().Z - RefRoot.GetLocation().Z) > 0.5)
		{
			for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
			{
				if (Index.GetInt() != 0 && Output.Pose.GetParentBoneIndex(Index) == Root)
				{
					Output.Pose[Index] = (Output.Pose[Index] * AnimRoot).GetRelativeTransform(RefRoot);
				}
			}
		}
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
