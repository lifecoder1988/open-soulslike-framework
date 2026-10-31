// Copyright Open Soulslike Framework contributors. MIT License.

#include "Items/OSFItemRegistry.h"
#include "Items/OSFItemData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"

UOSFItemRegistry* UOSFItemRegistry::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UOSFItemRegistry>() : nullptr;
}

void UOSFItemRegistry::RegisterItem(UOSFItemData* Item)
{
	if (Item)
	{
		Items.Add(Item->ItemId.IsNone() ? Item->GetFName() : Item->ItemId, Item);
	}
}

UOSFItemData* UOSFItemRegistry::FindItem(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	if (TObjectPtr<UOSFItemData>* Found = Items.Find(ItemId))
	{
		if (*Found)
		{
			return *Found;
		}
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UOSFItemData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);
	for (const FAssetData& Asset : Assets)
	{
		FName TagValue;
		const bool bHasTag = Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(UOSFItemData, ItemId), TagValue);
		if ((bHasTag && TagValue == ItemId) || Asset.AssetName == ItemId)
		{
			if (UOSFItemData* Item = Cast<UOSFItemData>(Asset.GetAsset()))
			{
				RegisterItem(Item);
				return Item;
			}
		}
	}
	return nullptr;
}

UOSFItemData* UOSFItemRegistry::Find(FName ItemId)
{
	UOSFItemRegistry* Registry = Get();
	return Registry ? Registry->FindItem(ItemId) : nullptr;
}
