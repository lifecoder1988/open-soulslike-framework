// Copyright Open Soulslike Framework contributors. MIT License.

#include "Game/OSFLocalization.h"
#include "OpenSoulslikeFramework.h"
#include "Dom/JsonObject.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/PolyglotTextData.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

int32 UOSFLocalization::LoadTranslationTable(const FString& JsonPath, const FString& Culture, bool bSetCulture)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *JsonPath))
	{
		UE_LOG(LogOSF, Warning, TEXT("Translation table not found: %s"), *JsonPath);
		return 0;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogOSF, Warning, TEXT("Translation table is not valid JSON: %s"), *JsonPath);
		return 0;
	}
	int32 Count = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Namespace : Root->Values)
	{
		const TSharedPtr<FJsonObject>* Keys = nullptr;
		if (!Namespace.Value->TryGetObject(Keys))
		{
			continue;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Keys)->Values)
		{
			FString Source, Text;
			const TSharedPtr<FJsonObject>* Fields = nullptr;
			if (Entry.Value->TryGetObject(Fields))
			{
				(*Fields)->TryGetStringField(TEXT("source"), Source);
				(*Fields)->TryGetStringField(TEXT("text"), Text);
			}
			else
			{
				Entry.Value->TryGetString(Text);
			}
			if (Text.IsEmpty())
			{
				continue;
			}
			FPolyglotTextData Data(ELocalizedTextSourceCategory::Game, Namespace.Key, Entry.Key, Source.IsEmpty() ? Text : Source, TEXT("en"));
			Data.AddLocalizedString(Culture, Text);
			FTextLocalizationManager::Get().RegisterPolyglotTextData(Data);
			++Count;
		}
	}
	if (bSetCulture)
	{
		FInternationalization::Get().SetCurrentCulture(Culture);
#if WITH_EDITOR
		// The editor shows game text in its native language unless game localization preview is on (PIE).
		if (GIsEditor)
		{
			FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);
		}
#endif
	}
	UE_LOG(LogOSF, Log, TEXT("Registered %d %s translations from %s"), Count, *Culture, *JsonPath);
	return Count;
}
