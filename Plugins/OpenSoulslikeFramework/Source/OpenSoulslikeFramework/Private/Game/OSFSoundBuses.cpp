// Copyright Open Soulslike Framework contributors. MIT License.

#include "Game/OSFSoundBuses.h"
#include "OpenSoulslikeFramework.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void UOSFSoundBuses::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	USoundClass* Master = MakeBus(EOSFSoundBus::Master, TEXT("OSF_Master"), nullptr);
	MakeBus(EOSFSoundBus::SFX, TEXT("OSF_SFX"), Master);
	MakeBus(EOSFSoundBus::Music, TEXT("OSF_Music"), Master);
	MakeBus(EOSFSoundBus::Voice, TEXT("OSF_Voice"), Master);
	MakeBus(EOSFSoundBus::UI, TEXT("OSF_UI"), Master);
}

USoundClass* UOSFSoundBuses::MakeBus(EOSFSoundBus Bus, const TCHAR* Name, USoundClass* Parent)
{
	USoundClass* Class = NewObject<USoundClass>(this, Name);
	Class->Properties.Volume = 1.f;
	Class->Properties.Pitch = 1.f;
	if (Parent)
	{
		Class->ParentClass = Parent;
		Parent->ChildClasses.Add(Class);
	}
	Buses.Add(Bus, Class);
	return Class;
}

UOSFSoundBuses* UOSFSoundBuses::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UOSFSoundBuses* Buses = Instance ? Instance->GetSubsystem<UOSFSoundBuses>() : nullptr;
	if (Buses)
	{
		Buses->RegisterWithAudioDevices();
	}
	return Buses;
}

void UOSFSoundBuses::RegisterWithAudioDevices()
{
	// A class the audio device has not seen plays with a warning per frame and no class properties.
	if (FAudioDeviceManager* Manager = FAudioDeviceManager::Get())
	{
		Manager->IterateOverAllDevices([this](Audio::FDeviceId, FAudioDevice* Device)
		{
			for (const TPair<EOSFSoundBus, TObjectPtr<USoundClass>>& Pair : Buses)
			{
				if (Pair.Value)
				{
					Device->RegisterSoundClass(Pair.Value);
				}
			}
		});
	}
}

USoundClass* UOSFSoundBuses::GetBus(EOSFSoundBus Bus) const
{
	const TObjectPtr<USoundClass>* Found = Buses.Find(Bus);
	return Found ? Found->Get() : nullptr;
}

void UOSFSoundBuses::Assign(USoundBase* Sound, EOSFSoundBus Bus) const
{
	if (Sound)
	{
		Sound->SoundClassObject = GetBus(Bus);
	}
}

void UOSFSoundBuses::SetBusVolume(EOSFSoundBus Bus, float Volume)
{
	if (USoundClass* Class = GetBus(Bus))
	{
		Class->Properties.Volume = FMath::Clamp(Volume, 0.f, 2.f);
	}
}

void UOSFSoundBuses::Duck(EOSFSoundBus Bus, float Volume, float Seconds, float FadeIn, float FadeOut)
{
	USoundClass* Class = GetBus(Bus);
	UWorld* World = GetWorld();
	if (!Class || !World)
	{
		return;
	}
	EndDuck(Bus);
	USoundMix* Mix = NewObject<USoundMix>(this);
	FSoundClassAdjuster Adjuster;
	Adjuster.SoundClassObject = Class;
	Adjuster.VolumeAdjuster = FMath::Clamp(Volume, 0.f, 1.f);
	Adjuster.bApplyToChildren = true;
	Mix->SoundClassEffects.Add(Adjuster);
	Mix->FadeInTime = FadeIn;
	Mix->FadeOutTime = FadeOut;
	Mix->Duration = Seconds > 0.f ? Seconds : -1.f;
	Ducks.Add(Bus, Mix);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

void UOSFSoundBuses::EndDuck(EOSFSoundBus Bus)
{
	if (TObjectPtr<USoundMix>* Mix = Ducks.Find(Bus))
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::PopSoundMixModifier(World, *Mix);
		}
		Ducks.Remove(Bus);
	}
}
