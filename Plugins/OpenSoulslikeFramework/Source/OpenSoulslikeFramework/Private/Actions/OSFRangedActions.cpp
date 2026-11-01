// Copyright Open Soulslike Framework contributors. MIT License.

#include "Actions/OSFRangedActions.h"
#include "Actions/OSFActionComponent.h"
#include "Characters/OSFCharacterBase.h"
#include "Combat/OSFCombatComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFWeaponActor.h"
#include "Stats/OSFBuffComponent.h"
#include "OSFGameplayTags.h"
#include "OpenSoulslikeFramework.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UOSFEquipmentComponent* EquipmentOf(const UOSFActionComponent* Actions)
	{
		return Actions ? OSF::Get<UOSFEquipmentComponent>(Actions->GetOwner()) : nullptr;
	}
}

// ============================================================================
// Shoot
// ============================================================================

UOSFAction_Shoot::UOSFAction_Shoot()
{
	ActionTag = OSFTags::Action_Shoot;
	GrantedTags.AddTag(OSFTags::State_Shooting);
	CancelledByActions.AddTag(OSFTags::Action_Dodge);
	MovementScale = 0.55f;
	WindupRotationRate = 1080.f;
	bCanBeBuffered = true;
	CurrentTiming.Windup = 0.06f;
	CurrentTiming.Active = 0.02f;
	CurrentTiming.Recovery = 0.18f;
	CurrentTiming.CancelWindowStart = 0.7f;
}

bool UOSFAction_Shoot::CanActivate_Implementation() const
{
	const UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	const UOSFWeaponData* Ranged = Equipment ? Equipment->GetActiveRangedWeapon() : nullptr;
	return Ranged && Equipment->GetMagazine(Ranged).Rounds > 0 && Super::CanActivate_Implementation();
}

UOSFAction* UOSFAction_Shoot::GetRedirect()
{
	const UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	const UOSFWeaponData* Ranged = Equipment ? Equipment->GetActiveRangedWeapon() : nullptr;
	if (Ranged && Equipment->GetMagazine(Ranged).Rounds <= 0)
	{
		UOSFAction* Reload = ActionComponent->FindAction(OSFTags::Action_Reload);
		return (Reload && Reload->CanActivate()) ? Reload : nullptr;
	}
	return nullptr;
}

void UOSFAction_Shoot::OnPreStart()
{
	UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	Weapon = Equipment ? Equipment->GetActiveRangedWeapon() : nullptr;
	if (Weapon)
	{
		CurrentTiming.Recovery = FMath::Max(0.02f, Weapon->Ranged.FireInterval - CurrentTiming.Windup - CurrentTiming.Active);
	}
}

const FOSFAnimClip* UOSFAction_Shoot::GetClipToPlay() const
{
	return (Weapon && Weapon->Ranged.FireClip.IsValid()) ? &Weapon->Ranged.FireClip : Super::GetClipToPlay();
}

void UOSFAction_Shoot::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window == EOSFActionWindow::Active && bOpen && Weapon)
	{
		FireWeapon(Weapon);
	}
}

FVector UOSFAction_Shoot::GetMuzzleLocation(const UOSFWeaponData* InWeapon) const
{
	const ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return FVector::ZeroVector;
	}
	const UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	const bool bRight = Equipment && Equipment->GetEquippedWeapon(true) == InWeapon;
	if (const AOSFWeaponActor* WeaponActor = Equipment ? Equipment->GetWeaponActor(bRight) : nullptr)
	{
		if (!WeaponActor->IsHidden())
		{
			// Barrel = weapon actor +X, but always in front of the character so close-range shots don't start behind it.
			const FVector FromActor = WeaponActor->GetActorTransform().TransformPosition(InWeapon ? InWeapon->Ranged.MuzzleOffset : FVector::ZeroVector);
			const FVector Chest = Character->GetActorLocation() + FVector(0.f, 0.f, 40.f);
			return FVector::DotProduct(FromActor - Chest, Character->GetActorForwardVector()) > 0.f ? FromActor : Chest + Character->GetActorForwardVector() * 40.f;
		}
	}
	return Character->GetActorLocation() + Character->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 45.f);
}

