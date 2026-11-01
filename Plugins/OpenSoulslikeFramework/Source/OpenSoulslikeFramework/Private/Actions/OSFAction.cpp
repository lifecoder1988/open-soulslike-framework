// Copyright Open Soulslike Framework contributors. MIT License.

#include "Actions/OSFAction.h"
#include "Actions/OSFActionComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Characters/OSFCharacterBase.h"
#include "OSFGameplayTags.h"
#include "Animation/OSFClipAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UOSFAction::UOSFAction()
{
	BlockedTags.AddTag(OSFTags::State_Dead);
	BlockedTags.AddTag(OSFTags::State_Staggered);
	BlockedTags.AddTag(OSFTags::State_PoiseBroken);
	BlockedTags.AddTag(OSFTags::State_BeingExecuted);
	BlockedTags.AddTag(OSFTags::State_InMenu);
	BlockedTags.AddTag(OSFTags::State_OnLadder);
	BlockedTags.AddTag(OSFTags::State_Resting);
	GrantedTags.AddTag(OSFTags::State_Busy);
}

UWorld* UOSFAction::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject) || !ActionComponent)
	{
		return nullptr;
	}
	return ActionComponent->GetWorld();
}

void UOSFAction::Initialize(UOSFActionComponent* InComponent)
{
	ActionComponent = InComponent;
}

ACharacter* UOSFAction::GetCharacter() const
{
	return ActionComponent ? Cast<ACharacter>(ActionComponent->GetOwner()) : nullptr;
}

UOSFStatComponent* UOSFAction::GetStats() const
{
	return ActionComponent ? OSF::Get<UOSFStatComponent>(ActionComponent->GetOwner()) : nullptr;
}

bool UOSFAction::CanActivate_Implementation() const
{
	if (!ActionComponent)
	{
		return false;
	}
	const FGameplayTagContainer& Tags = ActionComponent->GetStateTags();
	if (!Tags.HasAll(RequiredTags) || Tags.HasAny(BlockedTags))
	{
		return false;
	}
	if (const UOSFStatComponent* Stats = GetStats())
	{
		// Souls rule: stamina actions are allowed as long as the bar isn't empty.
		if (GetStaminaCost() > 0.f && Stats->HasStat(OSFTags::Stat_Stamina) && Stats->GetCurrent(OSFTags::Stat_Stamina) <= 0.f)
		{
			return false;
		}
		if (GetFPCost() > 0.f && Stats->HasStat(OSFTags::Stat_FP) && Stats->GetCurrent(OSFTags::Stat_FP) < GetFPCost())
		{
			return false;
		}
	}
	return true;
}

void UOSFAction::Start()
{
	bRunning = true;
	bEnding = false;
	Elapsed = 0.f;
	OpenWindows.Reset();
	Phase = EOSFActionPhase::None;

	OnPreStart();
	ActionComponent->AddStateTags(GrantedTags);

	if (UOSFStatComponent* Stats = GetStats())
	{
		Stats->TryConsume(OSFTags::Stat_Stamina, GetStaminaCost());
		Stats->TryConsume(OSFTags::Stat_FP, GetFPCost(), true);
	}

	bMontageMode = false;
	PlayingMontage = nullptr;
	if (UAnimMontage* ToPlay = GetMontageToPlay())
	{
		ACharacter* Character = GetCharacter();
		UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (AnimInstance && Character->PlayAnimMontage(ToPlay, GetMontagePlayRate(), GetMontageSection()) > 0.f)
		{
			bMontageMode = true;
			PlayingMontage = ToPlay;
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UOSFAction::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, ToPlay);
		}
	}

	bClipMode = false;
	if (!bMontageMode)
	{
		const FOSFAnimClip* ToPlay = GetClipToPlay();
		ACharacter* Character = GetCharacter();
		UOSFClipAnimInstance* ClipAnim = Character && Character->GetMesh() ? Cast<UOSFClipAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
		if (ToPlay && ToPlay->IsValid() && ClipAnim)
		{
			ClipTiming = GetTiming();
			const float Total = ClipTiming.GetTotal();
			const float ClipDuration = ToPlay->GetDuration();
			ClipAnim->PlayActionClip(*ToPlay, (bFitClipToTiming && !ToPlay->bLoop) ? Total : -1.f);
			if (!bFitClipToTiming && Total > KINDA_SMALL_NUMBER && ClipDuration > KINDA_SMALL_NUMBER && !ToPlay->bLoop)
			{
				const float Scale = ClipDuration / Total;
				ClipTiming.Windup *= Scale;
				ClipTiming.Active *= Scale;
				ClipTiming.Recovery *= Scale;
			}
			bClipMode = true;
		}
	}

	SetPhase(EOSFActionPhase::Windup);
	OnStarted();
	K2_OnStarted();

	if (bRunning && !bMontageMode)
	{
		UpdateProceduralTimeline();
	}
}

