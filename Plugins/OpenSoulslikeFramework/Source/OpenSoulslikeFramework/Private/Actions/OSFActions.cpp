// Copyright Open Soulslike Framework contributors. MIT License.

#include "Actions/OSFActions.h"
#include "Actions/OSFActionComponent.h"
#include "Characters/OSFCharacterBase.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFCombatLibrary.h"
#include "Combat/OSFWeaponTraceComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Items/OSFWeaponActor.h"
#include "Stats/OSFStatComponent.h"
#include "OSFGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ============================================================================
// Attack
// ============================================================================

UOSFAction_Attack::UOSFAction_Attack()
{
	GrantedTags.AddTag(OSFTags::State_Attacking);
	CancelledByActions.AddTag(OSFTags::Action_Dodge);
	bCancellableInCancelWindow = true;
	MovementScale = 0.f;
	WindupRotationRate = 720.f;

	FallbackAttack.Timing.Windup = 0.3f;
	FallbackAttack.Timing.Active = 0.15f;
	FallbackAttack.Timing.Recovery = 0.4f;
	FallbackAttack.StaminaCost = 12.f;
	FallbackAttack.Reach = 160.f;
}

UOSFAction_LightAttack::UOSFAction_LightAttack()
{
	ActionTag = OSFTags::Action_LightAttack;
	BaseType = EOSFAttackType::Light;
}

UOSFAction_HeavyAttack::UOSFAction_HeavyAttack()
{
	ActionTag = OSFTags::Action_HeavyAttack;
	BaseType = EOSFAttackType::Heavy;
}

UOSFAction_WeaponSkill::UOSFAction_WeaponSkill()
{
	ActionTag = OSFTags::Action_WeaponSkill;
	BaseType = EOSFAttackType::Skill;
}

UOSFAction_AIAbility::UOSFAction_AIAbility()
{
	ActionTag = OSFTags::Action_AIAbility;
	BaseType = EOSFAttackType::Light;
	bCanBeBuffered = false;
	// AI attacks shouldn't be cancelled into dodges by default (the AI decides explicitly).
	CancelledByActions.Reset();
}

UOSFAction* UOSFAction_Attack::GetRedirect()
{
	if (BaseType != EOSFAttackType::Light || !ActionComponent)
	{
		return nullptr;
	}
	// Light attack next to a staggered / unaware enemy becomes a riposte / backstab.
	if (const UOSFCombatComponent* Combat = OSF::Get<UOSFCombatComponent>(ActionComponent->GetOwner()))
	{
		bool bBackstab = false;
		if (AActor* Target = Combat->FindFinisherTarget(bBackstab))
		{
			if (UOSFAction_Execute* Execute = Cast<UOSFAction_Execute>(ActionComponent->FindAction(OSFTags::Action_Execute)))
			{
				Execute->SetTarget(Target, bBackstab);
				if (Execute->CanActivate())
				{
					return Execute;
				}
			}
		}
	}
	return nullptr;
}

UOSFAction* UOSFAction_WeaponSkill::GetRedirect()
{
	if (!ActionComponent)
	{
		return nullptr;
	}
	const UOSFEquipmentComponent* Equipment = OSF::Get<UOSFEquipmentComponent>(ActionComponent->GetOwner());
	if (Equipment && !Equipment->IsTwoHanding())
	{
		const UOSFWeaponData* Left = Equipment->GetEquippedWeapon(false);
		if (Left && Left->bCanParry)
		{
			return ActionComponent->FindAction(OSFTags::Action_Parry);
		}
	}
	return nullptr;
}

UOSFWeaponData* UOSFAction_Attack::GetAttackingWeapon(bool& bOutRightHand) const
{
	bOutRightHand = true;
	const UOSFEquipmentComponent* Equipment = ActionComponent ? OSF::Get<UOSFEquipmentComponent>(ActionComponent->GetOwner()) : nullptr;
	return Equipment ? Equipment->GetActiveWeapon(true) : nullptr;
}

