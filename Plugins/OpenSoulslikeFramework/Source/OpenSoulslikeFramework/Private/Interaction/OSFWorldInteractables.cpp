// Copyright Open Soulslike Framework contributors. MIT License.

#include "Interaction/OSFWorldInteractables.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Characters/OSFPlayerCharacter.h"
#include "AI/OSFEnemyCharacter.h"
#include "AI/OSFBossComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Progression/OSFProgressComponent.h"
#include "OSFGameplayTags.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "OSFInteractables"

// ============================================================================
// Resting point
// ============================================================================

AOSFRestingPoint::AOSFRestingPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	Prompt = LOCTEXT("Rest", "Rest");
	DisplayName = LOCTEXT("RestingPoint", "Resting Point");
	Color = FLinearColor(0.15f, 0.12f, 0.1f);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.25f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Root);
	Flame->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Flame->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.8f));
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ConeMesh.Succeeded())
	{
		Flame->SetStaticMesh(ConeMesh.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Light->SetIntensity(6000.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
}

void AOSFRestingPoint::UpdateVisuals()
{
	Light->SetIntensity(bDiscovered ? 8000.f : 600.f);
	Flame->SetVisibility(bDiscovered);
	if (UMaterialInstanceDynamic* FlameMaterial = Flame->CreateDynamicMaterialInstance(0))
	{
		FlameMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.5f, 0.1f));
	}
}

void AOSFRestingPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDiscovered)
	{
		const float Time = static_cast<float>(GetWorld()->GetTimeSeconds());
		Light->SetIntensity(7000.f + FMath::Sin(Time * 9.f) * 900.f + FMath::Sin(Time * 23.f) * 400.f);
		Flame->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.8f + FMath::Sin(Time * 11.f) * 0.08f));
	}
}

void AOSFRestingPoint::Discover(AActor* Interactor)
{
	if (bDiscovered)
	{
		return;
	}
	bDiscovered = true;
	if (UOSFProgressComponent* Progress = GetProgress(Interactor))
	{
		Progress->AddFlag(GetPersistentFlag());
	}
	UpdateVisuals();
}

void AOSFRestingPoint::Interact_Implementation(AActor* Interactor)
{
	const bool bWasDiscovered = bDiscovered;
	Discover(Interactor);
	if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Interactor))
	{
		if (!bWasDiscovered)
		{
			Player->Notify(LOCTEXT("Discovered", "RESTING POINT DISCOVERED"), EOSFNotifyType::Banner);
		}
		Player->RestAt(this);
	}
}

FText AOSFRestingPoint::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return bDiscovered ? FText::Format(LOCTEXT("RestAt", "Rest at {0}"), DisplayName) : LOCTEXT("Kindle", "Touch the flame");
}

void AOSFRestingPoint::RefreshFromProgress(UOSFProgressComponent* Progress)
{
	bDiscovered = Progress && Progress->HasFlag(GetPersistentFlag());
	UpdateVisuals();
}

FTransform AOSFRestingPoint::GetRespawnTransform() const
{
	const FVector Location = GetActorLocation() + GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 100.f);
	return FTransform((-GetActorForwardVector()).Rotation(), Location);
}

// ============================================================================
// Door
// ============================================================================

AOSFDoor::AOSFDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	Prompt = LOCTEXT("Open", "Open");
	LockedMessage = LOCTEXT("Locked", "Locked");
	Color = FLinearColor(0.3f, 0.2f, 0.12f);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);
	Hinge->SetRelativeLocation(FVector(0.f, -100.f, 0.f));

	Mesh->SetupAttachment(Hinge);
	Mesh->SetRelativeLocation(FVector(0.f, 100.f, 150.f));
	Mesh->SetRelativeScale3D(FVector(0.2f, 2.f, 3.f));
	InteractionVolume->SetSphereRadius(200.f);
	InteractionVolume->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void AOSFDoor::Interact_Implementation(AActor* Interactor)
{
	AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Interactor);
	if (bOneWay && Interactor)
	{
		const FVector ToInteractor = (Interactor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(GetActorForwardVector(), ToInteractor) < 0.f)
		{
			if (Player) { Player->Notify(LOCTEXT("OtherSide", "Does not open from this side"), EOSFNotifyType::Hint); }
			return;
		}
	}
	UOSFProgressComponent* Progress = GetProgress(Interactor);
	if (!RequiredProgress.IsEmpty() && !(Progress && Progress->HasAllProgress(RequiredProgress)))
	{
		if (Player) { Player->Notify(LockedMessage, EOSFNotifyType::Hint); }
		return;
	}
	if (RequiredKey)
	{
		UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(Interactor);
		if (!Inventory || !Inventory->HasItem(RequiredKey))
		{
			if (Player) { Player->Notify(LockedMessage, EOSFNotifyType::Hint); }
			return;
		}
		if (Player) { Player->Notify(FText::Format(LOCTEXT("UsedKey", "Used {0}"), RequiredKey->DisplayName), EOSFNotifyType::Hint); }
		if (bConsumeKey)
		{
			Inventory->RemoveItem(RequiredKey, 1);
		}
	}
	if (Progress)
	{
		Progress->AddFlag(GetPersistentFlag());
	}
	Open(false);
}

