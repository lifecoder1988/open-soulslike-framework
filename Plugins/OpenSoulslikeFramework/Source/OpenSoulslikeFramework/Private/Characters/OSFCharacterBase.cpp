// Copyright Open Soulslike Framework contributors. MIT License.

#include "Characters/OSFCharacterBase.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFWeaponTraceComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Interaction/OSFLadder.h"
#include "OSFGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AOSFCharacterBase::AOSFCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UOSFStatComponent>(TEXT("Stats"));
	ActionComponent = CreateDefaultSubobject<UOSFActionComponent>(TEXT("Actions"));
	Combat = CreateDefaultSubobject<UOSFCombatComponent>(TEXT("Combat"));
	StatusEffects = CreateDefaultSubobject<UOSFStatusEffectComponent>(TEXT("StatusEffects"));
	Buffs = CreateDefaultSubobject<UOSFBuffComponent>(TEXT("Buffs"));
	Inventory = CreateDefaultSubobject<UOSFInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UOSFEquipmentComponent>(TEXT("Equipment"));
	WeaponTrace = CreateDefaultSubobject<UOSFWeaponTraceComponent>(TEXT("WeaponTrace"));

	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->JumpZVelocity = 520.f;
	Movement->AirControl = 0.3f;
	Movement->BrakingDecelerationWalking = 2400.f;
	Movement->MaxAcceleration = 2400.f;
	Movement->GroundFriction = 8.f;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));

	// --- Procedural body (used when no skeletal mesh is set) ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -92.f));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(VisualRoot);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
	BodyMesh->SetRelativeScale3D(FVector(0.62f, 0.5f, 1.32f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(VisualRoot);
	HeadMesh->SetRelativeLocation(FVector(4.f, 0.f, 160.f));
	HeadMesh->SetRelativeScale3D(FVector(0.4f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetCanEverAffectNavigation(false);

	if (CylinderMesh.Succeeded()) { BodyMesh->SetStaticMesh(CylinderMesh.Object); }
	if (SphereMesh.Succeeded()) { HeadMesh->SetStaticMesh(SphereMesh.Object); }

	RightHandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandPivot"));
	RightHandPivot->SetupAttachment(VisualRoot);
	RightHandPivot->SetRelativeLocationAndRotation(FVector(18.f, 40.f, 110.f), FRotator(15.f, 0.f, 0.f));

	LeftHandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHandPivot"));
	LeftHandPivot->SetupAttachment(VisualRoot);
	LeftHandPivot->SetRelativeLocationAndRotation(FVector(22.f, -38.f, 112.f), FRotator(0.f, 0.f, 0.f));
}

void AOSFCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	bProceduralVisuals = !(GetMesh() && GetMesh()->GetSkeletalMeshAsset());
	VisualRoot->SetVisibility(bProceduralVisuals, true);
	VisualRoot->SetRelativeScale3D(FVector(VisualScale));
	if (bProceduralVisuals)
	{
		BodyMaterial = BodyMesh->CreateDynamicMaterialInstance(0);
		if (UMaterialInstanceDynamic* HeadMaterial = HeadMesh->CreateDynamicMaterialInstance(0))
		{
			HeadMaterial->SetVectorParameterValue(TEXT("Color"), BodyColor * 0.8f);
		}
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("Color"), BodyColor);
		}
	}

	Combat->OnDeath.AddDynamic(this, &AOSFCharacterBase::HandleDeath);
	Combat->OnDamaged.AddDynamic(this, &AOSFCharacterBase::HandleDamaged);
	ActionComponent->OnStateTagChanged.AddDynamic(this, &AOSFCharacterBase::HandleStateTagChanged);
}

void AOSFCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (ActionComponent)
	{
		TagContainer.AppendTags(ActionComponent->GetStateTags());
	}
	if (Buffs)
	{
		Buffs->AppendGrantedTags(TagContainer);
	}
}

bool AOSFCharacterBase::IsDead() const
{
	return Combat && Combat->IsDead();
}

bool AOSFCharacterBase::HasStateTag(FGameplayTag Tag) const
{
	return ActionComponent && ActionComponent->HasStateTag(Tag);
}

