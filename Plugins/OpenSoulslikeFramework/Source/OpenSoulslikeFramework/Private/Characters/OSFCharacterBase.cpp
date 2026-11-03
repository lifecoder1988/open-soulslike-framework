// Copyright Open Soulslike Framework contributors. MIT License.

#include "Characters/OSFCharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFWeaponTraceComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFItemData.h"
#include "Interaction/OSFLadder.h"
#include "Animation/OSFClipAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Actions/OSFRangedActions.h"
#include "OSFGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Items/OSFWeaponActor.h"
#include "GameFramework/Controller.h"

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
	// The follow camera passes through bodies (a crowd behind the hero would otherwise jam it onto the hero's back); the
	// player hides a body while the camera is inside it (AOSFPlayerCharacter::UpdateCameraOverlap).
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
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
	if (Equipment)
	{
		Equipment->OnShotFired.AddDynamic(this, &AOSFCharacterBase::HandleShotFiredForAim);
	}
	VisualRoot->SetRelativeScale3D(FVector(VisualScale));
	if (bProceduralVisuals && BodyStaticMesh)
	{
		BodyMesh->SetStaticMesh(BodyStaticMesh);
		BodyMesh->SetRelativeTransform(BodyStaticMeshTransform);
		HeadMesh->SetVisibility(false);
	}
	else if (bProceduralVisuals)
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

	if (!bProceduralVisuals)
	{
		InitClipAnimation();
	}

	Combat->OnDeath.AddDynamic(this, &AOSFCharacterBase::HandleDeath);
	Combat->OnDamaged.AddDynamic(this, &AOSFCharacterBase::HandleDamaged);
	Equipment->OnEquipmentChanged.AddDynamic(this, &AOSFCharacterBase::HandleEquipmentChangedForStance);
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
	UpdateHitFeel(DeltaSeconds);

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
	UpdateFootsteps(DeltaSeconds);
	if (bProceduralVisuals)
	{
		UpdateProceduralVisuals(DeltaSeconds);
	}
	else if (GetClipAnimInstance())
	{
		UpdateSkeletalPose(DeltaSeconds);
		UpdateAimPose(DeltaSeconds);
	}
}

void AOSFCharacterBase::HandleShotFiredForAim(UOSFEquipmentComponent* InEquipment, UOSFWeaponData* Weapon, int32 Hits)
{
	LastShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Recoil = FMath::Min(Recoil + RecoilKick, RecoilKick * 3.f);
}

void AOSFCharacterBase::UpdateFootsteps(float DeltaSeconds)
{
	if (!FootstepSound || IsDead() || CurrentLadder || !GetCharacterMovement()->IsMovingOnGround())
	{
		StepDistance = 0.f;
		return;
	}
	const float Speed = GetVelocity().Size2D();
	if (Speed < 60.f)
	{
		StepDistance = FootstepStride.X * 0.6f;	// the first step after standing comes soon
		return;
	}
	const float Fraction = FMath::Clamp((Speed - WalkSpeed * 0.5f) / FMath::Max(SprintSpeed - WalkSpeed * 0.5f, 1.f), 0.f, 1.f);
	const float Stride = FMath::Lerp(FootstepStride.X, FootstepStride.Y, Fraction) * FMath::Max(GetActorScale3D().GetMax(), 0.5f);
	StepDistance += Speed * DeltaSeconds;
	if (StepDistance >= Stride)
	{
		StepDistance -= Stride;
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, Feet, FootstepVolume * FMath::Lerp(0.55f, 1.f, Fraction), FMath::FRandRange(0.92f, 1.08f));
	}
}

FVector AOSFCharacterBase::GetAimDirection() const
{
	if (const AActor* Target = GetFocusTarget())
	{
		const FVector From = GetActorLocation() + FVector(0.f, 0.f, 40.f);
		return (Target->GetActorLocation() - From).GetSafeNormal();
	}
	if (const AController* MyController = GetController(); MyController && MyController->IsPlayerController())
	{
		return MyController->GetControlRotation().Vector();
	}
	return GetActorForwardVector();
}

