// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFWeaponActor.h"
#include "Items/OSFItemData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

AOSFWeaponActor::AOSFWeaponActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CastShadow = true;
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(Root);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMesh->SetGenerateOverlapEvents(false);
	SkeletalMesh->SetCanEverAffectNavigation(false);
	SkeletalMesh->SetVisibility(false);
}

void AOSFWeaponActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAlignToOwnerFacing && GetOwner())
	{
		SetActorRotation(FRotator(0.f, GetOwner()->GetActorRotation().Yaw, 0.f) + FacingOffset);
	}
}

void AOSFWeaponActor::InitializeWeapon(UOSFWeaponData* InData)
{
	WeaponData = InData;
	if (InData && InData->WeaponSkeletalMesh)
	{
		SkeletalMesh->SetSkeletalMesh(InData->WeaponSkeletalMesh);
		SkeletalMesh->SetRelativeTransform(InData->MeshTransform);
		SkeletalMesh->SetVisibility(true);
		Mesh->SetStaticMesh(nullptr);
	}
	else if (InData && InData->StaticMesh)
	{
		Mesh->SetStaticMesh(InData->StaticMesh);
		Mesh->SetRelativeTransform(InData->MeshTransform);
	}
	bBarrelCached = false;
}

void AOSFWeaponActor::ApplyBoneSpaceScale(float HolderScale)
{
	if (!WeaponData)
	{
		return;
	}
	FTransform Scaled = WeaponData->MeshTransform;
	Scaled.SetTranslation(Scaled.GetTranslation() * HolderScale);
	Scaled.SetScale3D(Scaled.GetScale3D() * HolderScale);
	(WeaponData->WeaponSkeletalMesh ? static_cast<USceneComponent*>(SkeletalMesh) : static_cast<USceneComponent*>(Mesh))->SetRelativeTransform(Scaled);
	bBarrelCached = false;
}

void AOSFWeaponActor::CacheBarrel()
{
	bBarrelCached = true;
	BarrelLocal = FVector::ForwardVector;
	MuzzleLocal = FVector(WeaponData ? WeaponData->BladeLength : 100.f, 0.f, 0.f);
	const UStaticMesh* StaticMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (!StaticMesh)
	{
		return;
	}
	// Longest side of the mesh, signed so it runs from the hand (the actor origin, in mesh space) toward the far end.
	const FBox Box = StaticMesh->GetBoundingBox();
	const FVector Size = Box.GetSize();
	const int32 Axis = (Size.X >= Size.Y && Size.X >= Size.Z) ? 0 : (Size.Y >= Size.Z ? 1 : 2);
	const FVector HandLocal = Mesh->GetRelativeTransform().Inverse().TransformPosition(FVector::ZeroVector);
	const bool bPositive = (Box.Max[Axis] - HandLocal[Axis]) >= (HandLocal[Axis] - Box.Min[Axis]);
	BarrelLocal = FVector::ZeroVector;
	BarrelLocal[Axis] = bPositive ? 1.f : -1.f;
	MuzzleLocal = Box.GetCenter();
	MuzzleLocal[Axis] = bPositive ? Box.Max[Axis] : Box.Min[Axis];
}

FVector AOSFWeaponActor::GetBarrelDirection() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return GetActorForwardVector();
	}
	if (!bBarrelCached)
	{
		const_cast<AOSFWeaponActor*>(this)->CacheBarrel();
	}
	return Mesh->GetComponentTransform().TransformVectorNoScale(BarrelLocal).GetSafeNormal();
}

FVector AOSFWeaponActor::GetBarrelInActorSpace() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return FVector::ForwardVector;
	}
	if (!bBarrelCached)
	{
		const_cast<AOSFWeaponActor*>(this)->CacheBarrel();
	}
	return Mesh->GetRelativeTransform().TransformVectorNoScale(BarrelLocal).GetSafeNormal();
}

FVector AOSFWeaponActor::GetMuzzleLocation() const
{
	if (Mesh && Mesh->GetStaticMesh() && Mesh->DoesSocketExist(TEXT("Muzzle")))
	{
		return Mesh->GetSocketLocation(TEXT("Muzzle"));
	}
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return GetActorTransform().TransformPosition(MuzzleLocal);
	}
	if (!bBarrelCached)
	{
		const_cast<AOSFWeaponActor*>(this)->CacheBarrel();
	}
	return Mesh->GetComponentTransform().TransformPosition(MuzzleLocal);
}

void AOSFWeaponActor::GetBladePoints(FVector& OutStart, FVector& OutEnd) const
{
	if (Mesh && Mesh->GetStaticMesh() && Mesh->DoesSocketExist(TraceStartSocket) && Mesh->DoesSocketExist(TraceEndSocket))
	{
		OutStart = Mesh->GetSocketLocation(TraceStartSocket);
		OutEnd = Mesh->GetSocketLocation(TraceEndSocket);
		return;
	}
	const float Length = WeaponData ? WeaponData->BladeLength : 100.f;
	OutStart = GetActorLocation() + GetActorForwardVector() * (Length * 0.15f);
	OutEnd = GetActorLocation() + GetActorForwardVector() * Length;
}

void AOSFWeaponActor::OnWeaponHit_Implementation(AActor* Victim, const FVector& Location, const FOSFHitInfo& HitInfo)
{
	if (WeaponData && WeaponData->ImpactSound && HitInfo.Result != EOSFHitResult::Ignored)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->ImpactSound, Location);
	}
}