FVector UOSFAction_Shoot::ComputeAimPoint(const UOSFWeaponData* InWeapon, const FVector& Muzzle) const
{
	const ACharacter* Character = GetCharacter();
	const float Range = InWeapon ? InWeapon->Ranged.Range : 5000.f;
	auto TargetPoint = [](const AActor* Target)
	{
		const UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
		return Target->GetActorLocation() + FVector(0.f, 0.f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.35f : 30.f);
	};

	if (const AOSFCharacterBase* OSFCharacter = Cast<AOSFCharacterBase>(Character))
	{
		if (const AActor* Focus = OSFCharacter->GetFocusTarget())
		{
			return TargetPoint(Focus);
		}
	}

	// Player: along the view, then aim assist inside a small cone.
	FVector ViewLocation = Muzzle;
	FVector ViewDirection = Character ? Character->GetActorForwardVector() : FVector::ForwardVector;
	if (const APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr)
	{
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewDirection = ViewRotation.Vector();
		// Third person: keep the shot level-ish with the character's facing when the camera looks down at it.
		ViewDirection = (ViewDirection.GetSafeNormal2D() * 0.85f + ViewDirection * 0.15f).GetSafeNormal();
	}
	const float AssistCos = FMath::Cos(FMath::DegreesToRadians(InWeapon ? InWeapon->Ranged.AimAssistDegrees : 8.f));
	const UOSFCombatComponent* MyCombat = Character ? OSF::Get<UOSFCombatComponent>(Character) : nullptr;
	const AActor* Best = nullptr;
	float BestDot = AssistCos;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AOSFCharacterBase> It(World); It; ++It)
		{
			const AOSFCharacterBase* Other = *It;
			if (Other == Character || Other->IsDead() || Other->IsHidden() || (MyCombat && MyCombat->IsSameTeam(Other)))
			{
				continue;
			}
			const FVector To = TargetPoint(Other) - Muzzle;
			if (To.Size() > Range)
			{
				continue;
			}
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), ViewDirection);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				Best = Other;
			}
		}
	}
	if (Best)
	{
		return TargetPoint(Best);
	}
	return Muzzle + ViewDirection * Range;
}