void AOSFCharacterBase::UpdateAimPose(float DeltaSeconds)
{
	UOSFClipAnimInstance* Clip = GetClipAnimInstance();
	if (!Clip || !Equipment)
	{
		return;
	}
	// The held gun: its actor's forward is where the clips leave it pointing (hip carry, across the chest...).
	UOSFWeaponData* Gun = Equipment->GetActiveRangedWeapon();
	const AOSFWeaponActor* GunActor = Gun ? Equipment->GetWeaponActor(Equipment->GetEquippedWeapon(true) == Gun) : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWantAim = Gun && GunActor && !IsDead() && !HasStateTag(OSFTags::State_Dodging) && !HasStateTag(OSFTags::State_Staggered)
		&& !HasStateTag(OSFTags::State_OnLadder) && !HasStateTag(OSFTags::State_Reloading) && !HasStateTag(OSFTags::State_Resting)
		&& (GetFocusTarget() != nullptr || Now - LastShotTime < AimHoldAfterShot);
	const float Rate = AimBlendTime > 0.f ? DeltaSeconds / AimBlendTime : 1.f;
	AimWeight = FMath::Clamp(AimWeight + (bWantAim ? Rate : -Rate), 0.f, 1.f);
	Recoil = FMath::FInterpTo(Recoil, 0.f, DeltaSeconds, RecoilRecovery);
	if (AimWeight <= 0.f || !GunActor)
	{
		Clip->SetAim(NAME_None, FVector::ForwardVector, FVector::ForwardVector, 0.f, MaxAimAngle);
		return;
	}
	// Where the barrel should point, in the mesh's component space, kicked up by the recoil of the last shots.
	FVector Desired = GetAimDirection();
	if (Recoil > 0.01f)
	{
		const FVector Side = FVector::CrossProduct(Desired, FVector::UpVector).GetSafeNormal();
		if (!Side.IsNearlyZero())
		{
			Desired = Desired.RotateAngleAxis(-Recoil, Side);
		}
	}
	const FTransform MeshTransform = GetMesh()->GetComponentTransform();
	Clip->AimRootBone = AimRootBone;
	Clip->SetAim(GunActor->GetAttachParentSocketName(), GunActor->GetBarrelInActorSpace(), MeshTransform.InverseTransformVectorNoScale(Desired), AimWeight, MaxAimAngle);
}

void AOSFCharacterBase::ApplyHitStop(float Seconds)
{
	if (Seconds <= 0.f || !GetWorld())
	{
		return;
	}
	HitStopUntil = FMath::Max(HitStopUntil, GetWorld()->GetRealTimeSeconds() + Seconds);
	CustomTimeDilation = 0.02f;
}

void AOSFCharacterBase::PlayHitImpact(const FVector& Location, float Scale, bool bGuarded)
{
	if (USoundBase* Sound = bGuarded && GuardSound ? GuardSound.Get() : HitSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, FMath::Clamp(0.6f + Scale * 0.3f, 0.6f, 1.3f), FMath::FRandRange(0.94f, 1.06f));
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bGuarded && HurtVoice && !IsDead() && Now - LastHurtVoiceTime > HurtVoiceCooldown)
	{
		LastHurtVoiceTime = Now;
		UGameplayStatics::PlaySoundAtLocation(this, HurtVoice, GetActorLocation(), 1.f, FMath::FRandRange(0.95f, 1.05f));
	}
	if (Scale > 0.f)
	{
		AOSFTracer::SpawnSpark(GetWorld(), Location, bGuarded ? GuardSparkColor : HitSparkColor, Scale, HitSparkMaterial);
	}
	if (bGuarded)
	{
		return;
	}
	OverlayFlash = OverlayFlash > 0.5f ? FMath::Max(OverlayFlash, 0.7f) : 1.f;
	UMeshComponent* Body = bProceduralVisuals ? static_cast<UMeshComponent*>(BodyMesh) : static_cast<UMeshComponent*>(GetMesh());
	if (HitFlashMaterial && Body)
	{
		if (!FlashMaterial)
		{
			FlashMaterial = UMaterialInstanceDynamic::Create(HitFlashMaterial, this);
		}
		FlashMaterial->SetVectorParameterValue(TEXT("Color"), HitFlashColor);
		FlashMaterial->SetScalarParameterValue(TEXT("Intensity"), HitFlashStrength * OverlayFlash * OverlayFlash);
		Body->SetOverlayMaterial(FlashMaterial);
	}
}