const FOSFAttackDefinition* UOSFAction_Attack::ResolveAttack(EOSFAttackType& OutType, int32& OutComboIndex) const
{
	OutType = BaseType;
	OutComboIndex = 0;
	if (AttackOverride.IsSet())
	{
		return &AttackOverride.GetValue();
	}

	const ACharacter* Character = GetCharacter();
	if (Character && (BaseType == EOSFAttackType::Light || BaseType == EOSFAttackType::Heavy))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->IsFalling())
		{
			OutType = EOSFAttackType::Jumping;
		}
		else if (ActionComponent->HasStateTag(OSFTags::State_Sprinting) && Character->GetVelocity().Size2D() > 300.f)
		{
			OutType = EOSFAttackType::Running;
		}
	}

	bool bRight = true;
	const UOSFWeaponData* Weapon = GetAttackingWeapon(bRight);
	if (OutType == BaseType && (BaseType == EOSFAttackType::Light || BaseType == EOSFAttackType::Heavy))
	{
		// Continue the combo if the previous attack of the same kind just ended (or was chained in its cancel window).
		const UOSFAction_Attack* Last = Cast<UOSFAction_Attack>(ActionComponent->LastFinishedAction.Get());
		const double Since = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - ActionComponent->LastActionEndTime;
		if (Last == this && Last->CurrentType == BaseType && Since <= ComboResetTime)
		{
			const int32 Length = Weapon ? FMath::Max(1, Weapon->GetComboLength(BaseType)) : 1;
			OutComboIndex = (ComboIndex + 1) % Length;
		}
	}

	if (Weapon)
	{
		if (const FOSFAttackDefinition* Def = Weapon->GetAttack(OutType, OutComboIndex))
		{
			return Def;
		}
	}
	return &FallbackAttack;
}

bool UOSFAction_Attack::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	EOSFAttackType Type;
	int32 Index;
	const FOSFAttackDefinition* Def = ResolveAttack(Type, Index);
	if (!Def)
	{
		return false;
	}
	if (const UOSFStatComponent* Stats = GetStats())
	{
		if (Def->FPCost > 0.f && Stats->GetCurrent(OSFTags::Stat_FP) < Def->FPCost)
		{
			return false;
		}
	}
	return true;
}

float UOSFAction_Attack::GetStaminaCost() const
{
	return CurrentAttack.StaminaCost;
}

float UOSFAction_Attack::GetFPCost() const
{
	return CurrentAttack.FPCost;
}

void UOSFAction_Attack::OnPreStart()
{
	EOSFAttackType Type;
	int32 Index;
	const FOSFAttackDefinition* Def = ResolveAttack(Type, Index);
	CurrentAttack = Def ? *Def : FallbackAttack;
	CurrentType = Type;
	ComboIndex = Index;
	CurrentWeapon = GetAttackingWeapon(bRightHand);
	AttackOverride.Reset();
}

void UOSFAction_Attack::OnStarted()
{
	bLungeDone = false;
	if (CurrentAttack.bHyperArmor)
	{
		SetWindow(EOSFActionWindow::HyperArmor, true);
	}
	if (CurrentAttack.bPerilous)
	{
		ActionComponent->AddStateTag(OSFTags::State_Perilous);
	}
	if (CurrentWeapon && CurrentWeapon->SwingSound && GetCharacter())
	{
		UGameplayStatics::PlaySoundAtLocation(this, CurrentWeapon->SwingSound, GetCharacter()->GetActorLocation(), 1.f, FMath::FRandRange(0.93f, 1.07f));
	}
	if (CurrentAttack.StartSound && GetCharacter())
	{
		UGameplayStatics::PlaySoundAtLocation(this, CurrentAttack.StartSound, GetCharacter()->GetActorLocation());
	}
	if (CurrentAttack.StartEffect.IsSet() && GetCharacter())
	{
		UOSFCombatLibrary::SpawnEffect(this, CurrentAttack.StartEffect, GetCharacter()->GetActorLocation(), GetCharacter()->GetActorRotation(), GetCharacter());
	}
}

