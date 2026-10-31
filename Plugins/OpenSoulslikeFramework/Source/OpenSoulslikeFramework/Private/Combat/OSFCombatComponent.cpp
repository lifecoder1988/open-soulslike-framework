// Copyright Open Soulslike Framework contributors. MIT License.

#include "Combat/OSFCombatComponent.h"
#include "Actions/OSFActionComponent.h"
#include "Actions/OSFActions.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFBuffComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFItemData.h"
#include "OSFGameplayTags.h"
#include "OpenSoulslikeFramework.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UOSFCombatComponent::UOSFCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

double UOSFCombatComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

UOSFActionComponent* UOSFCombatComponent::GetActionComponent() const
{
	return OSF::Get<UOSFActionComponent>(GetOwner());
}

UOSFStatComponent* UOSFCombatComponent::GetStatComponent() const
{
	return OSF::Get<UOSFStatComponent>(GetOwner());
}

void UOSFCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveStunTag.IsValid() && Now() >= StunEndTime)
	{
		if (UOSFActionComponent* Actions = GetActionComponent())
		{
			Actions->ClearStateTag(ActiveStunTag);
		}
		ActiveStunTag = FGameplayTag();
	}
}

bool UOSFCombatComponent::IsSameTeam(const AActor* Other) const
{
	const UOSFCombatComponent* OtherCombat = OSF::Get<UOSFCombatComponent>(Other);
	return OtherCombat && OtherCombat != this && OtherCombat->TeamId == TeamId;
}

bool UOSFCombatComponent::IsFacing(const AActor* Other) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Other)
	{
		return false;
	}
	const FVector ToOther = (Other->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	const float CosAngle = FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), ToOther);
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(GuardHalfAngle));
}

float UOSFCombatComponent::GetGuardAbsorption(FGameplayTag DamageType, float& OutGuardBoost) const
{
	OutGuardBoost = 0.f;
	const UOSFEquipmentComponent* Equipment = OSF::Get<UOSFEquipmentComponent>(GetOwner());
	const UOSFWeaponData* GuardWeapon = nullptr;
	if (Equipment)
	{
		GuardWeapon = Equipment->IsTwoHanding() ? Equipment->GetEquippedWeapon(true) : Equipment->GetEquippedWeapon(false);
		if (!GuardWeapon)
		{
			GuardWeapon = Equipment->GetEquippedWeapon(true);
		}
	}
	if (!GuardWeapon)
	{
		return UnarmedGuardAbsorption;
	}
	OutGuardBoost = GuardWeapon->GuardBoost;
	const float* Absorption = GuardWeapon->GuardAbsorption.Find(DamageType);
	return Absorption ? FMath::Clamp(*Absorption, 0.f, 1.f) : UnarmedGuardAbsorption;
}

float UOSFCombatComponent::GetDamageNegation(FGameplayTag DamageType) const
{
	float Remaining = 1.f;
	if (const UOSFEquipmentComponent* Equipment = OSF::Get<UOSFEquipmentComponent>(GetOwner()))
	{
		Remaining *= 1.f - Equipment->GetArmorNegation(DamageType);
	}
	if (const float* Natural = NaturalNegation.Find(DamageType))
	{
		Remaining *= 1.f - FMath::Clamp(*Natural / 100.f, 0.f, 1.f);
	}
	if (const UOSFStatComponent* Stats = GetStatComponent())
	{
		// Talismans / buffs can add negation in percent with an OSF.Damage.* modifier.
		Remaining *= 1.f - FMath::Clamp(Stats->GetModifierAdditive(DamageType) / 100.f, 0.f, 1.f);
	}
	return FMath::Clamp(1.f - Remaining, 0.f, 0.95f);
}