void AOSFCharacterBase::UpdateHitFeel(float DeltaSeconds)
{
	if (HitStopUntil > 0.0 && GetWorld()->GetRealTimeSeconds() >= HitStopUntil)
	{
		HitStopUntil = 0.0;
		CustomTimeDilation = 1.f;
	}
	if (OverlayFlash > 0.f)
	{
		// Real time, so the flash also fades during the hit stop.
		OverlayFlash = FMath::Max(0.f, OverlayFlash - GetWorld()->GetDeltaSeconds() / 0.18f);
		if (FlashMaterial)
		{
			FlashMaterial->SetScalarParameterValue(TEXT("Intensity"), HitFlashStrength * OverlayFlash * OverlayFlash);
		}
		if (OverlayFlash <= 0.f)
		{
			if (UMeshComponent* Body = bProceduralVisuals ? static_cast<UMeshComponent*>(BodyMesh) : static_cast<UMeshComponent*>(GetMesh()))
			{
				Body->SetOverlayMaterial(nullptr);
			}
		}
	}
}

UOSFClipAnimInstance* AOSFCharacterBase::GetClipAnimInstance() const
{
	return GetMesh() ? Cast<UOSFClipAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
}

void AOSFCharacterBase::InitClipAnimation()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset() || !IdleClip.IsValid())
	{
		return;
	}
	const UClass* AnimClass = SkeletalMesh->GetAnimClass();
	if (AnimClass && !AnimClass->IsChildOf(UOSFClipAnimInstance::StaticClass()))
	{
		return;	// the project supplies its own anim blueprint
	}
	if (!GetClipAnimInstance())
	{
		SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkeletalMesh->SetAnimInstanceClass(UOSFClipAnimInstance::StaticClass());
	}
	if (UOSFClipAnimInstance* ClipAnim = GetClipAnimInstance())
	{
		ClipAnim->WalkSpeed = ClipLocomotionSpeeds.X;
		ClipAnim->RunSpeed = ClipLocomotionSpeeds.Y;
		AppliedLocomotion[0] = AppliedLocomotion[1] = AppliedLocomotion[2] = nullptr;
		RefreshLocomotionClips();
	}
}

void AOSFCharacterBase::RefreshLocomotionClips()
{
	UOSFClipAnimInstance* ClipAnim = GetClipAnimInstance();
	if (!ClipAnim)
	{
		return;
	}
	const USkeleton* MeshSkeleton = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	const UOSFWeaponData* Weapon = Equipment ? Equipment->GetEquippedWeapon(true) : nullptr;
	auto Pick = [MeshSkeleton](const FOSFAnimClip& Own, const FOSFAnimClip* Stance) -> FOSFAnimClip
	{
		return (Stance && Stance->IsValid() && Stance->Sequence->GetSkeleton() == MeshSkeleton) ? *Stance : Own;
	};
	const FOSFAnimClip NewIdle = Pick(IdleClip, Weapon ? &Weapon->StanceIdleClip : nullptr);
	const FOSFAnimClip NewWalk = Pick(WalkClip, Weapon ? &Weapon->StanceWalkClip : nullptr);
	const FOSFAnimClip NewRun = Pick(RunClip, Weapon ? &Weapon->StanceRunClip : nullptr);
	if (AppliedLocomotion[0] == NewIdle.Sequence && AppliedLocomotion[1] == NewWalk.Sequence && AppliedLocomotion[2] == NewRun.Sequence)
	{
		return;
	}
	AppliedLocomotion[0] = NewIdle.Sequence;
	AppliedLocomotion[1] = NewWalk.Sequence;
	AppliedLocomotion[2] = NewRun.Sequence;
	ClipAnim->SetLocomotion(NewIdle, NewWalk, NewRun);
}

USkeletalMeshComponent* AOSFCharacterBase::AddMeshPart(USkeletalMesh* Part)
{
	USkeletalMeshComponent* Leader = GetMesh();
	if (!Part || !Leader)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
	Component->SetSkeletalMeshAsset(Part);
	Component->SetupAttachment(Leader);
	Component->SetRelativeTransform(FTransform::Identity);	// same origin and scale as the leader
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetLeaderPoseComponent(Leader);
	Component->bUseAttachParentBound = true;
	Component->RegisterComponent();
	MeshParts.Add(Component);
	return Component;
}