void UOSFAction_Attack::OnPhaseChanged(EOSFActionPhase NewPhase)
{
	if (NewPhase != EOSFActionPhase::Windup && CurrentAttack.bPerilous)
	{
		ActionComponent->ClearStateTag(OSFTags::State_Perilous);
	}
	if (NewPhase == EOSFActionPhase::Recovery && CurrentAttack.bHyperArmor && IsProcedural())
	{
		SetWindow(EOSFActionWindow::HyperArmor, false);
	}
}

void UOSFAction_Attack::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window != EOSFActionWindow::Active || !ActionComponent)
	{
		return;
	}
	AActor* Owner = ActionComponent->GetOwner();
	UOSFWeaponTraceComponent* Trace = OSF::Get<UOSFWeaponTraceComponent>(Owner);
	if (!Trace)
	{
		return;
	}
	if (bOpen)
	{
		const UOSFEquipmentComponent* Equipment = OSF::Get<UOSFEquipmentComponent>(Owner);
		const bool bTwoHanded = Equipment && Equipment->IsTwoHanding();
		const FOSFDamageSpec Spec = UOSFCombatLibrary::MakeAttackDamageSpec(Owner, CurrentWeapon, CurrentAttack, bTwoHanded);
		AOSFWeaponActor* WeaponActor = Equipment ? Equipment->GetWeaponActor(bRightHand) : nullptr;
		Trace->BeginTrace(Spec, CurrentAttack.Reach, CurrentAttack.TraceRadius, WeaponActor);

		if (CurrentAttack.Lunge > 0.f && IsProcedural() && !bLungeDone)
		{
			bLungeDone = true;
			ACharacter* Character = GetCharacter();
			bool bTooClose = false;
			if (const AOSFCharacterBase* OSFCharacter = Cast<AOSFCharacterBase>(Character))
			{
				if (const AActor* Focus = OSFCharacter->GetFocusTarget())
				{
					bTooClose = FVector::Dist2D(Focus->GetActorLocation(), Character->GetActorLocation()) < 150.f;
				}
			}
			if (Character && !bTooClose)
			{
				LungeRootMotionId = UOSFCombatLibrary::ApplyMovementBurst(Character, Character->GetActorForwardVector(), CurrentAttack.Lunge * 0.5f, FMath::Max(0.05f, GetEffectiveTiming().Active), TEXT("OSFLunge"));
			}
		}
	}
	else
	{
		Trace->EndTrace();
	}
}

void UOSFAction_Attack::OnTick(float DeltaTime)
{
	AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter());
	if (!Character || !IsProcedural())
	{
		return;
	}
	// Clip mode: step in during the last three quarters of the windup, arriving on the hit frame.
	const float Impact = GetClipImpactTime();
	if (Impact > 0.f && !bLungeDone && CurrentAttack.Lunge > 0.f && Elapsed >= Impact * 0.25f)
	{
		bLungeDone = true;
		bool bTooClose = false;
		if (const AActor* Focus = Character->GetFocusTarget())
		{
			bTooClose = FVector::Dist2D(Focus->GetActorLocation(), Character->GetActorLocation()) < 150.f;
		}
		if (!bTooClose)
		{
			LungeRootMotionId = UOSFCombatLibrary::ApplyMovementBurst(Character, Character->GetActorForwardVector(), CurrentAttack.Lunge * 0.5f, FMath::Max(0.05f, Impact * 0.75f), TEXT("OSFLunge"));
		}
	}
	// Procedural swing: raise back during windup, sweep during active, return during recovery.
	const FOSFActionTiming& T = GetEffectiveTiming();
	float Alpha = 0.f;
	if (Elapsed < T.Windup)
	{
		Alpha = -0.45f * FMath::InterpEaseOut(0.f, 1.f, Elapsed / FMath::Max(T.Windup, KINDA_SMALL_NUMBER), 2.f);
	}
	else if (Elapsed < T.Windup + T.Active)
	{
		const float A = (Elapsed - T.Windup) / FMath::Max(T.Active, KINDA_SMALL_NUMBER);
		Alpha = FMath::Lerp(-0.45f, 0.55f, A);
	}
	else
	{
		const float A = (Elapsed - T.Windup - T.Active) / FMath::Max(T.Recovery, KINDA_SMALL_NUMBER);
		Alpha = FMath::Lerp(0.55f, 0.f, FMath::Clamp(A, 0.f, 1.f));
	}
	Character->SetProceduralSwing(CurrentAttack.SwingYaw * Alpha, CurrentAttack.SwingPitch * Alpha);
}

