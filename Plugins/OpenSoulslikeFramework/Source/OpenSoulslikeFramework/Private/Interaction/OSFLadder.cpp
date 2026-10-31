// Copyright Open Soulslike Framework contributors. MIT License.

#include "Interaction/OSFLadder.h"
#include "Characters/OSFCharacterBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "OSFLadder"

AOSFLadder::AOSFLadder()
{
	Prompt = LOCTEXT("Climb", "Climb ladder");
	Color = FLinearColor(0.3f, 0.22f, 0.15f);

	// Rails use the base mesh; rungs are instanced.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rungs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rungs"));
	Rungs->SetupAttachment(Root);
	Rungs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rungs->SetStaticMesh(Mesh->GetStaticMesh());

	InteractionVolume->SetSphereRadius(130.f);
	InteractionVolume->SetRelativeLocation(FVector(-60.f, 0.f, 60.f));

	TopVolume = CreateDefaultSubobject<USphereComponent>(TEXT("TopVolume"));
	TopVolume->SetupAttachment(Root);
	TopVolume->SetSphereRadius(130.f);
	TopVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AOSFLadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// One cube stretched as both rails, rungs every RungSpacing.
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Mesh->SetRelativeScale3D(FVector(0.08f, 0.9f, Height / 100.f));
	Mesh->SetVisibility(false);

	Rungs->ClearInstances();
	for (const float Side : { -40.f, 40.f })
	{
		Rungs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-5.f, Side, Height * 0.5f), FVector(0.08f, 0.08f, Height / 100.f)));
	}
	for (float Z = RungSpacing; Z < Height; Z += RungSpacing)
	{
		Rungs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-5.f, 0.f, Z), FVector(0.06f, 0.8f, 0.05f)));
	}
	TopVolume->SetRelativeLocation(FVector(80.f, 0.f, Height + 60.f));
}

FVector AOSFLadder::GetClimbLocation(float AtHeight, float CapsuleHalfHeight) const
{
	return GetActorLocation() - GetActorForwardVector() * 50.f + FVector(0.f, 0.f, FMath::Clamp(AtHeight, 0.f, Height) + CapsuleHalfHeight);
}

FRotator AOSFLadder::GetClimbRotation() const
{
	return FRotator(0.f, GetActorRotation().Yaw, 0.f);
}

FVector AOSFLadder::GetTopExitLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * 90.f + FVector(0.f, 0.f, Height + 5.f);
}

FVector AOSFLadder::GetBottomExitLocation() const
{
	return GetActorLocation() - GetActorForwardVector() * 90.f + FVector(0.f, 0.f, 5.f);
}

bool AOSFLadder::IsInteractorAtTop(const AActor* Interactor) const
{
	return Interactor && Interactor->GetActorLocation().Z > GetActorLocation().Z + Height * 0.5f;
}

bool AOSFLadder::CanInteract_Implementation(AActor* Interactor) const
{
	const AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(Interactor);
	return Character && !Character->GetCurrentLadder();
}

void AOSFLadder::Interact_Implementation(AActor* Interactor)
{
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(Interactor))
	{
		Character->EnterLadder(this, IsInteractorAtTop(Interactor));
	}
}

FText AOSFLadder::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return IsInteractorAtTop(Interactor) ? LOCTEXT("ClimbDown", "Climb down") : LOCTEXT("ClimbUp", "Climb up");
}

#undef LOCTEXT_NAMESPACE
