// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Interaction/OSFInteractable.h"
#include "OSFTypes.h"
#include "OSFNPC.generated.h"

class USoundBase;

class UOSFItemData;
class UOSFProgressComponent;
class AOSFPlayerCharacter;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequenceBase;

/** One conversation. The first entry whose conditions pass is played. */
USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFDialogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FGameplayTagContainer RequiredProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FGameplayTagContainer BlockedByProgress;

	/** Name flags (see UOSFProgressComponent::AddFlag) - no tag declaration needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<FName> RequiredFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<FName> BlockedByFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (MultiLine = true))
	TArray<FText> Lines;

	/** Voice for each line (same index as Lines; missing entries stay silent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<TObjectPtr<USoundBase>> LineVoices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FGameplayTagContainer GrantProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<FName> GrantFlags;

	/** Items given to the player when the conversation ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<FOSFItemStack> GrantItems;

	/** Show the "Trade" option after this conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	bool bOfferTrade = true;
};

UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFDialogData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialog")
	TArray<FOSFDialogEntry> Entries;

	/** Returns the index of the first entry valid for this progress, INDEX_NONE if none. */
	int32 SelectEntry(const UOSFProgressComponent* Progress) const;
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFVendorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	TObjectPtr<UOSFItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	int32 Price = 100;

	/** -1 = infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	int32 Stock = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	FGameplayTagContainer RequiredProgress;
};

UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFVendorData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vendor")
	TArray<FOSFVendorEntry> Entries;

	/** Multiplier applied to an item's SellPrice when the player sells to this vendor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vendor")
	float SellMultiplier = 1.f;
};

/** Friendly NPC with progress driven dialogue and an optional shop. */
UCLASS(Blueprintable)
class OPENSOULSLIKEFRAMEWORK_API AOSFNPC : public AOSFInteractableBase
{
	GENERATED_BODY()

public:
	AOSFNPC();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FText NPCName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	TObjectPtr<UOSFDialogData> Dialog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	TObjectPtr<UOSFVendorData> Vendor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** Optional animated body (replaces the basic-shape body when a skeletal mesh is set). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC")
	TObjectPtr<USkeletalMeshComponent> SkeletalBody;

	/** Uses a skeletal mesh scaled to TargetHeight (cm), feet on the ground, looping IdleAnimation. */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetupSkeletalBody(USkeletalMesh* InMesh, float TargetHeight, float YawOffset = -90.f, UAnimSequenceBase* IdleAnimation = nullptr);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;

	/** Conversation for the current progress (null if nothing to say). */
	const FOSFDialogEntry* GetCurrentDialog(const UOSFProgressComponent* Progress) const;

	/** Applies the rewards / progress of a finished conversation. */
	void CompleteDialog(AOSFPlayerCharacter* Player, const FOSFDialogEntry& Entry);

	// --- Shop ---
	UFUNCTION(BlueprintPure, Category = "NPC")
	int32 GetRemainingStock(const UOSFProgressComponent* Progress, int32 EntryIndex) const;

	UFUNCTION(BlueprintCallable, Category = "NPC")
	bool Buy(AOSFPlayerCharacter* Player, int32 EntryIndex);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	bool Sell(AOSFPlayerCharacter* Player, UOSFItemData* Item);

	UFUNCTION(BlueprintPure, Category = "NPC")
	bool IsEntryAvailable(const UOSFProgressComponent* Progress, int32 EntryIndex) const;

protected:
	virtual FString GetFlagPrefix() const override { return TEXT("NPC"); }
	FName GetStockKey(int32 EntryIndex) const;
};