FOSFHitInfo UOSFCombatComponent::ReceiveDamage(const FOSFDamageEvent& Event)
{
	FOSFHitInfo Info;
	Info.Instigator = Event.Instigator;
	UOSFActionComponent* Actions = GetActionComponent();
	UOSFStatComponent* Stats = GetStatComponent();
	if (bDead || !Actions || !Stats)
	{
		return Info;
	}
	const FOSFDamageSpec& Spec = Event.Spec;
	if (Event.Instigator && IsSameTeam(Event.Instigator))
	{
		return Info;
	}
	if (!Spec.bIsCritical && (Actions->HasStateTag(OSFTags::State_Invincible) || Actions->HasStateTag(OSFTags::State_BeingExecuted)))
	{
		return Info;
	}

	AActor* Attacker = Event.Instigator;
	const bool bFacing = Attacker ? IsFacing(Attacker) : true;
	LastDamageTime = static_cast<float>(Now());

	// --- Parry ---
	if (!Spec.bIsCritical && Spec.bCanBeParried && bFacing && Actions->HasStateTag(OSFTags::State_Parrying))
	{
		if (UOSFCombatComponent* AttackerCombat = OSF::Get<UOSFCombatComponent>(Attacker))
		{
			AttackerCombat->NotifyParried(GetOwner());
		}
		Info.Result = EOSFHitResult::Parried;
		OnDamaged.Broadcast(this, Info);
		return Info;
	}

	float TotalDamage = 0.f;
	const float DamageTakenMultiplier = OSF::Get<UOSFBuffComponent>(GetOwner()) ? OSF::Get<UOSFBuffComponent>(GetOwner())->GetDamageTakenMultiplier() : 1.f;
	for (const TPair<FGameplayTag, float>& Pair : Spec.Damage)
	{
		TotalDamage += Pair.Value * (1.f - GetDamageNegation(Pair.Key)) * DamageTakenMultiplier;
	}

	// --- Guard ---
	if (!Spec.bIsCritical && Spec.bCanBeGuarded && bFacing && Actions->HasStateTag(OSFTags::State_Guarding))
	{
		const UOSFAction_Guard* Guard = Cast<UOSFAction_Guard>(Actions->GetCurrentAction());
		if (Guard && Guard->IsPerfectGuardWindow())
		{
			Info.Result = EOSFHitResult::PerfectGuarded;
			// A perfect guard staggers the attacker's poise a little.
			if (UOSFStatComponent* AttackerStats = OSF::Get<UOSFStatComponent>(Attacker))
			{
				AttackerStats->ModifyCurrent(OSFTags::Stat_Poise, -Spec.PoiseDamage * 0.5f, true);
			}
			OnDamaged.Broadcast(this, Info);
			return Info;
		}

		float GuardedDamage = 0.f;
		float GuardBoost = 0.f;
		for (const TPair<FGameplayTag, float>& Pair : Spec.Damage)
		{
			const float Absorb = GetGuardAbsorption(Pair.Key, GuardBoost);
			GuardedDamage += Pair.Value * (1.f - Absorb) * (1.f - GetDamageNegation(Pair.Key)) * DamageTakenMultiplier;
		}
		const float StaminaDamage = Spec.GetTotalDamage() * Spec.GuardStaminaDamageScale * (1.f - GuardBoost);
		const float Stamina = Stats->GetCurrent(OSFTags::Stat_Stamina);
		if (StaminaDamage >= Stamina)
		{
			Stats->ModifyCurrent(OSFTags::Stat_Stamina, -Stamina, true);
			Info.Result = EOSFHitResult::GuardBroken;
			Info.Reaction = EOSFHitReaction::GuardBreak;
			TotalDamage = GuardedDamage;
		}
		else
		{
			Stats->ModifyCurrent(OSFTags::Stat_Stamina, -StaminaDamage, true);
			Info.Result = EOSFHitResult::Guarded;
			TotalDamage = GuardedDamage;
		}

		Info.DamageDealt = TotalDamage;
		Stats->ModifyCurrent(OSFTags::Stat_Health, -TotalDamage);
		if (Stats->GetCurrent(OSFTags::Stat_Health) <= 0.f)
		{
			Info.Result = EOSFHitResult::Killed;
			Info.Reaction = EOSFHitReaction::None;
			OnDamaged.Broadcast(this, Info);
			Die(Attacker);
			return Info;
		}
		if (Info.Reaction == EOSFHitReaction::GuardBreak)
		{
			ApplyReaction(EOSFHitReaction::GuardBreak);
		}
		OnDamaged.Broadcast(this, Info);
		return Info;
	}

	// --- Clean hit ---
	Info.Result = EOSFHitResult::Hit;
	Info.DamageDealt = TotalDamage;
	Stats->ModifyCurrent(OSFTags::Stat_Health, -TotalDamage);

	if (UOSFStatusEffectComponent* Status = OSF::Get<UOSFStatusEffectComponent>(GetOwner()))
	{
		for (const TPair<FGameplayTag, float>& Pair : Spec.StatusBuildup)
		{
			Status->AddBuildup(Pair.Key, Pair.Value);
		}
	}

	if (Stats->GetCurrent(OSFTags::Stat_Health) <= 0.f)
	{
		Info.Result = EOSFHitResult::Killed;
		OnDamaged.Broadcast(this, Info);
		Die(Attacker);
		return Info;
	}

	// --- Poise ---
	if (!Spec.bIsCritical && !Actions->HasStateTag(OSFTags::State_HyperArmor) && Spec.PoiseDamage > 0.f && Stats->HasStat(OSFTags::Stat_Poise))
	{
		Stats->ModifyCurrent(OSFTags::Stat_Poise, -Spec.PoiseDamage, true);
		if (Stats->GetCurrent(OSFTags::Stat_Poise) <= 0.f)
		{
			Stats->SetCurrent(OSFTags::Stat_Poise, Stats->GetMax(OSFTags::Stat_Poise));
			Info.Reaction = (bPoiseBreakOpensRiposte && bCanBeRiposted) ? EOSFHitReaction::PoiseBreak : EOSFHitReaction::Stagger;
		}
		else
		{
			Info.Reaction = EOSFHitReaction::Flinch;
		}
	}

	if (Info.Reaction != EOSFHitReaction::None)
	{
		ApplyReaction(Info.Reaction);
	}
	OnDamaged.Broadcast(this, Info);
	return Info;
}

