// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFWeaponActor.h"
#include "Items/OSFItemData.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AOSFWeaponActor::AOSFWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CastShadow = true;
}

void AOSFWeaponActor::InitializeWeapon(UOSFWeaponData* InData)
{
	WeaponData = InData;
	if (InData && InData->StaticMesh)
	{
		Mesh->SetStaticMesh(InData->StaticMesh);
		Mesh->SetRelativeTransform(InData->MeshTransform);
	}
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
