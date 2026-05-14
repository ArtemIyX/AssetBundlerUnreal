// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetBundlerService.h"

#include "Containers/Ticker.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/AsyncTaskNotification.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ScopedTransaction.h"
#include "Animation/Skeleton.h"

namespace
{
	FString GetPackagePath(const UObject* InAsset)
	{
		return InAsset ? FPackageName::GetLongPackagePath(InAsset->GetOutermost()->GetName()) : FString();
	}

	template <typename AssetType>
	void AddUniqueAsset(TArray<TWeakObjectPtr<AssetType>>& InAssets, TSet<TObjectKey<UObject>>& InSeenAssets, AssetType* InAsset)
	{
		if (!InAsset || InSeenAssets.Contains(InAsset))
		{
			return;
		}

		InSeenAssets.Add(InAsset);
		InAssets.Add(InAsset);
	}

	void AddMoveItem(FAssetBundlerMovePlan& InPlan, TSet<TObjectKey<UObject>>& InSeenAssets, UObject* InAsset, const TCHAR* InTypeLabel)
	{
		if (!InAsset || InSeenAssets.Contains(InAsset))
		{
			return;
		}

		const FString sourcePath = GetPackagePath(InAsset);
		if (sourcePath.IsEmpty() || sourcePath == InPlan.TargetFolder)
		{
			InPlan.SkippedCount++;
			return;
		}

		InSeenAssets.Add(InAsset);

		FAssetBundlerMoveItem item;
		item.Asset = InAsset;
		item.SourcePath = sourcePath;
		item.TargetPath = InPlan.TargetFolder;
		item.TypeLabel = InTypeLabel;
		InPlan.ItemsToMove.Add(MoveTemp(item));
	}

	void MarkMovedAssetDirty(UObject* InAsset)
	{
		if (!InAsset)
		{
			return;
		}

		if (UPackage* outermostPackage = InAsset->GetOutermost())
		{
			outermostPackage->SetDirtyFlag(true);
			outermostPackage->MarkPackageDirty();
		}

		InAsset->MarkPackageDirty();
		if (UPackage* package = InAsset->GetPackage())
		{
			package->SetDirtyFlag(true);
			package->MarkPackageDirty();
		}
	}
}

FAssetBundlerMoveRunner::FAssetBundlerMoveRunner(FAssetBundlerMovePlan InPlan)
	: Plan(MoveTemp(InPlan))
{
	Result.TotalCount = Plan.ItemsToMove.Num();
	Result.SkippedCount = Plan.SkippedCount;
}

FAssetBundlerMoveRunner::~FAssetBundlerMoveRunner()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

void FAssetBundlerMoveRunner::Start()
{
	if (bRunning)
	{
		return;
	}

	bRunning = true;
	Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("AssetBundler", "BundleAssetsTransaction", "Bundle Assets"));

	FAsyncTaskNotificationConfig notificationConfig;
	notificationConfig.TitleText = NSLOCTEXT("AssetBundler", "BundleNotificationTitle", "Bundling assets");
	notificationConfig.ProgressText = FText::Format(
		NSLOCTEXT("AssetBundler", "BundleNotificationStart", "Moving assets 0/{0}"),
		FText::AsNumber(Result.TotalCount));
	notificationConfig.bCanCancel = true;
	notificationConfig.bKeepOpenOnFailure = true;
	Notification = MakeShared<FAsyncTaskNotification>(notificationConfig);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(AsShared(), &FAssetBundlerMoveRunner::Tick),
		0.0f);
}

void FAssetBundlerMoveRunner::Cancel()
{
	bCancelRequested = true;
}

bool FAssetBundlerMoveRunner::IsRunning() const
{
	return bRunning;
}

const FAssetBundlerMovePlan& FAssetBundlerMoveRunner::GetPlan() const
{
	return Plan;
}

bool FAssetBundlerMoveRunner::Tick(float InDeltaTime)
{
	if (!bRunning)
	{
		return false;
	}

	if (Notification.IsValid() && Notification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel)
	{
		bCancelRequested = true;
	}

	if (bCancelRequested)
	{
		Finish(true);
		return false;
	}

	if (!Plan.ItemsToMove.IsValidIndex(CurrentIndex))
	{
		Finish(false);
		return false;
	}

	const FAssetBundlerMoveItem& item = Plan.ItemsToMove[CurrentIndex];
	if (Notification.IsValid())
	{
		Notification->SetProgressText(FText::Format(
			NSLOCTEXT("AssetBundler", "BundleNotificationProgress", "Moving {0} {1}/{2}"),
			FText::FromString(item.Asset.IsValid() ? item.Asset->GetName() : FString(TEXT("Unknown"))),
			FText::AsNumber(CurrentIndex + 1),
			FText::AsNumber(Result.TotalCount)));
	}

	FString error;
	if (MoveItem(item, error))
	{
		Result.MovedCount++;
	}
	else
	{
		Result.FailedCount++;
		Result.Errors.Add(error);
	}

	CurrentIndex++;
	OnProgress.ExecuteIfBound(CurrentIndex, Result.TotalCount);
	return true;
}

