// Copyright Open Soulslike Framework contributors. MIT License.

#include "Characters/OSFPlayerCharacter.h"
#include "Characters/OSFInputConfig.h"
#include "Actions/OSFActionComponent.h"
#include "Combat/OSFCombatComponent.h"
#include "Combat/OSFLockOnComponent.h"
#include "Interaction/OSFInteractionComponent.h"
#include "Interaction/OSFWorldInteractables.h"
#include "Interaction/OSFNPC.h"
#include "Items/OSFEquipmentComponent.h"
#include "Items/OSFInventoryComponent.h"
#include "Items/OSFItemData.h"
#include "Progression/OSFProgressComponent.h"
#include "Progression/OSFQuestComponent.h"
#include "Stats/OSFStatComponent.h"
#include "Stats/OSFStatusEffectComponent.h"
#include "Game/OSFGameMode.h"
#include "Game/OSFPlayerController.h"
#include "Save/OSFSaveSubsystem.h"
#include "AI/OSFEnemyCharacter.h"
#include "OSFGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "OSFPlayer"

AOSFPlayerCharacter::AOSFPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 430.f;
	CameraBoom->SocketOffset = FVector(0.f, 35.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 14.f;
	CameraBoom->ProbeSize = 14.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(80.f);

	LockOn = CreateDefaultSubobject<UOSFLockOnComponent>(TEXT("LockOn"));
	Interaction = CreateDefaultSubobject<UOSFInteractionComponent>(TEXT("Interaction"));
	Progress = CreateDefaultSubobject<UOSFProgressComponent>(TEXT("Progress"));
	Quests = CreateDefaultSubobject<UOSFQuestComponent>(TEXT("Quests"));

	Combat->TeamId = 0;
	Combat->bPoiseBreakOpensRiposte = false;
	BodyColor = FLinearColor(0.55f, 0.55f, 0.6f);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void AOSFPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	RespawnTransform = GetActorTransform();
	Inventory->OnItemAdded.AddDynamic(this, &AOSFPlayerCharacter::HandleItemAdded);
	LockOn->OnLockOnChanged.AddDynamic(this, &AOSFPlayerCharacter::HandleLockOnChanged);
	AddMappingContext();
}

AOSFPlayerController* AOSFPlayerCharacter::GetOSFController() const
{
	return Cast<AOSFPlayerController>(GetController());
}

AActor* AOSFPlayerCharacter::GetFocusTarget() const
{
	return LockOn ? LockOn->GetTarget() : nullptr;
}

bool AOSFPlayerCharacter::IsInMenu() const
{
	const AOSFPlayerController* PC = GetOSFController();
	return PC && PC->IsInMenu();
}

void AOSFPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	AddMappingContext();
}

void AOSFPlayerCharacter::AddMappingContext()
{
	if (!ActiveInputConfig)
	{
		ActiveInputConfig = InputConfig ? InputConfig.Get() : UOSFInputConfig::BuildDefault(this);
	}
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !ActiveInputConfig->MappingContext)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (!Subsystem->HasMappingContext(ActiveInputConfig->MappingContext))
		{
			Subsystem->AddMappingContext(ActiveInputConfig->MappingContext, 0);
		}
	}
}

void AOSFPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!ActiveInputConfig)
	{
		ActiveInputConfig = InputConfig ? InputConfig.Get() : UOSFInputConfig::BuildDefault(this);
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	const UOSFInputConfig* C = ActiveInputConfig;
	Input->BindAction(C->Move, ETriggerEvent::Triggered, this, &AOSFPlayerCharacter::InputMove);
	Input->BindAction(C->Move, ETriggerEvent::Completed, this, &AOSFPlayerCharacter::InputMoveCompleted);
	Input->BindAction(C->Look, ETriggerEvent::Triggered, this, &AOSFPlayerCharacter::InputLook);
	Input->BindAction(C->Jump, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputJump);
	Input->BindAction(C->DodgeSprint, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputDodgePressed);
	Input->BindAction(C->DodgeSprint, ETriggerEvent::Completed, this, &AOSFPlayerCharacter::InputDodgeReleased);
	Input->BindAction(C->LightAttack, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputLight);
	Input->BindAction(C->HeavyAttack, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputHeavy);
	Input->BindAction(C->Guard, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputGuardPressed);
	Input->BindAction(C->Guard, ETriggerEvent::Completed, this, &AOSFPlayerCharacter::InputGuardReleased);
	Input->BindAction(C->WeaponSkill, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputSkill);
	Input->BindAction(C->LockOn, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputLockOn);
	Input->BindAction(C->UseItem, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputUseItem);
	Input->BindAction(C->Interact, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputInteract);
	Input->BindAction(C->CycleRight, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputCycleRight);
	Input->BindAction(C->CycleLeft, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputCycleLeft);
	Input->BindAction(C->CycleQuick, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputCycleQuick);
	Input->BindAction(C->TwoHand, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputTwoHand);
	if (C->Fire)
	{
		Input->BindAction(C->Fire, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputFire);
		Input->BindAction(C->Fire, ETriggerEvent::Completed, this, &AOSFPlayerCharacter::InputFireReleased);
	}
	if (C->Reload) { Input->BindAction(C->Reload, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputReload); }
	if (C->CycleAmmo) { Input->BindAction(C->CycleAmmo, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputCycleAmmo); }
	Input->BindAction(C->Menu, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenu);
	Input->BindAction(C->MenuUp, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuUp);
	Input->BindAction(C->MenuDown, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuDown);
	Input->BindAction(C->MenuLeft, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuLeft);
	Input->BindAction(C->MenuRight, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuRight);
	Input->BindAction(C->MenuConfirm, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuConfirm);
	Input->BindAction(C->MenuBack, ETriggerEvent::Started, this, &AOSFPlayerCharacter::InputMenuBack);
}

// --- Input --------------------------------------------------------------------------

void AOSFPlayerCharacter::InputMove(const FInputActionValue& Value)
{
	if (IsInMenu())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	if (GetCurrentLadder())
	{
		SetLadderInput(Axis.Y);
		return;
	}
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const FVector Direction = Forward * Axis.Y + Right * Axis.X;
	SetDesiredMoveDirection(Direction.GetClampedToMaxSize(1.f));
	const float Scale = GetActionMovementScale();
	if (Scale > 0.f)
	{
		AddMovementInput(Direction, Scale);
	}
}

void AOSFPlayerCharacter::InputMoveCompleted(const FInputActionValue& Value)
{
	SetDesiredMoveDirection(FVector::ZeroVector);
	SetLadderInput(0.f);
}

void AOSFPlayerCharacter::InputLook(const FInputActionValue& Value)
{
	if (IsInMenu())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>() * LookSensitivity;
	if (LockOn->IsLockedOn())
	{
		// Flick the stick / mouse to switch targets.
		const double Now = GetWorld()->GetTimeSeconds();
		if (FMath::Abs(Axis.X) > 6.f && Now - LastTargetSwitchTime > 0.4)
		{
			LockOn->SwitchTarget(Axis.X);
			LastTargetSwitchTime = Now;
		}
		return;
	}
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AOSFPlayerCharacter::InputJump()
{
	if (!IsInMenu() && !ActionComponent->IsBusy() && GetActionMovementScale() > 0.f)
	{
		Jump();
	}
}

void AOSFPlayerCharacter::InputDodgePressed()
{
	if (IsInMenu())
	{
		return;
	}
	if (GetCurrentLadder())
	{
		ExitLadder(false);
		return;
	}
	DodgePressTime = GetWorld()->GetTimeSeconds();
	bDodgeHeld = true;
}

void AOSFPlayerCharacter::InputDodgeReleased()
{
	const bool bWasSprinting = IsSprinting();
	SetSprinting(false);
	const double Held = GetWorld()->GetTimeSeconds() - DodgePressTime;
	bDodgeHeld = false;
	if (!IsInMenu() && !bWasSprinting && DodgePressTime >= 0.0 && Held < SprintHoldThreshold)
	{
		ActionComponent->TryExecuteAction(OSFTags::Action_Dodge);
	}
	DodgePressTime = -1.0;
}

void AOSFPlayerCharacter::InputLight()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_LightAttack); }
}

void AOSFPlayerCharacter::InputHeavy()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_HeavyAttack); }
}

void AOSFPlayerCharacter::InputGuardPressed()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_Guard); }
}

void AOSFPlayerCharacter::InputGuardReleased()
{
	ActionComponent->StopAction(OSFTags::Action_Guard);
}