bool AOSFCharacterBase::IsSprinting() const
{
	return HasStateTag(OSFTags::State_Sprinting);
}

void AOSFCharacterBase::SetSprinting(bool bNewSprinting)
{
	if (bNewSprinting == IsSprinting())
	{
		return;
	}
	if (bNewSprinting)
	{
		if (Stats->GetCurrent(OSFTags::Stat_Stamina) <= 0.f || HasStateTag(OSFTags::State_Guarding))
		{
			return;
		}
		ActionComponent->AddStateTag(OSFTags::State_Sprinting);
	}
	else
	{
		ActionComponent->ClearStateTag(OSFTags::State_Sprinting);
	}
}

FVector AOSFCharacterBase::GetDesiredFacingDirection() const
{
	if (const AActor* Focus = GetFocusTarget())
	{
		const FVector ToFocus = (Focus->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!ToFocus.IsNearlyZero())
		{
			return ToFocus;
		}
	}
	if (!DesiredMoveDirection.IsNearlyZero())
	{
		return DesiredMoveDirection.GetSafeNormal2D();
	}
	return GetActorForwardVector();
}

float AOSFCharacterBase::GetActionMovementScale() const
{
	if (!ActionComponent)
	{
		return 1.f;
	}
	static const FGameplayTagContainer Rooted = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(OSFTags::State_Dead);
		Tags.AddTag(OSFTags::State_Staggered);
		Tags.AddTag(OSFTags::State_PoiseBroken);
		Tags.AddTag(OSFTags::State_BeingExecuted);
		Tags.AddTag(OSFTags::State_Resting);
		Tags.AddTag(OSFTags::State_InMenu);
		Tags.AddTag(OSFTags::State_OnLadder);
		return Tags;
	}();
	if (ActionComponent->GetStateTags().HasAny(Rooted))
	{
		return 0.f;
	}
	const UOSFAction* Current = ActionComponent->GetCurrentAction();
	return Current ? Current->MovementScale : 1.f;
}

void AOSFCharacterBase::SetProceduralSwing(float Yaw, float Pitch, float Roll)
{
	SwingOffset = FRotator(Pitch, Yaw, Roll);
}

void AOSFCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsSprinting())
	{
		const bool bMoving = GetVelocity().Size2D() > 50.f;
		if (bMoving)
		{
			Stats->ModifyCurrent(OSFTags::Stat_Stamina, -SprintStaminaPerSecond * DeltaSeconds, true);
		}
		if (Stats->GetCurrent(OSFTags::Stat_Stamina) <= 0.f || HasStateTag(OSFTags::State_Guarding) || IsDead())
		{
			SetSprinting(false);
		}
	}

	UpdateStaminaRegen();
	UpdateMovementSpeed();
	UpdateLadder(DeltaSeconds);
	if (bProceduralVisuals)
	{
		UpdateProceduralVisuals(DeltaSeconds);
	}
}

void AOSFCharacterBase::UpdateStaminaRegen()
{
	float Multiplier = Buffs ? Buffs->GetStaminaRegenMultiplier() : 1.f;
	if (HasStateTag(OSFTags::State_Guarding))
	{
		Multiplier *= Stats->GuardingStaminaRegenScale;
	}
	if (HasStateTag(OSFTags::State_Attacking) || HasStateTag(OSFTags::State_Dodging) || IsSprinting())
	{
		Multiplier = 0.f;
	}
	Stats->RegenMultipliers.Add(OSFTags::Stat_Stamina, Multiplier);
}

void AOSFCharacterBase::UpdateMovementSpeed()
{
	float Speed = WalkSpeed;
	if (HasStateTag(OSFTags::State_Guarding))
	{
		Speed = GuardWalkSpeed;
	}
	else if (IsSprinting())
	{
		Speed = SprintSpeed;
	}
	if (Equipment && Equipment->GetEquipLoadCategory() == EOSFEquipLoad::Overloaded)
	{
		Speed *= 0.5f;
	}
	const float Scale = GetActionMovementScale();
	GetCharacterMovement()->MaxWalkSpeed = Speed * FMath::Max(Scale, 0.f);
}