int32 UOSFAction_Shoot::FireWeapon(UOSFWeaponData* InWeapon)
{
	ACharacter* Character = GetCharacter();
	UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	UWorld* World = GetWorld();
	if (!Character || !Equipment || !World || !InWeapon || !InWeapon->IsRanged())
	{
		return 0;
	}
	const FOSFMagazine Magazine = Equipment->GetMagazine(InWeapon);
	const UOSFAmmoData* Ammo = Magazine.Ammo;
	if (!Ammo || !Equipment->ConsumeRound(InWeapon))
	{
		return 0;
	}

	// Hits are traced from the shooter's chest (inside its own, ignored capsule): a muzzle that pokes into a
	// close enemy's capsule would otherwise start the trace inside it and miss. The tracer is drawn from the muzzle.
	const FVector Muzzle = GetMuzzleLocation(InWeapon);
	const FVector TraceOrigin = Character->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector AimPoint = ComputeAimPoint(InWeapon, Muzzle);
	const FVector AimDirection = (AimPoint - TraceOrigin).GetSafeNormal();
	if (!AimDirection.IsNearlyZero())
	{
		Character->SetActorRotation(FRotator(0.f, AimDirection.Rotation().Yaw, 0.f));
	}

	const float BuffMultiplier = OSF::Get<UOSFBuffComponent>(Character) ? OSF::Get<UOSFBuffComponent>(Character)->GetAttackPowerMultiplier() : 1.f;
	FOSFDamageSpec Spec;
	for (const TPair<FGameplayTag, float>& Pair : Ammo->Damage)
	{
		Spec.Damage.Add(Pair.Key, Pair.Value * InWeapon->Ranged.DamageScale * BuffMultiplier);
	}
	Spec.PoiseDamage = Ammo->PoiseDamage;
	Spec.StatusBuildup = Ammo->StatusBuildup;
	Spec.SourceTags = Ammo->SourceTags;
	Spec.bCanBeParried = false;
	Spec.bCanBeGuarded = true;
	Spec.GuardStaminaDamageScale = 0.35f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFShoot), false, Character);
	TArray<AActor*> Attached;
	Character->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	TSet<AActor*> Damaged;
	int32 Hits = 0;
	for (int32 Pellet = 0; Pellet < FMath::Max(1, Ammo->Pellets); ++Pellet)
	{
		const FVector Direction = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(InWeapon->Ranged.SpreadDegrees));
		const FVector End = TraceOrigin + Direction * InWeapon->Ranged.Range;
		TArray<FHitResult> Results;
		World->LineTraceMultiByObjectType(Results, TraceOrigin, End, Objects, Params);
		UE_LOG(LogOSF, Verbose, TEXT("Shot from %s towards %s: %d hits"), *TraceOrigin.ToCompactString(), *AimPoint.ToCompactString(), Results.Num());
		FVector Impact = End;
		bool bImpact = false;
		for (const FHitResult& Hit : Results)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor->IsA<AOSFWeaponActor>() || HitActor->IsA<AOSFTracer>())
			{
				continue;
			}
			// Only things that would stop a body stop a bullet: interaction spheres and other triggers don't.
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (HitComponent && HitComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}
			UOSFCombatComponent* VictimCombat = OSF::Get<UOSFCombatComponent>(HitActor);
			if (VictimCombat && (VictimCombat->IsDead() || VictimCombat->IsSameTeam(Character)))
			{
				continue;
			}
			Impact = Hit.ImpactPoint;
			bImpact = true;
			if (!VictimCombat)
			{
				UE_LOG(LogOSF, Verbose, TEXT("Shot stopped by %s (%s) after %.0f cm"), *HitActor->GetName(), HitComponent ? *HitComponent->GetName() : TEXT("-"), Hit.Distance);
			}
			if (VictimCombat)
			{
				FOSFDamageEvent Event;
				Event.Spec = Spec;
				// Pellets after the first one only add damage, not repeated poise / status.
				if (Damaged.Contains(HitActor))
				{
					Event.Spec.PoiseDamage = 0.f;
					Event.Spec.StatusBuildup.Reset();
				}
				Event.Instigator = Character;
				Event.DamageCauser = Character;
				Event.HitLocation = Hit.ImpactPoint;
				const FOSFHitInfo Info = VictimCombat->ReceiveDamage(Event);
				UE_LOG(LogOSF, Verbose, TEXT("Shot hit %s: result %d, damage %.1f, negated %.0f%%%s"), *HitActor->GetName(), static_cast<int32>(Info.Result), Info.DamageDealt, Info.NegatedFraction * 100.f, Info.bCountered ? TEXT(", countered") : TEXT(""));
				if (Info.Result != EOSFHitResult::Ignored)
				{
					++Hits;
					Damaged.Add(HitActor);
					if (UOSFCombatComponent* MyCombat = OSF::Get<UOSFCombatComponent>(Character))
					{
						MyCombat->OnHitLanded.Broadcast(MyCombat, HitActor, Info);
					}
				}
			}
			break;
		}
		AOSFTracer::Spawn(World, Muzzle, Impact, Ammo->TracerColor, bImpact, InWeapon->Ranged.TracerMaterial);
	}
	if (InWeapon->Ranged.FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, InWeapon->Ranged.FireSound, Muzzle);
	}
	Equipment->OnShotFired.Broadcast(Equipment, InWeapon, Hits);
	return Hits;
}

// ============================================================================
// Reload
// ============================================================================

UOSFAction_Reload::UOSFAction_Reload()
{
	ActionTag = OSFTags::Action_Reload;
	GrantedTags.AddTag(OSFTags::State_Reloading);
	CancelledByActions.AddTag(OSFTags::Action_Dodge);
	MovementScale = 0.7f;
	WindupRotationRate = 0.f;
	bCancellableInCancelWindow = true;
	CurrentTiming.Windup = 1.3f;
	CurrentTiming.Active = 0.05f;
	CurrentTiming.Recovery = 0.2f;
	CurrentTiming.CancelWindowStart = 0.f;
}

