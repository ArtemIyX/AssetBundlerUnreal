// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetBundler.h"

#include "AssetBundlerActions.h"
#include "AssetBundlerMenuExtensions.h"

#define LOCTEXT_NAMESPACE "FAssetBundlerModule"

const FName FAssetBundlerModule::AssetBundlerTabName(TEXT("AssetBundler"));

void FAssetBundlerModule::StartupModule()
{
	FAssetBundlerActions::RegisterTabSpawner();
	FAssetBundlerMenuExtensions::Register();
}

void FAssetBundlerModule::ShutdownModule()
{
	FAssetBundlerMenuExtensions::Unregister();
	FAssetBundlerActions::UnregisterTabSpawner();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAssetBundlerModule, AssetBundler)