void UOSFAction_Attack::OnEnded(bool bInterrupted)
{
	if (ActionComponent)
	{
		ActionComponent->ClearStateTag(OSFTags::State_Perilous);
		if (UOSFWeaponTraceComponent* Trace = OSF::Get<UOSFWeaponTraceComponent>(ActionComponent->GetOwner()))
		{
			Trace->EndTrace();
		}
	}
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		Character->SetProceduralSwing(0.f, 0.f);
		if (bInterrupted)
		{
			UOSFCombatLibrary::StopMovementBurst(Character, LungeRootMotionId);
		}
	}
	LungeRootMotionId = 0;
}

// ============================================================================
// Dodge
// ============================================================================

UOSFAction_Dodge::UOSFAction_Dodge()
{
	ActionTag = OSFTags::Action_Dodge;
	StaminaCost = 16.f;
	GrantedTags.AddTag(OSFTags::State_Dodging);
	MovementScale = 0.f;
	WindupRotationRate = 0.f;
	Timing.Windup = 0.04f;
	Timing.Active = 0.4f;
	Timing.Recovery = 0.3f;
	Timing.CancelWindowStart = 0.35f;
	bFitClipToTiming = true;	// the roll clip follows the dodge's timing, not the other way round
}

const FOSFAnimClip* UOSFAction_Dodge::GetClipToPlay() const
{
	if (const AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		const FOSFAnimClip& Stance = Character->GetDodgeClip(bBackstep);
		if (Stance.IsValid())
		{
			return &Stance;
		}
	}
	return Super::GetClipToPlay();
}

bool UOSFAction_Dodge::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	const UOSFEquipmentComponent* Equipment = ActionComponent ? OSF::Get<UOSFEquipmentComponent>(ActionComponent->GetOwner()) : nullptr;
	const ACharacter* Character = GetCharacter();
	if (Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling())
	{
		return false;
	}
	return !Equipment || Equipment->GetEquipLoadCategory() != EOSFEquipLoad::Overloaded;
}

void UOSFAction_Dodge::OnPreStart()
{
	ACharacter* Character = GetCharacter();
	FVector Input = FVector::ZeroVector;
	if (const AOSFCharacterBase* OSFCharacter = Cast<AOSFCharacterBase>(Character))
	{
		// Raw movement intent (not the facing direction, which follows the lock-on target).
		Input = OSFCharacter->GetDesiredMoveDirection();
	}
	else if (Character)
	{
		Input = Character->GetLastMovementInputVector();
	}
	Input.Z = 0.f;

	const UOSFEquipmentComponent* Equipment = Character ? OSF::Get<UOSFEquipmentComponent>(Character) : nullptr;
	const int32 Load = Equipment ? FMath::Clamp(static_cast<int32>(Equipment->GetEquipLoadCategory()), 0, 2) : 0;

	CurrentTiming = Timing;
	bBackstep = Input.IsNearlyZero();
	if (bBackstep)
	{
		Direction = Character ? -Character->GetActorForwardVector() : FVector::BackwardVector;
		Distance = BackstepDistance;
		CurrentTiming.Active = BackstepIFrames;
		CurrentTiming.Recovery = 0.25f;
	}
	else
	{
		Direction = Input.GetSafeNormal();
		Distance = RollDistance[Load];
		CurrentTiming.Active = RollIFrames[Load];
		CurrentTiming.Recovery = 0.3f + 0.12f * Load;
		if (Character)
		{
			Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		}
	}
}