FText AOSFDoor::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return Prompt;
}

void AOSFDoor::Open(bool bInstant)
{
	bOpen = true;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bInstant)
	{
		OpenAlpha = 1.f;
		Hinge->SetRelativeRotation(FRotator(0.f, OpenAngle, 0.f));
	}
}

void AOSFDoor::RefreshFromProgress(UOSFProgressComponent* Progress)
{
	if (Progress && Progress->HasFlag(GetPersistentFlag()))
	{
		Open(true);
	}
}

void AOSFDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOpen && OpenAlpha < 1.f)
	{
		OpenAlpha = FMath::Min(1.f, OpenAlpha + DeltaSeconds * 0.8f);
		Hinge->SetRelativeRotation(FRotator(0.f, OpenAngle * FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.f), 0.f));
	}
}

// ============================================================================
// Container
// ============================================================================

AOSFContainer::AOSFContainer()
{
	Prompt = LOCTEXT("OpenChest", "Open chest");
	Color = FLinearColor(0.35f, 0.22f, 0.08f);
	Mesh->SetRelativeScale3D(FVector(0.6f, 1.f, 0.5f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	Lid->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Lid->SetRelativeScale3D(FVector(0.62f, 1.02f, 0.12f));
	Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lid->SetStaticMesh(Mesh->GetStaticMesh());
}

void AOSFContainer::Interact_Implementation(AActor* Interactor)
{
	if (bOpened)
	{
		return;
	}
	UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(Interactor);
	AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Interactor);
	if (Inventory)
	{
		for (const FOSFLootEntry& Entry : Loot)
		{
			if (Entry.Item && FMath::FRand() <= Entry.Chance)
			{
				Inventory->AddItem(Entry.Item, Entry.Count);
			}
		}
		if (Currency > 0)
		{
			Inventory->AddCurrency(Currency);
		}
	}
	if (Player && Loot.Num() == 0 && Currency == 0)
	{
		Player->Notify(LOCTEXT("Empty", "Empty"), EOSFNotifyType::Hint);
	}
	if (UOSFProgressComponent* Progress = GetProgress(Interactor))
	{
		Progress->AddFlag(GetPersistentFlag());
	}
	SetOpenedVisual(true);
}

void AOSFContainer::SetOpenedVisual(bool bInOpened)
{
	bOpened = bInOpened;
	Lid->SetRelativeLocationAndRotation(bOpened ? FVector(-35.f, 0.f, 80.f) : FVector(0.f, 0.f, 55.f), bOpened ? FRotator(-70.f, 0.f, 0.f) : FRotator::ZeroRotator);
}

void AOSFContainer::RefreshFromProgress(UOSFProgressComponent* Progress)
{
	SetOpenedVisual(Progress && Progress->HasFlag(GetPersistentFlag()));
}

// ============================================================================
// Item pickup
// ============================================================================

AOSFItemPickup::AOSFItemPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.18f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Color = FLinearColor(1.f, 0.85f, 0.4f);
	InteractionVolume->SetSphereRadius(120.f);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetIntensity(400.f);
	Glow->SetAttenuationRadius(220.f);
	Glow->SetLightColor(Color);
	Glow->SetCastShadows(false);
	Glow->SetMobility(EComponentMobility::Movable);
}

void AOSFItemPickup::BeginPlay()
{
	Super::BeginPlay();
	BaseZ = Mesh->GetRelativeLocation().Z;
	if (Item)
	{
		SetMeshColor(Item->Tint);
		if (Glow)
		{
			Glow->SetLightColor(Item->Tint);
		}
	}
}

void AOSFItemPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCollected)
	{
		const float Time = static_cast<float>(GetWorld()->GetTimeSeconds());
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + FMath::Sin(Time * 2.5f) * 8.f));
	}
}