void FAssetBundlerMoveRunner::Finish(bool bInCanceled)
{
	bRunning = false;
	Result.bCanceled = bInCanceled;

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (Notification.IsValid())
	{
		if (bInCanceled)
		{
			Notification->SetComplete(
				NSLOCTEXT("AssetBundler", "BundleNotificationCanceledTitle", "Bundling canceled"),
				FText::Format(
					NSLOCTEXT("AssetBundler", "BundleNotificationCanceled", "Moved {0}/{1} assets"),
					FText::AsNumber(Result.MovedCount),
					FText::AsNumber(Result.TotalCount)),
				false);
		}
		else
		{
			const bool bSuccess = Result.FailedCount == 0;
			Notification->SetComplete(
				bSuccess
					? NSLOCTEXT("AssetBundler", "BundleNotificationDoneTitle", "Bundling complete")
					: NSLOCTEXT("AssetBundler", "BundleNotificationFailedTitle", "Bundling finished with errors"),
				FText::Format(
					NSLOCTEXT("AssetBundler", "BundleNotificationDone", "Moved {0}/{1} assets"),
					FText::AsNumber(Result.MovedCount),
					FText::AsNumber(Result.TotalCount)),
				bSuccess);
		}
	}

	Transaction.Reset();
	OnFinished.ExecuteIfBound(Result);
}

bool FAssetBundlerMoveRunner::MoveItem(const FAssetBundlerMoveItem& InItem, FString& OutError)
{
	UObject* asset = InItem.Asset.Get();
	if (!asset)
	{
		OutError = FString::Printf(TEXT("Missing %s asset"), *InItem.TypeLabel);
		return false;
	}

	asset->Modify();
	if (UPackage* outermostPackage = asset->GetOutermost())
	{
		outermostPackage->Modify();
	}
	if (UPackage* package = asset->GetPackage())
	{
		package->Modify();
	}

	ObjectTools::FPackageGroupName packageGroupName;
	packageGroupName.ObjectName = asset->GetName();
	packageGroupName.GroupName = TEXT("");
	packageGroupName.PackageName = InItem.TargetPath / asset->GetName();

	TSet<UPackage*> packagesUserRefusedToFullyLoad;
	FText errorMessage;
	const bool bLeaveRedirector = true;
	if (!ObjectTools::RenameSingleObject(asset, packageGroupName, packagesUserRefusedToFullyLoad, errorMessage, nullptr, bLeaveRedirector))
	{
		OutError = errorMessage.IsEmpty()
			? FString::Printf(TEXT("Failed to move %s: %s"), *InItem.TypeLabel, *asset->GetPathName())
			: errorMessage.ToString();
		return false;
	}

	MarkMovedAssetDirty(asset);

	return true;
}

FAssetBundlerMovePlan FAssetBundlerService::BuildMovePlan(
	USkeletalMesh* InSkeletalMesh,
	UStaticMesh* InStaticMesh,
	UPhysicsAsset* InPhysicsAsset,
	USkeleton* InSkeleton,
	const TArray<TWeakObjectPtr<UMaterialInterface>>& InMaterials,
	const TArray<TWeakObjectPtr<UTexture>>& InTextures,
	bool bInCreateSubfolder,
	const FString& InSubfolderName,
	bool bInMovePhysicsAsset,
	bool bInMoveSkeleton)
{
	FAssetBundlerMovePlan plan;
	plan.SkeletalMesh = InSkeletalMesh;
	plan.StaticMesh = InStaticMesh;
	plan.PhysicsAsset = InPhysicsAsset;
	plan.Skeleton = InSkeleton;
	plan.TargetFolder = InSkeletalMesh ? GetPackagePath(InSkeletalMesh) : GetPackagePath(InStaticMesh);
	if (bInCreateSubfolder && !InSubfolderName.IsEmpty())
	{
		plan.TargetFolder /= InSubfolderName;
	}

	TSet<TObjectKey<UObject>> seenCollections;
	for (const TWeakObjectPtr<UMaterialInterface>& material : InMaterials)
	{
		AddUniqueAsset(plan.Materials, seenCollections, material.Get());
	}

	for (const TWeakObjectPtr<UTexture>& texture : InTextures)
	{
		AddUniqueAsset(plan.Textures, seenCollections, texture.Get());
	}

	TSet<TObjectKey<UObject>> seenMoves;
	if (bInCreateSubfolder)
	{
		AddMoveItem(plan, seenMoves, InSkeletalMesh, TEXT("Skeletal Mesh"));
		AddMoveItem(plan, seenMoves, InStaticMesh, TEXT("Static Mesh"));
	}

	if (bInMovePhysicsAsset)
	{
		AddMoveItem(plan, seenMoves, InPhysicsAsset, TEXT("Physics Asset"));
	}

	if (bInMoveSkeleton)
	{
		AddMoveItem(plan, seenMoves, InSkeleton, TEXT("Skeleton"));
	}

	for (const TWeakObjectPtr<UMaterialInterface>& material : plan.Materials)
	{
		AddMoveItem(plan, seenMoves, material.Get(), TEXT("Material"));
	}

	for (const TWeakObjectPtr<UTexture>& texture : plan.Textures)
	{
		AddMoveItem(plan, seenMoves, texture.Get(), TEXT("Texture"));
	}

	return plan;
}
