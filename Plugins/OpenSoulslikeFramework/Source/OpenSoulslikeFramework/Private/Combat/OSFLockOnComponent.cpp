// Copyright Open Soulslike Framework contributors. MIT License.

#include "Combat/OSFLockOnComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UOSFLockOnComponent::UOSFLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

FVector UOSFLockOnComponent::GetViewLocation(FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			OutRotation = PC->PlayerCameraManager->GetCameraRotation();
			return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	OutRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UOSFLockOnComponent::GetCandidates(TArray<AActor*>& OutCandidates) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	const UOSFCombatComponent* MyCombat = OSF::Get<UOSFCombatComponent>(Owner);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFLockOn), false, Owner);
	World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AcquireRadius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(Candidate);
		if (Candidate && Combat && !Combat->IsDead() && !(MyCombat && MyCombat->IsSameTeam(Candidate)))
		{
			OutCandidates.AddUnique(Candidate);
		}
	}
}

bool UOSFLockOnComponent::HasLineOfSight(const AActor* Candidate) const
{
	FRotator ViewRotation;
	const FVector ViewLocation = GetViewLocation(ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFLockOnLOS), false, GetOwner());
	Params.AddIgnoredActor(Candidate);
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Candidate->GetActorLocation(), ECC_Visibility, Params);
}

AActor* UOSFLockOnComponent::FindBestTarget(const AActor* Exclude, float SideBias) const
{
	TArray<AActor*> Candidates;
	GetCandidates(Candidates);
	FRotator ViewRotation;
	const FVector ViewLocation = GetViewLocation(ViewRotation);
	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal2D();
	const FVector ViewRight = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y).GetSafeNormal2D();

	AActor* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (AActor* Candidate : Candidates)
	{
		if (Candidate == Exclude || !HasLineOfSight(Candidate))
		{
			continue;
		}
		const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
		const FVector Dir = ToCandidate.GetSafeNormal2D();
		const float Facing = FVector::DotProduct(ViewForward, Dir);
		if (Facing < 0.2f)
		{
			continue;
		}
		const float Side = FVector::DotProduct(ViewRight, Dir);
		if (SideBias != 0.f && Side * SideBias <= 0.05f)
		{
			continue;
		}
		const float Distance = ToCandidate.Size();
		float Score = Facing * 2.f - Distance / AcquireRadius;
		if (SideBias != 0.f)
		{
			Score = -FMath::Abs(Side);
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void UOSFLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}
	if (AActor* Best = FindBestTarget(nullptr, 0.f))
	{
		LockOn(Best);
	}
	else if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		// No target: recenter the camera behind the character (souls behaviour).
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(FRotator(-10.f, Pawn->GetActorRotation().Yaw, 0.f));
		}
	}
}

bool UOSFLockOnComponent::LockOn(AActor* NewTarget)
{
	if (!NewTarget || !OSF::Get<UOSFCombatComponent>(NewTarget))
	{
		return false;
	}
	Target = NewTarget;
	LostSightTime = 0.f;
	OnLockOnChanged.Broadcast(NewTarget);
	return true;
}

void UOSFLockOnComponent::ClearLockOn()
{
	if (Target.IsValid())
	{
		Target.Reset();
		OnLockOnChanged.Broadcast(nullptr);
	}
}

void UOSFLockOnComponent::SwitchTarget(float Direction)
{
	if (!IsLockedOn() || FMath::IsNearlyZero(Direction))
	{
		return;
	}
	if (AActor* Next = FindBestTarget(Target.Get(), FMath::Sign(Direction)))
	{
		LockOn(Next);
	}
}

FVector UOSFLockOnComponent::GetTargetLocation() const
{
	const AActor* Current = Target.Get();
	if (!Current)
	{
		return FVector::ZeroVector;
	}
	return Current->GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

void UOSFLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Current = Target.Get();
	if (!Current)
	{
		return;
	}
	const UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(Current);
	const float Distance = FVector::Dist(Current->GetActorLocation(), GetOwner()->GetActorLocation());
	if (!Combat || Combat->IsDead() || Distance > BreakDistance)
	{
		ClearLockOn();
		return;
	}
	LostSightTime = HasLineOfSight(Current) ? 0.f : LostSightTime + DeltaTime;
	if (LostSightTime > LostSightTimeout)
	{
		ClearLockOn();
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (Controller)
	{
		const FVector From = Pawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		FRotator Desired = (GetTargetLocation() - From).Rotation();
		Desired.Pitch = FMath::Clamp(Desired.Pitch + CameraPitchOffset, -45.f, 20.f);
		const FRotator NewRotation = FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, CameraInterpSpeed);
		Controller->SetControlRotation(NewRotation);
	}
}
