// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "OSFTypes.h"
#include "OSFQuestComponent.generated.h"

class UOSFProgressComponent;
class UOSFInventoryComponent;

UENUM(BlueprintType)
enum class EOSFQuestObjectiveType : uint8
{
	Kill,		// Key = enemy kill key (AOSFEnemyCharacter::KillKey), Count kills after the quest started
	Flag,		// Key = progress flag (e.g. "Boss.Warden", "NPC.Smith.Met")
	Progress,	// ProgressTag must be owned
	Item		// Item x Count in the inventory
};

USTRUCT(BlueprintType)
struct OPENSOULSLIKEFRAMEWORK_API FOSFQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EOSFQuestObjectiveType Type = EOSFQuestObjectiveType::Flag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTag ProgressTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TObjectPtr<UOSFItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = 1))
	int32 Count = 1;
};

/** Quest definition: ordered objectives, rewards and the flags that start / record it. */
UCLASS(BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFQuestData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FOSFQuestObjective> Objectives;

	/** Objectives must be completed in order (only the current one is tracked / shown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bSequential = true;

	/** Starts automatically once the player owns this flag (None = only StartQuest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName StartFlag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText CompletionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FOSFItemStack> RewardItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 RewardCurrency = 0;

	/** Extra flags granted on completion (a "Quest.<Id>.Done" flag is always granted). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FName> CompletionFlags;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSFOnQuestEvent, UOSFQuestComponent*, Quests, UOSFQuestData*, Quest);

/**
 * Tracks quests through the progress component (flags "Quest.<Id>.Active/Done", kill counters "Kill.<Key>"),
 * so quest state is saved and loaded with the rest of the progression.
 */
UCLASS(ClassGroup = (OSF), meta = (BlueprintSpawnableComponent), BlueprintType)
class OPENSOULSLIKEFRAMEWORK_API UOSFQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSFQuestComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Quests this character knows about (started automatically by StartFlag or via StartQuest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSF|Quest")
	TArray<TObjectPtr<UOSFQuestData>> Quests;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Quest")
	FOSFOnQuestEvent OnQuestStarted;

	/** An objective was completed (the quest itself may still be running). */
	UPROPERTY(BlueprintAssignable, Category = "OSF|Quest")
	FOSFOnQuestEvent OnQuestUpdated;

	UPROPERTY(BlueprintAssignable, Category = "OSF|Quest")
	FOSFOnQuestEvent OnQuestCompleted;

	UFUNCTION(BlueprintCallable, Category = "OSF|Quest")
	bool StartQuest(UOSFQuestData* Quest);

	UFUNCTION(BlueprintPure, Category = "OSF|Quest")
	bool IsQuestActive(const UOSFQuestData* Quest) const;

	UFUNCTION(BlueprintPure, Category = "OSF|Quest")
	bool IsQuestComplete(const UOSFQuestData* Quest) const;

	/** Current progress (0..Count) of an objective. */
	UFUNCTION(BlueprintPure, Category = "OSF|Quest")
	int32 GetObjectiveProgress(const UOSFQuestData* Quest, int32 ObjectiveIndex) const;

	/** First unfinished objective, INDEX_NONE when all are done. */
	UFUNCTION(BlueprintPure, Category = "OSF|Quest")
	int32 GetCurrentObjective(const UOSFQuestData* Quest) const;

	/** Active quests, in declaration order. */
	UFUNCTION(BlueprintPure, Category = "OSF|Quest")
	TArray<UOSFQuestData*> GetActiveQuests() const;

	/** Records a kill for Kill objectives (called by AOSFEnemyCharacter on death). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Quest")
	void NotifyKill(FName KillKey);

	/** Re-checks every quest now (it also runs a few times per second). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Quest")
	void Evaluate();

	static FName KillCounter(FName KillKey);

protected:
	UOSFProgressComponent* GetProgress() const;
	UOSFInventoryComponent* GetInventory() const;
	static FName QuestFlag(const UOSFQuestData* Quest, const TCHAR* Suffix);
	FName BaselineKey(const UOSFQuestData* Quest, int32 ObjectiveIndex) const;
	void CompleteQuest(UOSFQuestData* Quest);

	TMap<TObjectPtr<UOSFQuestData>, int32> LastObjective;
	float EvaluateAccumulator = 0.f;
};