void UOSFAction::TickAction(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	Elapsed += DeltaTime;
	if (!bMontageMode)
	{
		UpdateProceduralTimeline();
	}
	if (bRunning && Phase == EOSFActionPhase::Windup && WindupRotationRate > 0.f)
	{
		RotateTowardsDesiredDirection(DeltaTime, WindupRotationRate);
	}
	if (bRunning)
	{
		OnTick(DeltaTime);
	}
}

void UOSFAction::UpdateProceduralTimeline()
{
	const FOSFActionTiming& T = GetEffectiveTiming();
	const float ActiveStart = T.Windup;
	const float ActiveEnd = T.Windup + T.Active;
	const float CancelStart = ActiveEnd + T.Recovery * T.CancelWindowStart;
	const float Total = T.GetTotal();

	if (Elapsed < ActiveStart)
	{
		SetPhase(EOSFActionPhase::Windup);
	}
	else if (Elapsed < ActiveEnd)
	{
		SetPhase(EOSFActionPhase::Active);
	}
	else
	{
		// A long frame (hitch, low frame rate, very short active window) must not skip the active window:
		// open it for this frame so traces / shots still happen, then move on to the recovery.
		if (Phase == EOSFActionPhase::Windup || Phase == EOSFActionPhase::None)
		{
			SetPhase(EOSFActionPhase::Active);
		}
		if (!bRunning)
		{
			return;
		}
		SetPhase(EOSFActionPhase::Recovery);
		if (Elapsed >= CancelStart && !IsWindowOpen(EOSFActionWindow::Cancel))
		{
			SetWindow(EOSFActionWindow::Cancel, true);
		}
	}

	if (!bIsHoldAction && Elapsed >= Total)
	{
		EndAction();
	}
}

void UOSFAction::SetPhase(EOSFActionPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	const EOSFActionPhase OldPhase = Phase;
	Phase = NewPhase;

	if (!bMontageMode)
	{
		// Procedural mode: the active phase maps to the action's main window.
		if (OldPhase == EOSFActionPhase::Active)
		{
			SetWindow(EOSFActionWindow::Active, false);
		}
		if (NewPhase == EOSFActionPhase::Active)
		{
			SetWindow(EOSFActionWindow::Active, true);
		}
	}
	OnPhaseChanged(NewPhase);
}

