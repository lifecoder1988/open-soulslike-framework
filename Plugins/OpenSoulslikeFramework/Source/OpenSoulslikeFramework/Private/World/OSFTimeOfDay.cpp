// Copyright Open Soulslike Framework contributors. MIT License.

#include "World/OSFTimeOfDay.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AOSFTimeOfDay::AOSFTimeOfDay()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AOSFTimeOfDay* AOSFTimeOfDay::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AOSFTimeOfDay> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AOSFTimeOfDay::BeginPlay()
{
	Super::BeginPlay();
	if (!Sun)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			Sun = *It;
			break;
		}
	}
	ApplySun();
}

void AOSFTimeOfDay::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DayLengthMinutes > 0.f)
	{
		TimeOfDay = FMath::Fmod(TimeOfDay + DeltaSeconds * 24.f / (DayLengthMinutes * 60.f), 24.f);
		ApplySun();
	}
}

void AOSFTimeOfDay::PassTime(float Hours)
{
	TimeOfDay = FMath::Fmod(TimeOfDay + Hours, 24.f);
	ApplySun();
}

void AOSFTimeOfDay::ApplySun()
{
	if (Sun)
	{
		// 6h = sunrise (pitch 0), 12h = noon (pitch -90), 18h = sunset, night below the horizon.
		const float Pitch = -(TimeOfDay - 6.f) / 12.f * 180.f;
		Sun->SetActorRotation(FRotator(Pitch, 35.f, 0.f));
		if (ULightComponent* Light = Sun->GetLightComponent())
		{
			const float Daylight = FMath::Clamp(FMath::Sin((TimeOfDay - 6.f) / 12.f * PI), 0.f, 1.f);
			Light->SetIntensity(FMath::Lerp(0.6f, 10.f, FMath::Sqrt(Daylight)));
		}
	}
	const int32 Hour = FMath::FloorToInt(TimeOfDay);
	if (Hour != LastHour)
	{
		LastHour = Hour;
		OnHourChanged.Broadcast(Hour);
	}
}