TArray<USkeletalMeshComponent*> AOSFCharacterBase::GetMeshParts() const
{
	TArray<USkeletalMeshComponent*> Parts;
	for (const TObjectPtr<USkeletalMeshComponent>& Part : MeshParts)
	{
		if (Part)
		{
			Parts.Add(Part);
		}
	}
	return Parts;
}

void AOSFCharacterBase::SetupSkeletalMesh(USkeletalMesh* NewMesh, float TargetHeight, float YawOffset)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!NewMesh || !SkeletalMesh)
	{
		return;
	}
	SkeletalMesh->SetSkeletalMeshAsset(NewMesh);
	const FBox Box = NewMesh->GetImportedBounds().GetBox();
	const float Height = FMath::Max(Box.GetSize().Z, 1.f);
	const float Scale = TargetHeight / Height;
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SkeletalMesh->SetRelativeScale3D(FVector(Scale));
	SkeletalMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight - Box.Min.Z * Scale), FRotator(0.f, YawOffset, 0.f));
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshBaseTransform = SkeletalMesh->GetRelativeTransform();
	bMeshBaseCaptured = true;
	bProceduralVisuals = false;
	VisualRoot->SetVisibility(false, true);
	if (HasActorBegunPlay())
	{
		InitClipAnimation();
		Equipment->RefreshEquipment();
	}
}

