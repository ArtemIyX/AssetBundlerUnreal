// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetBundlerMenuExtensions.h"

#include "AssetBundlerActions.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FAssetBundlerMenuExtensions"

namespace
{
	const TCHAR* AssetBundlerOwner = TEXT("AssetBundler");

	bool IsSupportedAsset(const FAssetData& InAsset)
	{
		const FTopLevelAssetPath classPath = InAsset.AssetClassPath;
		return classPath == USkeletalMesh::StaticClass()->GetClassPathName()
			|| classPath == UStaticMesh::StaticClass()->GetClassPathName()
			|| classPath == UPhysicsAsset::StaticClass()->GetClassPathName();
	}

	bool HasOnlySupportedAssets(const TArray<FAssetData>& InSelectedAssets)
	{
		if (InSelectedAssets.IsEmpty())
		{
			return false;
		}

		for (const FAssetData& asset : InSelectedAssets)
		{
			if (!IsSupportedAsset(asset))
			{
				return false;
			}
		}

		return true;
	}

	void RegisterAssetMenu(const TCHAR* InMenuName)
	{
		if (UToolMenu* assetMenu = UToolMenus::Get()->ExtendMenu(InMenuName))
		{
			FToolMenuSection& section = assetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			section.AddDynamicEntry(TEXT("AssetBundler.BundleAssets"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* context = InSection.Context.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!context)
				{
					return;
				}

				const TArray<FAssetData> selectedAssets = context->SelectedAssets;
				if (!HasOnlySupportedAssets(selectedAssets))
				{
					return;
				}

				InSection.AddEntry(FToolMenuEntry::InitMenuEntry(
					TEXT("AssetBundler.BundleAssets"),
					LOCTEXT("BundleAssetsLabel", "Bundle Assets"),
					LOCTEXT("BundleAssetsTooltip", "Open Asset Bundler for the selected assets."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.FolderClosed")),
					FToolUIActionChoice(FExecuteAction::CreateLambda([selectedAssets]()
					{
						FAssetBundlerActions::OpenBundlerWindow(selectedAssets);
					}))));
			}));
		}
	}
}

void FAssetBundlerMenuExtensions::Register()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic([]()
		{
			FToolMenuOwnerScoped ownerScoped(AssetBundlerOwner);
			RegisterAssetMenu(TEXT("ContentBrowser.AssetContextMenu.SkeletalMesh"));
			RegisterAssetMenu(TEXT("ContentBrowser.AssetContextMenu.StaticMesh"));
			RegisterAssetMenu(TEXT("ContentBrowser.AssetContextMenu.PhysicsAsset"));
		}));
}

void FAssetBundlerMenuExtensions::Unregister()
{
	if (UToolMenus::TryGet())
	{
		UToolMenus::Get()->UnregisterOwnerByName(AssetBundlerOwner);
	}
}

#undef LOCTEXT_NAMESPACE
