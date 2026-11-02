// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Characters/OSFPlayerCharacter.h"
#include "OSFTypes.h"
#include "OSFPlayerController.generated.h"

class AOSFRestingPoint;
class UAudioComponent;
class USoundBase;
class AOSFNPC;
class UOSFItemData;

UENUM(BlueprintType)
enum class EOSFMenu : uint8
{
	None,
	System,
	Rest,
	LevelUp,
	Inventory,
	Equipment,
	EquipSelect,
	Crafting,
	Storage,
	Vendor,
	Dialogue,
	SaveSlots,
	LoadSlots,
	Title		// start screen: New Game / Continue / Quit (bOpenTitleOnStart)
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFMenuEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	FText Value;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	TObjectPtr<UOSFItemData> Item = nullptr;
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFNotification
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	EOSFNotifyType Type = EOSFNotifyType::Toast;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	float Remaining = 3.f;

	UPROPERTY(BlueprintReadOnly, Category = "OSF|UI")
	float Duration = 3.f;
};

USTRUCT()
struct FOSFMenuFrame
{
	GENERATED_BODY()

	EOSFMenu Menu = EOSFMenu::None;
	int32 Selection = 0;
	int32 Tab = 0;
	EOSFEquipSlot Slot = EOSFEquipSlot::None;
	int32 SlotIndex = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnMenuChanged, EOSFMenu, NewMenu);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSFOnInputDeviceChanged, bool, bGamepad);