void AOSFItemPickup::Interact_Implementation(AActor* Interactor)
{
	UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(Interactor);
	if (bCollected || !Item || !Inventory)
	{
		return;
	}
	Inventory->AddItem(Item, Count);
	if (bPersistent)
	{
		if (UOSFProgressComponent* Progress = GetProgress(Interactor))
		{
			Progress->AddFlag(GetPersistentFlag());
		}
	}
	SetCollected(true);
}

FText AOSFItemPickup::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return Item ? FText::Format(LOCTEXT("PickUp", "Pick up {0}"), Item->DisplayName) : FText::GetEmpty();
}

void AOSFItemPickup::SetCollected(bool bInCollected)
{
	bCollected = bInCollected;
	SetActorHiddenInGame(bCollected);
	InteractionVolume->SetCollisionEnabled(bCollected ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

void AOSFItemPickup::RefreshFromProgress(UOSFProgressComponent* Progress)
{
	if (bPersistent)
	{
		SetCollected(Progress && Progress->HasFlag(GetPersistentFlag()));
	}
}

// ============================================================================
// Bloodstain
// ============================================================================

AOSFBloodstain::AOSFBloodstain()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.05f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Color = FLinearColor(0.2f, 1.f, 0.4f);
	InteractionVolume->SetSphereRadius(140.f);
}

void AOSFBloodstain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Time = static_cast<float>(GetWorld()->GetTimeSeconds());
	Mesh->SetRelativeScale3D(FVector(0.3f + FMath::Sin(Time * 3.f) * 0.05f, 0.3f + FMath::Sin(Time * 3.f) * 0.05f, 0.05f));
}

void AOSFBloodstain::Interact_Implementation(AActor* Interactor)
{
	if (UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(Interactor))
	{
		Inventory->AddCurrency(Currency);
		if (AOSFPlayerCharacter* Player = Cast<AOSFPlayerCharacter>(Interactor))
		{
			Player->Notify(FText::Format(LOCTEXT("Recovered", "Recovered {0} runes"), FText::AsNumber(Currency)), EOSFNotifyType::Hint);
			Player->OnBloodstainRecovered(this);
		}
	}
	Destroy();
}

FText AOSFBloodstain::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return LOCTEXT("Retrieve", "Retrieve lost runes");
}

// ============================================================================
// Fog gate
// ============================================================================

AOSFFogGate::AOSFFogGate()
{
	Prompt = LOCTEXT("Traverse", "Traverse the mist");
	Color = FLinearColor(0.85f, 0.85f, 0.95f);
	Mesh->SetRelativeScale3D(FVector(0.1f, 4.f, 4.f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	InteractionVolume->SetSphereRadius(260.f);
	InteractionVolume->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void AOSFFogGate::BeginPlay()
{
	if (GateMaterial)
	{
		Mesh->SetMaterial(0, GateMaterial);
		Mesh->SetCastShadow(false);
	}
	Super::BeginPlay();
	if (Boss && Boss->Combat)
	{
		Boss->Combat->OnDeath.AddDynamic(this, &AOSFFogGate::HandleBossDeath);
	}
}

bool AOSFFogGate::CanInteract_Implementation(AActor* Interactor) const
{
	if (IsHidden() || !Interactor)
	{
		return false;
	}
	// Only enter from the front (forward points into the arena).
	const FVector ToInteractor = (Interactor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	return FVector::DotProduct(GetActorForwardVector(), ToInteractor) < 0.f && !(Boss && Boss->IsBossFightActive());
}

void AOSFFogGate::Interact_Implementation(AActor* Interactor)
{
	if (!Interactor)
	{
		return;
	}
	const FVector Through = GetActorLocation() + GetActorForwardVector() * 220.f;
	Interactor->SetActorLocation(FVector(Through.X, Through.Y, Interactor->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	Interactor->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	// Turn the camera too: aiming and lock-on follow the view.
	if (const APawn* Pawn = Cast<APawn>(Interactor); Pawn && Pawn->GetController())
	{
		Pawn->GetController()->SetControlRotation(FRotator(-10.f, GetActorRotation().Yaw, 0.f));
	}
	if (Boss)
	{
		Boss->BeginBossFight(Interactor);
	}
}

void AOSFFogGate::SetGateActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
}

void AOSFFogGate::HandleBossDeath(UOSFCombatComponent* InCombat, AActor* Killer)
{
	SetGateActive(false);
}

void AOSFFogGate::RefreshFromProgress(UOSFProgressComponent* Progress)
{
	const bool bDefeated = Boss && Boss->IsBossDefeated(Progress);
	SetGateActive(!bDefeated);
}

#undef LOCTEXT_NAMESPACE
