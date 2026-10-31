// Copyright Open Soulslike Framework contributors. MIT License.

#include "Combat/OSFWeaponTraceComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Items/OSFWeaponActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UOSFWeaponTraceComponent::UOSFWeaponTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOSFWeaponTraceComponent::BeginTrace(const FOSFDamageSpec& Spec, float Reach, float Radius, AOSFWeaponActor* Weapon)
{
	ActiveSpec = Spec;
	ActiveReach = Reach;
	ActiveRadius = Radius;
	ActiveWeapon = Weapon;
	HitActors.Reset();
	PreviousPoints.Reset();
	bHasPrevious = false;
	bTracing = true;
	if (Weapon)
	{
		Weapon->OnTraceStateChanged(true);
	}
	TraceStep();
}

void UOSFWeaponTraceComponent::EndTrace()
{
	if (!bTracing)
	{
		return;
	}
	if (AOSFWeaponActor* Weapon = ActiveWeapon.Get())
	{
		Weapon->OnTraceStateChanged(false);
	}
	bTracing = false;
	ActiveWeapon.Reset();
	PreviousPoints.Reset();
}

void UOSFWeaponTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bTracing)
	{
		TraceStep();
	}
}

void UOSFWeaponTraceComponent::GatherBladePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (const AOSFWeaponActor* Weapon = ActiveWeapon.Get())
	{
		FVector Start, End;
		Weapon->GetBladePoints(Start, End);
		const int32 Samples = FMath::Max(2, BladeSamples);
		for (int32 Index = 0; Index < Samples; ++Index)
		{
			OutPoints.Add(FMath::Lerp(Start, End, static_cast<float>(Index) / (Samples - 1)));
		}
	}
}

void UOSFWeaponTraceComponent::TraceStep()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFWeaponTrace), false, Owner);
	if (AOSFWeaponActor* Weapon = ActiveWeapon.Get())
	{
		Params.AddIgnoredActor(Weapon);
	}
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(ActiveRadius * 0.5f);

	TArray<FHitResult> Hits;
	TArray<FVector> Points;
	GatherBladePoints(Points);

	if (Points.Num() > 0)
	{
		// Sweep every blade sample from its previous position to the current one.
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector From = (bHasPrevious && PreviousPoints.IsValidIndex(Index)) ? PreviousPoints[Index] : Points[Index];
			TArray<FHitResult> SampleHits;
			World->SweepMultiByChannel(SampleHits, From, Points[Index] + FVector(0.f, 0.f, 0.01f), FQuat::Identity, TraceChannel, Sphere, Params);
			Hits.Append(SampleHits);
			if (bDrawDebug)
			{
				DrawDebugLine(World, From, Points[Index], FColor::Red, false, 1.f, 0, 2.f);
			}
		}
		PreviousPoints = Points;
		bHasPrevious = true;
	}

	// The forward sphere always runs: it makes attacks reliable with procedural visuals and fast blades.
	{
		const FVector Start = Owner->GetActorLocation();
		const FVector End = Start + Owner->GetActorForwardVector() * ActiveReach;
		TArray<FHitResult> SphereHits;
		World->SweepMultiByChannel(SphereHits, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(ActiveRadius), Params);
		Hits.Append(SphereHits);
		if (bDrawDebug)
		{
			DrawDebugCapsule(World, (Start + End) * 0.5f, ActiveReach * 0.5f, ActiveRadius, FRotationMatrix::MakeFromZ(End - Start).ToQuat(), FColor::Orange, false, 0.5f);
		}
	}

	for (const FHitResult& Hit : Hits)
	{
		if (AActor* Victim = Hit.GetActor())
		{
			DeliverHit(Victim, Hit.ImpactPoint);
		}
	}
}

FOSFHitInfo UOSFWeaponTraceComponent::DeliverHit(AActor* Victim, const FVector& HitLocation)
{
	FOSFHitInfo Info;
	AActor* Owner = GetOwner();
	if (!Victim || Victim == Owner || HitActors.Contains(Victim))
	{
		return Info;
	}
	UOSFCombatComponent* VictimCombat = OSF::Get<UOSFCombatComponent>(Victim);
	UOSFCombatComponent* MyCombat = OSF::Get<UOSFCombatComponent>(Owner);
	if (!VictimCombat || VictimCombat->IsDead() || (MyCombat && MyCombat->IsSameTeam(Victim)))
	{
		return Info;
	}
	HitActors.Add(Victim);

	FOSFDamageEvent Event;
	Event.Spec = ActiveSpec;
	Event.Instigator = Owner;
	Event.DamageCauser = ActiveWeapon.IsValid() ? static_cast<AActor*>(ActiveWeapon.Get()) : Owner;
	Event.HitLocation = HitLocation;
	Info = VictimCombat->ReceiveDamage(Event);

	if (MyCombat)
	{
		MyCombat->OnHitLanded.Broadcast(MyCombat, Victim, Info);
	}
	if (AOSFWeaponActor* Weapon = ActiveWeapon.Get())
	{
		Weapon->OnWeaponHit(Victim, HitLocation, Info);
	}
	// A parried attack stops dealing damage.
	if (Info.Result == EOSFHitResult::Parried)
	{
		EndTrace();
	}
	return Info;
}
