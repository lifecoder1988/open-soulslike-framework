// Copyright Open Soulslike Framework contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSFLocalization.generated.h"

/**
 * Runtime translations for the framework's own UI text (menus, prompts, notifications) without a localization
 * dashboard pass: a JSON table is registered as polyglot text data, keyed by the LOCTEXT namespace / key.
 * Generate the template with Tools/Localization/extract_osf_text.py.
 */
UCLASS()
class OPENSOULSLIKEFRAMEWORK_API UOSFLocalization : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Loads {"<namespace>": {"<key>": {"source": "...", "text": "..."}}} (or {"<key>": "text"} per namespace) and
	 * registers every non-empty "text" for Culture. With bSetCulture the current culture switches to Culture.
	 * Returns the number of registered strings.
	 */
	UFUNCTION(BlueprintCallable, Category = "OSF|Localization")
	static int32 LoadTranslationTable(const FString& JsonPath, const FString& Culture = TEXT("zh-Hans"), bool bSetCulture = true);
};