void AOSFCharacterBase::UpdateProceduralVisuals(float DeltaSeconds)
{
	// Body pose from state.
	FRotator BodyTarget = FRotator::ZeroRotator;
	FVector BodyScale = FVector(1.f);
	if (IsDead())
	{
		DeathFall = FMath::Min(1.f, DeathFall + DeltaSeconds * 2.5f);
		BodyTarget = FRotator(0.f, 0.f, 85.f * DeathFall);
	}
	else if (HasStateTag(OSFTags::State_PoiseBroken) || HasStateTag(OSFTags::State_BeingExecuted))
	{
		BodyTarget = FRotator(-30.f, 0.f, 0.f);
		BodyScale = FVector(1.f, 1.f, 0.75f);
	}
	else if (HasStateTag(OSFTags::State_Staggered))
	{
		const float Time = static_cast<float>(GetWorld()->GetTimeSeconds());
		BodyTarget = FRotator(18.f, 0.f, FMath::Sin(Time * 40.f) * 6.f);
	}
	else if (HasStateTag(OSFTags::State_Dodging))
	{
		const UOSFAction_Dodge* Dodge = Cast<UOSFAction_Dodge>(ActionComponent->GetCurrentAction());
		if (Dodge && !Dodge->IsBackstep())
		{
			const float Total = 0.6f;
			BodyTarget = FRotator(-360.f * FMath::Clamp(Dodge->GetElapsed() / Total, 0.f, 1.f), 0.f, 0.f);
			BodyScale = FVector(1.f, 1.f, 0.6f);
		}
		else
		{
			BodyTarget = FRotator(14.f, 0.f, 0.f);
		}
	}
	else if (HasStateTag(OSFTags::State_Resting))
	{
		BodyScale = FVector(1.f, 1.f, 0.6f);
	}

	const bool bSnap = HasStateTag(OSFTags::State_Dodging);
	const FRotator CurrentBody = VisualRoot->GetRelativeRotation();
	VisualRoot->SetRelativeRotation(bSnap ? BodyTarget : FMath::RInterpTo(CurrentBody, BodyTarget, DeltaSeconds, 12.f));
	VisualRoot->SetRelativeScale3D(FMath::VInterpTo(VisualRoot->GetRelativeScale3D(), BodyScale * VisualScale, DeltaSeconds, 14.f));

	// Right hand: attack swings.
	SmoothedSwing = FMath::RInterpTo(SmoothedSwing, SwingOffset, DeltaSeconds, 30.f);
	RightHandPivot->SetRelativeRotation(FRotator(15.f, 0.f, 0.f) + SmoothedSwing);

	// Left hand: shield up while guarding.
	const bool bGuard = HasStateTag(OSFTags::State_Guarding) || HasStateTag(OSFTags::State_Parrying);
	const FVector LeftTargetLocation = bGuard ? FVector(48.f, -6.f, 118.f) : FVector(22.f, -38.f, 112.f);
	const FRotator LeftTargetRotation = bGuard ? FRotator(0.f, -10.f, 0.f) : FRotator::ZeroRotator;
	LeftHandPivot->SetRelativeLocation(FMath::VInterpTo(LeftHandPivot->GetRelativeLocation(), LeftTargetLocation, DeltaSeconds, 18.f));
	LeftHandPivot->SetRelativeRotation(FMath::RInterpTo(LeftHandPivot->GetRelativeRotation(), LeftTargetRotation, DeltaSeconds, 18.f));

	// Colour: hit flash, attack telegraph.
	if (BodyMaterial)
	{
		HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds * 4.f);
		FLinearColor Color = BodyColor;
		const UOSFAction* Current = ActionComponent->GetCurrentAction();
		if (Current && Current->GetPhase() == EOSFActionPhase::Windup && HasStateTag(OSFTags::State_Attacking))
		{
			Color = FMath::Lerp(Color, FLinearColor(1.f, 0.45f, 0.1f), 0.35f);
		}
		if (HasStateTag(OSFTags::State_PoiseBroken))
		{
			Color = FMath::Lerp(Color, FLinearColor(1.f, 0.9f, 0.3f), 0.5f);
		}
		if (IsDead())
		{
			Color = Color * 0.35f;
		}
		Color = FMath::Lerp(Color, FLinearColor::White, HitFlash);
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AOSFCharacterBase::HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer)
{
	SetSprinting(false);
	if (CurrentLadder)
	{
		ExitLadder(false);
	}
	K2_OnDeath(Killer);
}