void AOSFCharacterBase::UpdateSkeletalPose(float DeltaSeconds)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!bMeshBaseCaptured)
	{
		MeshBaseTransform = SkeletalMesh->GetRelativeTransform();
		bMeshBaseCaptured = true;
	}
	FRotator Lean = FRotator::ZeroRotator;
	if (IsDead() && bDeathClipPlaying)
	{
		// The death clip lays the body down itself.
	}
	else if (IsDead())
	{
		DeathFall = FMath::Min(1.f, DeathFall + DeltaSeconds * 2.2f);
		Lean = FRotator(0.f, 0.f, 82.f * FMath::InterpEaseIn(0.f, 1.f, DeathFall, 2.f));
	}
	else if (HasStateTag(OSFTags::State_PoiseBroken) || HasStateTag(OSFTags::State_BeingExecuted))
	{
		Lean = FRotator(-24.f, 0.f, 0.f);
	}
	else if (HasStateTag(OSFTags::State_Staggered))
	{
		// Thrown away from the blow: back from a hit in front, forward from behind, sideways from the flank.
		const float Time = static_cast<float>(GetWorld()->GetTimeSeconds());
		Lean = FRotator(12.f * LastHitFrom.X, 0.f, -12.f * LastHitFrom.Y + FMath::Sin(Time * 40.f) * 4.f);
	}
	else if (HasStateTag(OSFTags::State_Dodging))
	{
		const UOSFAction* Current = ActionComponent->GetCurrentAction();
		if (!(Current && Current->IsClipMode()))
		{
			Lean = FRotator(-28.f, 0.f, 0.f);
		}
	}
	// Lean is expressed in actor space and applied around the feet, lifted by half the body's thickness so a
	// body tipped over lies on the ground instead of at hip height (or half sunk into it).
	SmoothedSwing = FMath::RInterpTo(SmoothedSwing, Lean, DeltaSeconds, IsDead() ? 30.f : 12.f);
	const FQuat LeanQuat = SmoothedSwing.Quaternion();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Pivot(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight() + Capsule->GetUnscaledCapsuleRadius() * 0.5f);
	SkeletalMesh->SetRelativeLocationAndRotation(Pivot + LeanQuat.RotateVector(MeshBaseTransform.GetLocation() - Pivot), LeanQuat * MeshBaseTransform.GetRotation());

	// Hit flash on the mesh's materials is project specific; the overlay is left to the game.
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
	else if (HasStateTag(OSFTags::State_Attacking))
	{
		// Whole-body telegraph: lean back in the windup, lunge into the swing, settle in the recovery.
		const UOSFAction* Current = ActionComponent->GetCurrentAction();
		const EOSFActionPhase AttackPhase = Current ? Current->GetPhase() : EOSFActionPhase::None;
		const float Pitch = AttackPhase == EOSFActionPhase::Windup ? 12.f : (AttackPhase == EOSFActionPhase::Active ? -16.f : -5.f);
		BodyTarget = FRotator(Pitch, SwingOffset.Yaw * 0.2f, 0.f);
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
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
	if (UOSFClipAnimInstance* ClipAnim = GetClipAnimInstance(); ClipAnim && DeathClip.IsValid()
		&& DeathClip.Sequence->GetSkeleton() == GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
	{
		FOSFAnimClip Clip = DeathClip;
		Clip.bLoop = false;
		Clip.bUpperBody = false;
		ClipAnim->PlayActionClip(Clip, -1.f, true);
		bDeathClipPlaying = true;
	}
	K2_OnDeath(Killer);
}

void AOSFCharacterBase::HandleDamaged(UOSFCombatComponent* InCombat, const FOSFHitInfo& HitInfo)
{
	// Where it came from, for the directional flinch and the lean of the procedural stagger.
	if (HitInfo.Instigator && HitInfo.Instigator != this)
	{
		const FVector From = (HitInfo.Instigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!From.IsNearlyZero())
		{
			LastHitFrom = GetActorRotation().UnrotateVector(From);
		}
	}
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
	if (Tag == OSFTags::State_Parrying)
	{
		Equipment->SetOffHandDrawn(bAdded);	// a sheathed parrying dagger comes out for the parry
	}
	if (bAdded && (Tag == OSFTags::State_Staggered || Tag == OSFTags::State_PoiseBroken))
	{
		if (UOSFClipAnimInstance* ClipAnim = GetClipAnimInstance())
		{
			// The reaction that matches the side the blow landed on: heavy for a broken stance, else the flinch.
			const FOSFDirectionalClips& Set = (Tag == OSFTags::State_PoiseBroken && StaggerClips.IsValid()) ? StaggerClips : HitReactClips;
			const FOSFAnimClip& Reaction = Set.IsValid() ? Set.Pick(LastHitFrom) : HitReactClip;
			Reaction.IsValid() ? ClipAnim->PlayActionClip(Reaction) : ClipAnim->StopActionClip(0.1f);
		}
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
	bDeathClipPlaying = false;
	SwingOffset = FRotator::ZeroRotator;
	SmoothedSwing = FRotator::ZeroRotator;
	if (bMeshBaseCaptured && !bProceduralVisuals && GetMesh())
	{
		GetMesh()->SetRelativeTransform(MeshBaseTransform);
	}
	if (UOSFClipAnimInstance* ClipAnim = GetClipAnimInstance())
	{
		ClipAnim->StopActionClip(0.f);
	}
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
	UpdateClimbClip();
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
	if (UOSFClipAnimInstance* Anim = GetClipAnimInstance(); Anim && ClimbClip.IsValid())
	{
		Anim->SetActionRate(1.f);
		Anim->StopActionClip(0.15f);
	}
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
	UpdateClimbClip();
	if (bProceduralVisuals)
	{
		const float Phase = FMath::Sin(LadderHeight * 0.08f);
		RightHandPivot->SetRelativeLocation(FVector(18.f, 40.f, 110.f + Phase * 20.f));
		LeftHandPivot->SetRelativeLocation(FVector(22.f, -38.f, 112.f - Phase * 20.f));
	}
}

void AOSFCharacterBase::UpdateClimbClip()
{
	UOSFClipAnimInstance* Anim = GetClipAnimInstance();
	if (!Anim || !CurrentLadder || !ClimbClip.IsValid())
	{
		return;
	}
	const bool bDown = LadderInput < -KINDA_SMALL_NUMBER;
	const bool bOwnDownClip = ClimbDownClip.IsValid();
	const FOSFAnimClip& Wanted = (bDown && bOwnDownClip) ? ClimbDownClip : ClimbClip;
	if (!Anim->IsPlayingSequence(Wanted.Sequence))
	{
		Anim->PlayActionClip(Wanted);
	}
	// One cycle covers ClimbClipStride cm of ladder; backwards when going down on the up clip.
	const float Rate = CurrentLadder->ClimbSpeed / FMath::Max(ClimbClipStride, 10.f);
	Anim->SetActionRate((bDown && !bOwnDownClip) ? -Rate : Rate);
	Anim->HoldActionClip(FMath::IsNearlyZero(LadderInput));
}
