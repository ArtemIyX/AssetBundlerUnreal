// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetBundlerActions.h"

#include "AssetBundler.h"
#include "SAssetBundlerWindow.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FAssetBundlerActions"

namespace
{
	TWeakPtr<SAssetBundlerWindow> WindowWidget;
}

void FAssetBundlerActions::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			FAssetBundlerModule::AssetBundlerTabName,
			FOnSpawnTab::CreateStatic(&FAssetBundlerActions::SpawnTab))
		.SetDisplayName(LOCTEXT("AssetBundlerTabTitle", "Asset Bundler"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FAssetBundlerActions::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FAssetBundlerModule::AssetBundlerTabName);
}

void FAssetBundlerActions::OpenBundlerWindow(const TArray<FAssetData>& InSelectedAssets)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FAssetBundlerModule::AssetBundlerTabName);
	SetAssetsToAssetBundlerWindow(WindowWidget, InSelectedAssets);
}

TSharedRef<SDockTab> FAssetBundlerActions::SpawnTab(const FSpawnTabArgs& InArgs)
{
	TSharedRef<SDockTab> tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);
	tab->SetContent(CreateAssetBundlerWindow(tab, &WindowWidget));
	return tab;
}

#undef LOCTEXT_NAMESPACE
