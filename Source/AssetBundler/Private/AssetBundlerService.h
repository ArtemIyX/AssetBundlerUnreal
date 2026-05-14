// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class UPhysicsAsset;
class USkeletalMesh;
class UStaticMesh;
class USkeleton;
class UMaterialInterface;
class UTexture;
class FAsyncTaskNotification;
class FScopedTransaction;

struct FAssetBundlerMoveItem
{
	TWeakObjectPtr<UObject> Asset;
	FString SourcePath;
	FString TargetPath;
	FString TypeLabel;
};

struct FAssetBundlerMovePlan
{
	TWeakObjectPtr<USkeletalMesh> SkeletalMesh;
	TWeakObjectPtr<UStaticMesh> StaticMesh;
	TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
	TWeakObjectPtr<USkeleton> Skeleton;
	FString TargetFolder;
	TArray<TWeakObjectPtr<UMaterialInterface>> Materials;
	TArray<TWeakObjectPtr<UTexture>> Textures;
	TArray<FAssetBundlerMoveItem> ItemsToMove;
	int32 SkippedCount = 0;

	bool IsValid() const
	{
		return (SkeletalMesh.IsValid() || StaticMesh.IsValid()) && !TargetFolder.IsEmpty();
	}
};

struct FAssetBundlerRunResult
{
	int32 TotalCount = 0;
	int32 MovedCount = 0;
	int32 FailedCount = 0;
	int32 SkippedCount = 0;
	bool bCanceled = false;
	TArray<FString> Errors;
};

class FAssetBundlerMoveRunner : public TSharedFromThis<FAssetBundlerMoveRunner>
{
public:
	DECLARE_DELEGATE_TwoParams(FOnProgress, int32, int32)
	DECLARE_DELEGATE_OneParam(FOnFinished, const FAssetBundlerRunResult&)

	explicit FAssetBundlerMoveRunner(FAssetBundlerMovePlan InPlan);
	~FAssetBundlerMoveRunner();

	void Start();
	void Cancel();
	bool IsRunning() const;
	const FAssetBundlerMovePlan& GetPlan() const;

	FOnProgress OnProgress;
	FOnFinished OnFinished;

private:
	bool Tick(float InDeltaTime);
	void Finish(bool bInCanceled);
	bool MoveItem(const FAssetBundlerMoveItem& InItem, FString& OutError);

private:
	FAssetBundlerMovePlan Plan;
	FAssetBundlerRunResult Result;
	TSharedPtr<FAsyncTaskNotification> Notification;
	TUniquePtr<FScopedTransaction> Transaction;
	FTSTicker::FDelegateHandle TickHandle;
	TAtomic<bool> bCancelRequested = false;
	bool bRunning = false;
	int32 CurrentIndex = 0;
};

class FAssetBundlerService
{
public:
	static FAssetBundlerMovePlan BuildMovePlan(
		USkeletalMesh* InSkeletalMesh,
		UStaticMesh* InStaticMesh,
		UPhysicsAsset* InPhysicsAsset,
		USkeleton* InSkeleton,
		const TArray<TWeakObjectPtr<UMaterialInterface>>& InMaterials,
		const TArray<TWeakObjectPtr<UTexture>>& InTextures,
		bool bInMovePhysicsAsset,
		bool bInMoveSkeleton);
};
