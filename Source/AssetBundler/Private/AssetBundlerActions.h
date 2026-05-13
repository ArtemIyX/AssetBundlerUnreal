// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class SDockTab;
class FSpawnTabArgs;

class FAssetBundlerActions
{
public:
	static void RegisterTabSpawner();
	static void UnregisterTabSpawner();
	static void OpenBundlerWindow(const TArray<FAssetData>& InSelectedAssets);

private:
	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& InArgs);
};