void UOSFCombatComponent::ApplyDirectDamage(float Amount, AActor* Causer)
{
	UOSFStatComponent* Stats = GetStatComponent();
	if (bDead || !Stats || Amount <= 0.f)
	{
		return;
	}
	Stats->ModifyCurrent(OSFTags::Stat_Health, -Amount);
	if (Stats->GetCurrent(OSFTags::Stat_Health) <= 0.f)
	{
		Die(Causer);
	}
}

void UOSFCombatComponent::Kill(AActor* Killer)
{
	if (UOSFStatComponent* Stats = GetStatComponent())
	{
		Stats->SetCurrent(OSFTags::Stat_Health, 0.f);
	}
	Die(Killer);
}

void UOSFCombatComponent::Die(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	if (UOSFActionComponent* Actions = GetActionComponent())
	{
		Actions->CancelCurrentAction();
		if (ActiveStunTag.IsValid())
		{
			Actions->ClearStateTag(ActiveStunTag);
			ActiveStunTag = FGameplayTag();
		}
		Actions->ClearStateTag(OSFTags::State_BeingExecuted);
		Actions->AddStateTag(OSFTags::State_Dead);
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->DisableMovement();
		Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		if (DeathMontage)
		{
			Character->PlayAnimMontage(DeathMontage);
		}
	}
	UE_LOG(LogOSF, Log, TEXT("%s died (killer: %s)"), *GetNameSafe(GetOwner()), *GetNameSafe(Killer));
	OnDeath.Broadcast(this, Killer);
}

void UOSFCombatComponent::Revive()
{
	bDead = false;
	ActiveStunTag = FGameplayTag();
	if (UOSFActionComponent* Actions = GetActionComponent())
	{
		Actions->CancelCurrentAction();
		for (const FGameplayTag& Tag : { OSFTags::State_Dead.GetTag(), OSFTags::State_Staggered.GetTag(), OSFTags::State_PoiseBroken.GetTag(), OSFTags::State_BeingExecuted.GetTag() })
		{
			Actions->ClearStateTag(Tag);
		}
	}
	if (UOSFStatComponent* Stats = GetStatComponent())
	{
		Stats->RestoreAll();
	}
	if (UOSFStatusEffectComponent* Status = OSF::Get<UOSFStatusEffectComponent>(GetOwner()))
	{
		Status->CureAll();
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->StopAllMontages(0.f);
		}
	}
}

void UOSFCombatComponent::ApplyReaction(EOSFHitReaction Reaction, float DurationOverride)
{
	UOSFActionComponent* Actions = GetActionComponent();
	if (!Actions || bDead || Reaction == EOSFHitReaction::None)
	{
		return;
	}

	if (Reaction == EOSFHitReaction::Flinch)
	{
		// Flinches only interrupt item use / guard-less idle actions, never attacks with hyper armor.
		OnHitReaction.Broadcast(this, Reaction);
		return;
	}

	float Duration = StaggerDuration;
	FGameplayTag Tag = OSFTags::State_Staggered;
	switch (Reaction)
	{
	case EOSFHitReaction::GuardBreak:
		Duration = GuardBreakDuration;
		break;
	case EOSFHitReaction::PoiseBreak:
	case EOSFHitReaction::Parried:
		Duration = RiposteWindow;
		Tag = bCanBeRiposted ? OSFTags::State_PoiseBroken : OSFTags::State_Staggered;
		break;
	default:
		break;
	}
	if (DurationOverride >= 0.f)
	{
		Duration = DurationOverride;
	}

	Actions->CancelCurrentAction();
	if (ActiveStunTag.IsValid())
	{
		Actions->ClearStateTag(ActiveStunTag);
	}
	ActiveStunTag = Tag;
	Actions->AddStateTag(Tag);
	StunEndTime = Now() + Duration;

	if (const TObjectPtr<UAnimMontage>* Montage = ReactionMontages.Find(Reaction))
	{
		PlayReactionMontage(*Montage);
	}
	OnHitReaction.Broadcast(this, Reaction);
}