void AOSFCharacterBase::HandleDamaged(UOSFCombatComponent* InCombat, const FOSFHitInfo& HitInfo)
{
	if (HitInfo.Result == EOSFHitResult::Hit || HitInfo.Result == EOSFHitResult::Killed || HitInfo.Result == EOSFHitResult::GuardBroken)
	{
		HitFlash = 0.8f;
	}
	else if (HitInfo.Result == EOSFHitResult::Guarded || HitInfo.Result == EOSFHitResult::PerfectGuarded)
	{
		HitFlash = 0.3f;
	}
}

void AOSFCharacterBase::HandleStateTagChanged(FGameplayTag Tag, bool bAdded)
{
	if (bAdded && (Tag == OSFTags::State_Staggered || Tag == OSFTags::State_PoiseBroken))
	{
		SetSprinting(false);
		if (CurrentLadder)
		{
			ExitLadder(false);
		}
	}
}

void AOSFCharacterBase::ResetCharacter(const FTransform& Transform)
{
	if (CurrentLadder)
	{
		ExitLadder(false);
	}
	SetSprinting(false);
	ActionComponent->CancelCurrentAction();
	Buffs->RemoveAllBuffs(true);
	Combat->Revive();
	SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* MyController = GetController())
	{
		MyController->SetControlRotation(Transform.GetRotation().Rotator());
	}
	DeathFall = 0.f;
	SwingOffset = FRotator::ZeroRotator;
	if (VisualRoot)
	{
		VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

// --- Ladder ---------------------------------------------------------------------

void AOSFCharacterBase::EnterLadder(AOSFLadder* Ladder, bool bFromTop)
{
	if (!Ladder || CurrentLadder || IsDead())
	{
		return;
	}
	ActionComponent->CancelCurrentAction();
	SetSprinting(false);
	CurrentLadder = Ladder;
	LadderHeight = bFromTop ? Ladder->GetLadderHeight() - 10.f : 10.f;
	ActionComponent->AddStateTag(OSFTags::State_OnLadder);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	SetActorRotation(Ladder->GetClimbRotation());
	SetActorLocation(Ladder->GetClimbLocation(LadderHeight, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void AOSFCharacterBase::ExitLadder(bool bAtTop)
{
	if (!CurrentLadder)
	{
		return;
	}
	AOSFLadder* Ladder = CurrentLadder;
	CurrentLadder = nullptr;
	LadderInput = 0.f;
	ActionComponent->ClearStateTag(OSFTags::State_OnLadder);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	const FVector Exit = bAtTop ? Ladder->GetTopExitLocation() : Ladder->GetBottomExitLocation();
	SetActorLocation(Exit + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
}

void AOSFCharacterBase::UpdateLadder(float DeltaSeconds)
{
	if (!CurrentLadder)
	{
		return;
	}
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	LadderHeight += LadderInput * CurrentLadder->ClimbSpeed * DeltaSeconds;
	if (LadderHeight >= CurrentLadder->GetLadderHeight())
	{
		ExitLadder(true);
		return;
	}
	if (LadderHeight <= 0.f && LadderInput < 0.f)
	{
		ExitLadder(false);
		return;
	}
	SetActorLocation(CurrentLadder->GetClimbLocation(LadderHeight, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	if (bProceduralVisuals)
	{
		const float Phase = FMath::Sin(LadderHeight * 0.08f);
		RightHandPivot->SetRelativeLocation(FVector(18.f, 40.f, 110.f + Phase * 20.f));
		LeftHandPivot->SetRelativeLocation(FVector(22.f, -38.f, 112.f - Phase * 20.f));
	}
}