void UOSFAction_Dodge::OnStarted()
{
	if (const AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()); Character && Character->DodgeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Character->DodgeSound, Character->GetActorLocation(), 1.f, FMath::FRandRange(0.94f, 1.06f));
	}
	if (IsProcedural())
	{
		const float Duration = CurrentTiming.Windup + CurrentTiming.Active + CurrentTiming.Recovery * 0.4f;
		RootMotionId = UOSFCombatLibrary::ApplyMovementBurst(GetCharacter(), Direction, Distance, Duration, TEXT("OSFDodge"));
	}
}

void UOSFAction_Dodge::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	// Procedural timeline opens "Active": for a dodge that is the i-frame window.
	if (Window == EOSFActionWindow::Active)
	{
		SetWindow(EOSFActionWindow::Invincible, bOpen);
	}
}

void UOSFAction_Dodge::OnEnded(bool bInterrupted)
{
	if (bInterrupted)
	{
		UOSFCombatLibrary::StopMovementBurst(GetCharacter(), RootMotionId);
	}
	RootMotionId = 0;
}

// ============================================================================
// Guard / Parry
// ============================================================================

UOSFAction_Guard::UOSFAction_Guard()
{
	ActionTag = OSFTags::Action_Guard;
	bIsHoldAction = true;
	bCanBeBuffered = true;
	MovementScale = 1.f;
	WindupRotationRate = 0.f;
	GrantedTags.Reset();
	GrantedTags.AddTag(OSFTags::State_Guarding);
	// Guard can be cancelled by anything (guard counters, dodges, items...).
	Timing.Windup = 0.f;
	Timing.Active = 0.f;
	Timing.Recovery = 0.f;
	Timing.CancelWindowStart = 0.f;
}

const FOSFAnimClip* UOSFAction_Guard::GetClipToPlay() const
{
	if (const AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		const FOSFAnimClip& Stance = Character->GetGuardClip();
		if (Stance.IsValid())
		{
			return &Stance;
		}
	}
	return Super::GetClipToPlay();
}

UOSFAction_Parry::UOSFAction_Parry()
{
	ActionTag = OSFTags::Action_Parry;
	StaminaCost = 10.f;
	GrantedTags.AddTag(OSFTags::State_Attacking);
	WindupRotationRate = 540.f;
	Timing.Windup = 0.08f;
	Timing.Active = 0.25f;
	Timing.Recovery = 0.55f;
	Timing.CancelWindowStart = 0.8f;
}

void UOSFAction_Parry::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window == EOSFActionWindow::Active)
	{
		SetWindow(EOSFActionWindow::Parry, bOpen);
	}
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		if (IsProcedural() && Window == EOSFActionWindow::Active)
		{
			Character->SetProceduralSwing(0.f, 0.f, bOpen ? -60.f : 0.f);
		}
	}
}

// ============================================================================
// Use item
// ============================================================================

UOSFAction_UseItem::UOSFAction_UseItem()
{
	ActionTag = OSFTags::Action_UseItem;
	GrantedTags.AddTag(OSFTags::State_UsingItem);
	MovementScale = 0.35f;
	WindupRotationRate = 0.f;
	bCancellableInCancelWindow = true;
}

UOSFItemData* UOSFAction_UseItem::GetItemToUse() const
{
	if (ItemOverride)
	{
		return ItemOverride;
	}
	const UOSFEquipmentComponent* Equipment = ActionComponent ? OSF::Get<UOSFEquipmentComponent>(ActionComponent->GetOwner()) : nullptr;
	return Equipment ? Equipment->GetActiveQuickItem() : nullptr;
}