void UOSFCombatComponent::PlayReactionMontage(UAnimMontage* Montage)
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (Montage)
		{
			Character->PlayAnimMontage(Montage);
		}
	}
}

void UOSFCombatComponent::NotifyParried(AActor* Parrier)
{
	ApplyReaction(EOSFHitReaction::Parried);
}

bool UOSFCombatComponent::CanBeRiposted() const
{
	const UOSFActionComponent* Actions = GetActionComponent();
	return !bDead && bCanBeRiposted && Actions && Actions->HasStateTag(OSFTags::State_PoiseBroken) && !Actions->HasStateTag(OSFTags::State_BeingExecuted);
}

bool UOSFCombatComponent::CanBeBackstabbedBy(const AActor* Attacker) const
{
	const AActor* Owner = GetOwner();
	const UOSFActionComponent* Actions = GetActionComponent();
	if (bDead || !bCanBeBackstabbed || !Owner || !Attacker || !Actions || Actions->HasStateTag(OSFTags::State_BeingExecuted))
	{
		return false;
	}
	// Can't backstab a character that is mid-attack with hyper armor.
	if (Actions->HasStateTag(OSFTags::State_HyperArmor))
	{
		return false;
	}
	const FVector ToAttacker = (Attacker->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	const bool bBehind = FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), ToAttacker) < -0.6f;
	const bool bAttackerFacing = FVector::DotProduct(Attacker->GetActorForwardVector().GetSafeNormal2D(), -ToAttacker) > 0.6f;
	return bBehind && bAttackerFacing;
}

AActor* UOSFCombatComponent::FindFinisherTarget(bool& bOutBackstab) const
{
	bOutBackstab = false;
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSFFinisher), false, Owner);
	World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(FinisherSearchRadius), Params);

	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const UOSFCombatComponent* CandidateCombat = OSF::Get<UOSFCombatComponent>(Candidate);
		if (!Candidate || Candidate == Owner || !CandidateCombat || CandidateCombat->IsDead() || IsSameTeam(Candidate))
		{
			continue;
		}
		const FVector ToCandidate = (Candidate->GetActorLocation() - Owner->GetActorLocation());
		const float Distance = ToCandidate.Size2D();
		const bool bInFront = FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), ToCandidate.GetSafeNormal2D()) > 0.5f;
		if (!bInFront || Distance >= BestDistance)
		{
			continue;
		}
		if (CandidateCombat->CanBeRiposted())
		{
			Best = Candidate;
			BestDistance = Distance;
			bOutBackstab = false;
		}
		else if (Distance < FinisherSearchRadius * 0.75f && CandidateCombat->CanBeBackstabbedBy(Owner))
		{
			Best = Candidate;
			BestDistance = Distance;
			bOutBackstab = true;
		}
	}
	return Best;
}

void UOSFCombatComponent::BeginBeingExecuted(AActor* Executor)
{
	UOSFActionComponent* Actions = GetActionComponent();
	if (!Actions)
	{
		return;
	}
	CurrentExecutor = Executor;
	Actions->CancelCurrentAction();
	if (ActiveStunTag.IsValid())
	{
		Actions->ClearStateTag(ActiveStunTag);
		ActiveStunTag = FGameplayTag();
	}
	Actions->AddStateTag(OSFTags::State_BeingExecuted);
	if (BeingExecutedMontage)
	{
		PlayReactionMontage(BeingExecutedMontage);
	}
}

void UOSFCombatComponent::EndBeingExecuted()
{
	CurrentExecutor.Reset();
	if (UOSFActionComponent* Actions = GetActionComponent())
	{
		Actions->ClearStateTag(OSFTags::State_BeingExecuted);
		if (!bDead)
		{
			// Short get-up stagger after surviving a finisher.
			ApplyReaction(EOSFHitReaction::Stagger, 0.6f);
		}
	}
}