void AOSFPlayerCharacter::InputSkill()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_WeaponSkill); }
}

void AOSFPlayerCharacter::InputLockOn()
{
	if (!IsInMenu()) { LockOn->ToggleLockOn(); }
}

void AOSFPlayerCharacter::InputUseItem()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_UseItem); }
}

void AOSFPlayerCharacter::InputInteract()
{
	if (!IsInMenu()) { Interaction->TryInteract(); }
}

void AOSFPlayerCharacter::InputCycleRight()
{
	if (!IsInMenu() && !ActionComponent->IsBusy()) { Equipment->CycleWeapon(true); }
}

void AOSFPlayerCharacter::InputCycleLeft()
{
	if (!IsInMenu() && !ActionComponent->IsBusy()) { Equipment->CycleWeapon(false); }
}

void AOSFPlayerCharacter::InputCycleQuick()
{
	if (!IsInMenu()) { Equipment->CycleQuickItem(); }
}

void AOSFPlayerCharacter::InputTwoHand()
{
	if (!IsInMenu() && !ActionComponent->IsBusy()) { Equipment->ToggleTwoHand(); }
}

void AOSFPlayerCharacter::InputFire()
{
	bFireHeld = true;
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_Shoot); }
}

void AOSFPlayerCharacter::InputFireReleased()
{
	bFireHeld = false;
}

void AOSFPlayerCharacter::InputReload()
{
	if (!IsInMenu()) { ActionComponent->TryExecuteAction(OSFTags::Action_Reload); }
}

void AOSFPlayerCharacter::InputCycleAmmo()
{
	if (IsInMenu() || ActionComponent->IsBusy())
	{
		return;
	}
	if (UOSFWeaponData* Ranged = Equipment->GetActiveRangedWeapon())
	{
		if (Equipment->CycleAmmo(Ranged))
		{
			ActionComponent->TryExecuteAction(OSFTags::Action_Reload);
		}
	}
}

void AOSFPlayerCharacter::InputMenu()
{
	if (AOSFPlayerController* PC = GetOSFController())
	{
		PC->ToggleSystemMenu();
	}
}

void AOSFPlayerCharacter::InputMenuUp() { if (IsInMenu()) { GetOSFController()->MenuNavigate(0, -1); } }
void AOSFPlayerCharacter::InputMenuDown() { if (IsInMenu()) { GetOSFController()->MenuNavigate(0, 1); } }
void AOSFPlayerCharacter::InputMenuLeft() { if (IsInMenu()) { GetOSFController()->MenuNavigate(-1, 0); } }
void AOSFPlayerCharacter::InputMenuRight() { if (IsInMenu()) { GetOSFController()->MenuNavigate(1, 0); } }
void AOSFPlayerCharacter::InputMenuConfirm() { if (IsInMenu()) { GetOSFController()->MenuConfirm(); } }
void AOSFPlayerCharacter::InputMenuBack() { if (IsInMenu()) { GetOSFController()->MenuBack(); } }

bool AOSFPlayerCharacter::PerformAction(FGameplayTag ActionTag)
{
	return ActionComponent->TryExecuteAction(ActionTag);
}

// --- Tick ---------------------------------------------------------------------------

void AOSFPlayerCharacter::AddCameraShake(float Amplitude, float Duration)
{
	if (Amplitude <= 0.f || Duration <= 0.f)
	{
		return;
	}
	if (ShakeTime <= 0.f)
	{
		CameraBaseLocation = FollowCamera->GetRelativeLocation();
	}
	ShakeAmplitude = FMath::Max(ShakeAmplitude, Amplitude);
	ShakeTime = FMath::Max(ShakeTime, Duration);
}

void AOSFPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ShakeTime > 0.f)
	{
		// Real time: the shake keeps going through the hit stop.
		ShakeTime -= GetWorld()->GetDeltaSeconds();
		const float Strength = ShakeAmplitude * FMath::Clamp(ShakeTime * 4.f, 0.f, 1.f);
		FollowCamera->SetRelativeLocation(CameraBaseLocation + FMath::VRand() * Strength);
		if (ShakeTime <= 0.f)
		{
			ShakeAmplitude = 0.f;
			FollowCamera->SetRelativeLocation(CameraBaseLocation);
		}
	}

	if (bDodgeHeld && !IsSprinting() && DodgePressTime >= 0.0 && GetWorld()->GetTimeSeconds() - DodgePressTime >= SprintHoldThreshold && !ActionComponent->IsBusy())
	{
		SetSprinting(true);
	}

	// Holding fire keeps shooting at the weapon's fire rate.
	if (bFireHeld && !IsInMenu() && !ActionComponent->IsBusy())
	{
		ActionComponent->TryExecuteAction(OSFTags::Action_Shoot, false);
	}

	// Strafe around the lock-on target (except while sprinting / rolling).
	const AActor* Target = LockOn->GetTarget();
	const bool bStrafe = Target && !IsSprinting() && !HasStateTag(OSFTags::State_Dodging) && !IsDead() && !GetCurrentLadder();
	GetCharacterMovement()->bOrientRotationToMovement = !bStrafe;
	if (bStrafe && GetActionMovementScale() > 0.f && !ActionComponent->IsBusy())
	{
		const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const FRotator Desired(0.f, To.Rotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, 12.f));
	}
}

// --- Flow -----------------------------------------------------------------------------

void AOSFPlayerCharacter::RestAt(AOSFRestingPoint* RestingPoint)
{
	if (!RestingPoint || IsDead())
	{
		return;
	}
	LockOn->ClearLockOn();
	ActionComponent->CancelCurrentAction();
	ActionComponent->AddStateTag(OSFTags::State_Resting);
	Stats->RestoreAll();
	StatusEffects->CureAll();
	Inventory->RefillFlasks();
	SetRespawnPoint(RestingPoint->GetRespawnTransform(), RestingPoint->GetSaveId());

	if (AOSFGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSFGameMode>())
	{
		GameMode->ResetWorld(Progress);
	}
	if (UOSFSaveSubsystem* Save = UOSFSaveSubsystem::Get(this))
	{
		Save->AutoSave(this);
	}
	if (AOSFPlayerController* PC = GetOSFController())
	{
		PC->OpenRestMenu(RestingPoint);
	}
}

void AOSFPlayerCharacter::StopResting()
{
	ActionComponent->ClearStateTag(OSFTags::State_Resting);
}

void AOSFPlayerCharacter::BeginDialogue(AOSFNPC* NPC)
{
	if (AOSFPlayerController* PC = GetOSFController())
	{
		LockOn->ClearLockOn();
		PC->OpenDialogue(NPC);
	}
}

void AOSFPlayerCharacter::Notify(const FText& Text, EOSFNotifyType Type)
{
	if (AOSFPlayerController* PC = GetOSFController())
	{
		PC->PushNotification(Text, Type);
	}
}

void AOSFPlayerCharacter::SetActiveBoss(AOSFEnemyCharacter* Boss)
{
	ActiveBoss = Boss;
}

void AOSFPlayerCharacter::SetRespawnPoint(const FTransform& Transform, FName RestingPointId)
{
	RespawnTransform = Transform;
	LastRestingPointId = RestingPointId;
}

void AOSFPlayerCharacter::HandleItemAdded(UOSFInventoryComponent* InInventory, UOSFItemData* Item, int32 Count)
{
	if (Item)
	{
		Notify(Count > 1 ? FText::Format(LOCTEXT("ItemCount", "{0} x{1}"), Item->DisplayName, FText::AsNumber(Count)) : Item->DisplayName, EOSFNotifyType::Toast);
	}
}

void AOSFPlayerCharacter::HandleLockOnChanged(AActor* NewTarget)
{
}

void AOSFPlayerCharacter::OnBloodstainRecovered(AOSFBloodstain* Bloodstain)
{
	if (AOSFGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSFGameMode>())
	{
		GameMode->ClearBloodstain();
	}
}

void AOSFPlayerCharacter::HandleDeath(UOSFCombatComponent* InCombat, AActor* Killer)
{
	Super::HandleDeath(InCombat, Killer);
	LockOn->ClearLockOn();
	if (ActiveBoss.IsValid())
	{
		ActiveBoss.Reset();
	}
	Notify(LOCTEXT("YouDied", "YOU DIED"), EOSFNotifyType::Banner);
	if (AOSFGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSFGameMode>())
	{
		GameMode->HandlePlayerDeath(this);
	}
}

#undef LOCTEXT_NAMESPACE