bool UOSFAction_UseItem::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	const UOSFInventoryComponent* Inventory = ActionComponent ? OSF::Get<UOSFInventoryComponent>(ActionComponent->GetOwner()) : nullptr;
	return Inventory && Inventory->CanUseItem(GetItemToUse());
}

UAnimMontage* UOSFAction_UseItem::GetMontageToPlay() const
{
	return UsingItem ? UsingItem->UseMontage.Get() : nullptr;
}

void UOSFAction_UseItem::OnPreStart()
{
	UsingItem = GetItemToUse();
	ItemOverride = nullptr;
	bConsumed = false;
	CurrentTiming = Timing;
	if (UsingItem)
	{
		CurrentTiming.Windup = UsingItem->UseDelay;
		CurrentTiming.Active = 0.05f;
		CurrentTiming.Recovery = FMath::Max(0.05f, UsingItem->UseDuration - UsingItem->UseDelay);
		CurrentTiming.CancelWindowStart = 0.6f;
		MovementScale = UsingItem->UseMoveSpeedScale;
	}
}

void UOSFAction_UseItem::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window == EOSFActionWindow::Active && bOpen && !bConsumed && UsingItem)
	{
		bConsumed = true;
		if (UsingItem->UseSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, UsingItem->UseSound, ActionComponent->GetOwner()->GetActorLocation());
		}
		if (UsingItem->UseEffect.IsSet())
		{
			AActor* User = ActionComponent->GetOwner();
			UOSFCombatLibrary::SpawnEffect(this, UsingItem->UseEffect, User->GetActorLocation(), User->GetActorRotation(), User);
		}
		if (UOSFInventoryComponent* Inventory = OSF::Get<UOSFInventoryComponent>(ActionComponent->GetOwner()))
		{
			Inventory->ConsumeItem(UsingItem);
		}
	}
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		if (IsProcedural() && Window == EOSFActionWindow::Active)
		{
			Character->SetProceduralSwing(0.f, bOpen ? 70.f : 0.f);
		}
	}
}

void UOSFAction_UseItem::OnEnded(bool bInterrupted)
{
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		Character->SetProceduralSwing(0.f, 0.f);
	}
	UsingItem = nullptr;
}

// ============================================================================
// Execute (riposte / backstab)
// ============================================================================

UOSFAction_Execute::UOSFAction_Execute()
{
	ActionTag = OSFTags::Action_Execute;
	bCanBeBuffered = false;
	bCancellableInCancelWindow = true;
	WindupRotationRate = 0.f;
	StaminaCost = 0.f;
	GrantedTags.AddTag(OSFTags::State_Executing);
	GrantedTags.AddTag(OSFTags::State_Invincible);
	GrantedTags.AddTag(OSFTags::State_Attacking);
	Timing.Windup = 0.55f;
	Timing.Active = 0.1f;
	Timing.Recovery = 1.0f;
	Timing.CancelWindowStart = 0.7f;
}

bool UOSFAction_Execute::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation() || !Target.IsValid())
	{
		return false;
	}
	const UOSFCombatComponent* TargetCombat = OSF::Get<UOSFCombatComponent>(Target.Get());
	return TargetCombat && !TargetCombat->IsDead();
}

UAnimMontage* UOSFAction_Execute::GetMontageToPlay() const
{
	const UOSFEquipmentComponent* Equipment = ActionComponent ? OSF::Get<UOSFEquipmentComponent>(ActionComponent->GetOwner()) : nullptr;
	const UOSFWeaponData* Weapon = Equipment ? Equipment->GetActiveWeapon(true) : nullptr;
	if (!Weapon)
	{
		return Montage;
	}
	return bBackstab ? Weapon->BackstabMontage.Get() : Weapon->RiposteMontage.Get();
}