void UOSFAction::SetWindow(EOSFActionWindow Window, bool bOpen)
{
	if (!bRunning || IsWindowOpen(Window) == bOpen)
	{
		return;
	}
	if (bOpen)
	{
		OpenWindows.Add(Window);
	}
	else
	{
		OpenWindows.Remove(Window);
	}

	switch (Window)
	{
	case EOSFActionWindow::Invincible:
		bOpen ? ActionComponent->AddStateTag(OSFTags::State_Invincible) : ActionComponent->RemoveStateTag(OSFTags::State_Invincible);
		break;
	case EOSFActionWindow::HyperArmor:
		bOpen ? ActionComponent->AddStateTag(OSFTags::State_HyperArmor) : ActionComponent->RemoveStateTag(OSFTags::State_HyperArmor);
		break;
	case EOSFActionWindow::Parry:
		bOpen ? ActionComponent->AddStateTag(OSFTags::State_Parrying) : ActionComponent->RemoveStateTag(OSFTags::State_Parrying);
		break;
	case EOSFActionWindow::Cancel:
		bOpen ? ActionComponent->AddStateTag(OSFTags::State_ComboWindow) : ActionComponent->RemoveStateTag(OSFTags::State_ComboWindow);
		break;
	default:
		break;
	}

	// Montage mode: the montage's damage window drives the phase too.
	if (bMontageMode && Window == EOSFActionWindow::Active)
	{
		SetPhase(bOpen ? EOSFActionPhase::Active : EOSFActionPhase::Recovery);
	}

	OnWindowChanged(Window, bOpen);
	K2_OnWindowChanged(Window, bOpen);
}

void UOSFAction::Finish(bool bInterrupted)
{
	if (!bRunning)
	{
		return;
	}
	bEnding = true;

	TArray<EOSFActionWindow> Windows = OpenWindows.Array();
	for (EOSFActionWindow Window : Windows)
	{
		SetWindow(Window, false);
	}

	if (bMontageMode && PlayingMontage)
	{
		if (ACharacter* Character = GetCharacter())
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
			{
				if (AnimInstance->Montage_IsPlaying(PlayingMontage))
				{
					AnimInstance->Montage_Stop(0.15f, PlayingMontage);
				}
			}
		}
	}

	if (bClipMode)
	{
		ACharacter* Character = GetCharacter();
		if (UOSFClipAnimInstance* ClipAnim = Character && Character->GetMesh() ? Cast<UOSFClipAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr)
		{
			ClipAnim->StopActionClip(bInterrupted ? 0.12f : -1.f);
		}
		bClipMode = false;
	}

	ActionComponent->RemoveStateTags(GrantedTags);
	OnEnded(bInterrupted);
	K2_OnEnded(bInterrupted);

	bRunning = false;
	bEnding = false;
	Phase = EOSFActionPhase::None;
	PlayingMontage = nullptr;
}

void UOSFAction::EndAction()
{
	if (bRunning && !bEnding && ActionComponent)
	{
		ActionComponent->HandleActionFinished(this, false);
	}
}

void UOSFAction::Release()
{
	if (bIsHoldAction)
	{
		EndAction();
	}
}

void UOSFAction::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (bRunning && !bEnding && InMontage == PlayingMontage)
	{
		if (bIsHoldAction && !bInterrupted)
		{
			return;
		}
		ActionComponent->HandleActionFinished(this, bInterrupted);
	}
}

bool UOSFAction::CanBeCancelledBy(const UOSFAction* Other) const
{
	if (!Other)
	{
		return false;
	}
	if (CancelledByActions.HasTag(Other->ActionTag))
	{
		return true;
	}
	return bCancellableInCancelWindow && IsWindowOpen(EOSFActionWindow::Cancel);
}

void UOSFAction::RotateTowardsDesiredDirection(float DeltaTime, float RateDegPerSec)
{
	ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	FVector Desired = FVector::ZeroVector;
	if (const AOSFCharacterBase* OSFCharacter = Cast<AOSFCharacterBase>(Character))
	{
		Desired = OSFCharacter->GetDesiredFacingDirection();
	}
	else
	{
		Desired = Character->GetLastMovementInputVector();
	}
	Desired.Z = 0.f;
	if (Desired.IsNearlyZero())
	{
		return;
	}
	const FRotator Current = Character->GetActorRotation();
	const FRotator Target = Desired.Rotation();
	const FRotator NewRotation = FMath::RInterpConstantTo(Current, FRotator(0.f, Target.Yaw, 0.f), DeltaTime, RateDegPerSec);
	Character->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
}