/**
 * Player controller: owns the UI state machine (menus, dialogue, notifications) independently from rendering.
 * The default AOSFHUD draws it with the canvas; a UMG front-end can use the same Blueprint API
 * (GetMenuEntries / MenuNavigate / MenuConfirm / MenuBack / OnMenuChanged).
 * Also hosts the "osf.*" style debug exec commands used by automation (MCP) and designers.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API AOSFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOSFPlayerController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	UPROPERTY(BlueprintAssignable, Category = "OSF|UI")
	FOSFOnMenuChanged OnMenuChanged;

	/** Opens the title menu (New Game / Continue / Quit) when play starts; the game menu then offers "Quit to Title". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|UI")
	bool bOpenTitleOnStart = false;

	/** Menus that pause the world while they are anywhere in the menu stack (the title always pauses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|UI")
	TSet<EOSFMenu> PausingMenus;

	/** Last meaningful input came from a gamepad: prompts and key hints should show pad glyphs. */
	UFUNCTION(BlueprintPure, Category = "OSF|Input")
	bool IsUsingGamepad() const { return bUsingGamepad; }

	UPROPERTY(BlueprintAssignable, Category = "OSF|Input")
	FOSFOnInputDeviceChanged OnInputDeviceChanged;

	/** Menu feedback sounds (2D). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|UI")
	TObjectPtr<USoundBase> MenuMoveSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|UI")
	TObjectPtr<USoundBase> MenuConfirmSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|UI")
	TObjectPtr<USoundBase> MenuBackSound;

	// --- Menus ---

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	bool IsInMenu() const { return MenuStack.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	EOSFMenu GetCurrentMenu() const { return MenuStack.Num() > 0 ? MenuStack.Last().Menu : EOSFMenu::None; }

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void OpenMenu(EOSFMenu Menu);

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void CloseAllMenus();

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void ToggleSystemMenu();

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void OpenRestMenu(AOSFRestingPoint* RestingPoint);

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void OpenDialogue(AOSFNPC* NPC);

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void MenuNavigate(int32 DeltaX, int32 DeltaY);

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void MenuConfirm();

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void MenuBack();

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	TArray<FOSFMenuEntry> GetMenuEntries() const;

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	FText GetMenuTitle() const;

	/** Extra text block (selected item details, dialogue line, level-up preview...). */
	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	FText GetMenuDetails() const;

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	int32 GetMenuSelection() const { return MenuStack.Num() > 0 ? MenuStack.Last().Selection : 0; }

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	FText GetMenuTabLabel() const;

	// --- Notifications ---

	UFUNCTION(BlueprintCallable, Category = "OSF|UI")
	void PushNotification(const FText& Text, EOSFNotifyType Type);

	UFUNCTION(BlueprintPure, Category = "OSF|UI")
	const TArray<FOSFNotification>& GetNotifications() const { return Notifications; }

	UFUNCTION(BlueprintPure, Category = "OSF")
	AOSFPlayerCharacter* GetOSFPlayer() const { return Cast<AOSFPlayerCharacter>(GetPawn()); }

	// --- Debug / automation commands (type in the console or call through MCP) ---

	UFUNCTION(Exec) void OSFGiveRunes(int32 Amount);
	UFUNCTION(Exec) void OSFGiveItem(const FString& ItemId, int32 Count = 1);
	UFUNCTION(Exec) void OSFGod();
	UFUNCTION(Exec) void OSFHeal();
	UFUNCTION(Exec) void OSFKillAll();
	UFUNCTION(Exec) void OSFRest();
	UFUNCTION(Exec) void OSFSave(int32 Slot);
	UFUNCTION(Exec) void OSFLoad(int32 Slot);
	UFUNCTION(Exec) void OSFAction(const FString& ActionTag);
	UFUNCTION(Exec) void OSFLockOn();
	/** Ranged weapons: switch to the next ammo type that has rounds, then reload. */
	UFUNCTION(Exec) void OSFCycleAmmo();
	/** Cycles the right-hand (1) or left-hand (0) weapon slot, like the cycle inputs. */
	UFUNCTION(Exec) void OSFCycleWeapon(int32 bRightHand = 1);
	/** Turns the player and camera towards the first actor whose name or class contains Filter. */
	UFUNCTION(Exec) void OSFFace(const FString& Filter);
	UFUNCTION(Exec) void OSFTeleport(float X, float Y, float Z);
	UFUNCTION(Exec) void OSFDebugTraces();
	UFUNCTION(Exec) void OSFStatus();
	UFUNCTION(Exec) void OSFMenu(const FString& Command);
	/** Interact with the focused interactable (resting point, NPC, chest, door, ladder, fog gate...). */
	UFUNCTION(Exec) void OSFInteract();
	/** Teleport next to the first actor whose name or class contains Filter (e.g. "FogGate", "NPC", "Chest"). */
	UFUNCTION(Exec) void OSFGoTo(const FString& Filter);
	/** Autopilot (UOSFAutoPilotComponent on the pawn): start | stop | status. */
	UFUNCTION(Exec) void OSFAutoPilot(const FString& Command);

	/** Text summary of the player state (also printed by OSFStatus). */
	UFUNCTION(BlueprintPure, Category = "OSF")
	FString GetStatusReport() const;

protected:
	UPROPERTY(Transient)
	TArray<FOSFMenuFrame> MenuStack;

	UPROPERTY(Transient)
	TArray<FOSFNotification> Notifications;

	TWeakObjectPtr<AOSFRestingPoint> CurrentRestingPoint;
	TWeakObjectPtr<AOSFNPC> DialogueNPC;
	int32 DialogueEntryIndex = INDEX_NONE;
	int32 DialogueLine = 0;
	bool bDialogueFinished = false;
	bool bUsingGamepad = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DialogueVoice;

	/** Plays the current dialogue line's voice (stops the previous one). */
	void PlayDialogueVoice();
	void PlayMenuSound(USoundBase* Sound);
	TMap<FGameplayTag, int32> PendingLevelUp;

	void OnMenusChanged();
	void ConfirmEntry(FOSFMenuFrame& Frame, int32 Index);
	int32 GetEntryCount() const;
	void GetInventoryList(TArray<UOSFItemData*>& Out, bool bStorage = false) const;
	void GetEquipCandidates(EOSFEquipSlot Slot, TArray<UOSFItemData*>& Out) const;
	static TArray<FGameplayTag> GetLevelAttributes();
	FText DescribeItem(const UOSFItemData* Item) const;
};
