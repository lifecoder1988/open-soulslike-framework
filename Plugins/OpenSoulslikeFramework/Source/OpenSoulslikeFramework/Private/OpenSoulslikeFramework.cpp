// Copyright Open Soulslike Framework contributors. MIT License.

#include "OpenSoulslikeFramework.h"

DEFINE_LOG_CATEGORY(LogOSF);

void FOpenSoulslikeFrameworkModule::StartupModule()
{
	UE_LOG(LogOSF, Log, TEXT("Open Soulslike Framework loaded."));
}

void FOpenSoulslikeFrameworkModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FOpenSoulslikeFrameworkModule, OpenSoulslikeFramework)