void UOSFAction_Execute::OnPreStart()
{
	bDamageDealt = false;
	ACharacter* Character = GetCharacter();
	AActor* Victim = Target.Get();
	if (!Character || !Victim)
	{
		return;
	}

	// Snap into finisher position.
	const FVector VictimLocation = Victim->GetActorLocation();
	FVector NewLocation;
	FRotator NewRotation;
	if (bBackstab)
	{
		NewLocation = VictimLocation - Victim->GetActorForwardVector() * BackstabDistance;
		NewRotation = FRotator(0.f, Victim->GetActorRotation().Yaw, 0.f);
	}
	else
	{
		const FVector ToAttacker = (Character->GetActorLocation() - VictimLocation).GetSafeNormal2D();
		NewLocation = VictimLocation + ToAttacker * RiposteDistance;
		NewRotation = FRotator(0.f, (-ToAttacker).Rotation().Yaw, 0.f);
	}
	NewLocation.Z = Character->GetActorLocation().Z;
	Character->SetActorLocationAndRotation(NewLocation, NewRotation, true);

	if (UOSFCombatComponent* TargetCombat = OSF::Get<UOSFCombatComponent>(Victim))
	{
		TargetCombat->BeginBeingExecuted(Character);
	}
}

void UOSFAction_Execute::OnWindowChanged(EOSFActionWindow Window, bool bOpen)
{
	if (Window != EOSFActionWindow::Active || !bOpen || bDamageDealt)
	{
		return;
	}
	bDamageDealt = true;
	AActor* Owner = ActionComponent->GetOwner();
	UOSFCombatComponent* TargetCombat = OSF::Get<UOSFCombatComponent>(Target.Get());
	if (!TargetCombat)
	{
		return;
	}

	const UOSFEquipmentComponent* Equipment = OSF::Get<UOSFEquipmentComponent>(Owner);
	const UOSFWeaponData* Weapon = Equipment ? Equipment->GetActiveWeapon(true) : nullptr;
	FOSFAttackDefinition Def;
	Def.MotionValue = MotionValue;
	FOSFDamageSpec Spec = UOSFCombatLibrary::MakeAttackDamageSpec(Owner, Weapon, Def, Equipment && Equipment->IsTwoHanding());
	const float Crit = Weapon ? Weapon->CriticalMultiplier : 2.5f;
	for (TPair<FGameplayTag, float>& Pair : Spec.Damage)
	{
		Pair.Value *= Crit;
	}
	Spec.bIsCritical = true;
	Spec.bCanBeGuarded = false;
	Spec.bCanBeParried = false;
	// Finisher: the heaviest hit feel.
	Spec.HitFeel.HitStop = 0.12f;
	Spec.HitFeel.Knockback = 35.f;
	Spec.HitFeel.CameraShake = 7.f;
	Spec.HitFeel.CameraShakeDuration = 0.28f;
	Spec.HitFeel.Spark = 1.8f;

	FOSFDamageEvent Event;
	Event.Spec = Spec;
	Event.Instigator = Owner;
	Event.DamageCauser = Owner;
	Event.HitLocation = Target->GetActorLocation();
	const FOSFHitInfo Info = TargetCombat->ReceiveDamage(Event);
	if (UOSFCombatComponent* MyCombat = OSF::Get<UOSFCombatComponent>(Owner))
	{
		MyCombat->OnHitLanded.Broadcast(MyCombat, Target.Get(), Info);
	}

	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		if (IsProcedural())
		{
			Character->SetProceduralSwing(0.f, -40.f);
		}
	}
}

void UOSFAction_Execute::OnEnded(bool bInterrupted)
{
	if (UOSFCombatComponent* TargetCombat = OSF::Get<UOSFCombatComponent>(Target.Get()))
	{
		TargetCombat->EndBeingExecuted();
	}
	if (AOSFCharacterBase* Character = Cast<AOSFCharacterBase>(GetCharacter()))
	{
		Character->SetProceduralSwing(0.f, 0.f);
	}
	Target.Reset();
}