bool UOSFAction_Reload::CanActivate_Implementation() const
{
	const UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	const UOSFWeaponData* Ranged = Equipment ? Equipment->GetActiveRangedWeapon() : nullptr;
	return Ranged && Equipment->CanReload(Ranged) && Super::CanActivate_Implementation();
}

void UOSFAction_Reload::OnPreStart()
{
	UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent);
	Weapon = Equipment ? Equipment->GetActiveRangedWeapon() : nullptr;
	if (Weapon)
	{
		CurrentTiming.Windup = FMath::Max(0.05f, Weapon->Ranged.ReloadTime - CurrentTiming.Active - CurrentTiming.Recovery);
		if (Weapon->Ranged.ReloadSound && GetCharacter())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Weapon->Ranged.ReloadSound, GetCharacter()->GetActorLocation());
		}
	}
}

const FOSFAnimClip* UOSFAction_Reload::GetClipToPlay() const
{
	return (Weapon && Weapon->Ranged.ReloadClip.IsValid()) ? &Weapon->Ranged.ReloadClip : Super::GetClipToPlay();
}

void UOSFAction_Reload::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window == EOSFActionWindow::Active && bOpen && Weapon)
	{
		if (UOSFEquipmentComponent* Equipment = EquipmentOf(ActionComponent))
		{
			Equipment->Reload(Weapon);
		}
	}
}

// ============================================================================
// Tracer
// ============================================================================

AOSFTracer::AOSFTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	RootComponent = Beam;
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	if (CylinderMesh.Succeeded()) { Beam->SetStaticMesh(CylinderMesh.Object); }

	Spark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spark"));
	Spark->SetupAttachment(Beam);
	Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Spark->SetCastShadow(false);
	Spark->SetUsingAbsoluteScale(true);
	if (SphereMesh.Succeeded()) { Spark->SetStaticMesh(SphereMesh.Object); }

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Beam);
	Flash->SetIntensity(9000.f);
	Flash->SetAttenuationRadius(450.f);
	Flash->SetCastShadows(false);
}

AOSFTracer* AOSFTracer::Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bImpact, UMaterialInterface* Material)
{
	if (!World)
	{
		return nullptr;
	}
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Cylinder is 100 units tall along Z: rotate Z onto the shot direction and centre it on the segment.
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta / Length).Rotator();
	AOSFTracer* Tracer = World->SpawnActor<AOSFTracer>(AOSFTracer::StaticClass(), FTransform(Rotation, Start + Delta * 0.5f), Params);
	if (!Tracer)
	{
		return nullptr;
	}
	if (Material)
	{
		Tracer->Beam->SetMaterial(0, Material);
		Tracer->Spark->SetMaterial(0, Material);
	}
	Tracer->BeamScale = FVector(0.025f, 0.025f, Length / 100.f);
	Tracer->Beam->SetRelativeScale3D(Tracer->BeamScale);
	if (UMaterialInstanceDynamic* Material = Tracer->Beam->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color * 4.f);
	}
	Tracer->Spark->SetWorldLocation(End);
	Tracer->Spark->SetWorldScale3D(FVector(bImpact ? 0.22f : 0.f));
	if (UMaterialInstanceDynamic* SparkMaterial = Tracer->Spark->CreateDynamicMaterialInstance(0))
	{
		SparkMaterial->SetVectorParameterValue(TEXT("Color"), Color * 6.f);
	}
	Tracer->Flash->SetWorldLocation(Start);
	Tracer->Flash->SetLightColor(Color);
	return Tracer;
}

void AOSFTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Life += DeltaSeconds;
	const float Alpha = FMath::Clamp(1.f - Life / MaxLife, 0.f, 1.f);
	Beam->SetRelativeScale3D(FVector(BeamScale.X * Alpha, BeamScale.Y * Alpha, BeamScale.Z));
	Flash->SetIntensity(9000.f * Alpha);
	if (Life >= MaxLife * 1.5f)
	{
		Destroy();
	}
}
