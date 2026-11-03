// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OSFSoundBuses.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;

UENUM(BlueprintType)
enum class EOSFSoundBus : uint8
{
	Master,
	SFX,
	Music,
	Voice,
	UI
};

/**
 * Sound buses without content: five sound classes (SFX / Music / Voice / UI under Master) made at startup, so a game
 * can set levels per bus and duck one bus under another (music and effects under dialogue, everything under a hit stop)
 * without shipping sound class or sound mix assets.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFSoundBuses : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UOSFSoundBuses* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "OSF|Audio")
	USoundClass* GetBus(EOSFSoundBus Bus) const;

	/** Routes a sound through a bus (sets its sound class). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Audio")
	void Assign(USoundBase* Sound, EOSFSoundBus Bus) const;

	/** Base level of a bus (settings menu). */
	UFUNCTION(BlueprintCallable, Category = "OSF|Audio")
	void SetBusVolume(EOSFSoundBus Bus, float Volume);

	/**
	 * Lowers a bus to Volume for Seconds (fades in and out), e.g. music to 0.3 under a voice line. Seconds <= 0 holds
	 * until EndDuck. A new duck on the same bus replaces the old one.
	 */
	UFUNCTION(BlueprintCallable, Category = "OSF|Audio")
	void Duck(EOSFSoundBus Bus, float Volume, float Seconds, float FadeIn = 0.15f, float FadeOut = 0.6f);

	UFUNCTION(BlueprintCallable, Category = "OSF|Audio")
	void EndDuck(EOSFSoundBus Bus);

	/** Makes every audio device know the buses (classes made with NewObject are not registered by PostLoad). */
	void RegisterWithAudioDevices();

private:
	UPROPERTY(Transient)
	TMap<EOSFSoundBus, TObjectPtr<USoundClass>> Buses;

	UPROPERTY(Transient)
	TMap<EOSFSoundBus, TObjectPtr<USoundMix>> Ducks;

	USoundClass* MakeBus(EOSFSoundBus Bus, const TCHAR* Name, USoundClass* Parent);
};
