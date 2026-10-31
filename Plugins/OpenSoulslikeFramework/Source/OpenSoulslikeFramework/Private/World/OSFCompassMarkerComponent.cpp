// Copyright Open Soulslike Framework contributors. MIT License.

#include "World/OSFCompassMarkerComponent.h"
#include "Engine/World.h"

TArray<TWeakObjectPtr<UOSFCompassMarkerComponent>> UOSFCompassMarkerComponent::Markers;

UOSFCompassMarkerComponent::UOSFCompassMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSFCompassMarkerComponent::OnRegister()
{
	Super::OnRegister();
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		Markers.AddUnique(this);
	}
}

void UOSFCompassMarkerComponent::OnUnregister()
{
	Markers.RemoveAll([this](const TWeakObjectPtr<UOSFCompassMarkerComponent>& Marker) { return !Marker.IsValid() || Marker.Get() == this; });
	Super::OnUnregister();
}

const TArray<TWeakObjectPtr<UOSFCompassMarkerComponent>>& UOSFCompassMarkerComponent::GetAllMarkers()
{
	Markers.RemoveAll([](const TWeakObjectPtr<UOSFCompassMarkerComponent>& Marker) { return !Marker.IsValid(); });
	return Markers;
}
