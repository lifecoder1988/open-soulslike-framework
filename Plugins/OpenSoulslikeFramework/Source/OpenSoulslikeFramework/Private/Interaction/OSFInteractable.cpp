// Copyright Open Soulslike Framework contributors. MIT License.

#include "Interaction/OSFInteractable.h"
#include "Progression/OSFProgressComponent.h"
#include "OSFTypes.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AOSFInteractableBase::AOSFInteractableBase()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Root);
	InteractionVolume->SetSphereRadius(160.f);
	InteractionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionVolume->SetGenerateOverlapEvents(true);
	InteractionVolume->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AOSFInteractableBase::BeginPlay()
{
	Super::BeginPlay();
	SetMeshColor(Color);
}

void AOSFInteractableBase::SetMeshColor(const FLinearColor& InColor)
{
	if (!MeshMaterial && Mesh && Mesh->GetStaticMesh())
	{
		MeshMaterial = Mesh->CreateDynamicMaterialInstance(0);
	}
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), InColor);
	}
}

FName AOSFInteractableBase::GetSaveId() const
{
	return SaveId.IsNone() ? GetFName() : SaveId;
}

FName AOSFInteractableBase::GetPersistentFlag() const
{
	return FName(*FString::Printf(TEXT("%s.%s"), *GetFlagPrefix(), *GetSaveId().ToString()));
}

UOSFProgressComponent* AOSFInteractableBase::GetProgress(AActor* Interactor)
{
	return OSF::Get<UOSFProgressComponent>(Interactor);
}
