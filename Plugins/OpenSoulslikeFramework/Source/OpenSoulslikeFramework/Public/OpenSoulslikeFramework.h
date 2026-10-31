// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

OPENSOULSLIKEFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogOSF, Log, All);

class FOpenSoulslikeFrameworkModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
