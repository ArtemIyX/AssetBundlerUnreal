// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class SWidget;
class USkeletalMesh;
class UStaticMesh;
class UPhysicsAsset;
class USkeleton;
class STableViewBase;
class SDockTab;
class FAssetThumbnailPool;
class SVerticalBox;
class FAssetBundlerMoveRunner;
struct FAssetBundlerMovePlan;
struct FAssetBundlerRunResult;

struct FDisplayedAssetItem
{
	TWeakObjectPtr<UObject> Asset;
};

class SAssetBundlerWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetBundlerWindow) {}
		SLATE_ARGUMENT(TWeakPtr<SDockTab>, OwnerTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetSelectedAssets(const TArray<FAssetData>& InAssets);

private:
	enum class EAssetBundlerMode : uint8
	{
		None,
		SkeletalMesh,
		StaticMesh
	};

	TSharedRef<SWidget> BuildPropertyPanel();
	TSharedRef<SWidget> BuildAssetPreviewRow(const FText& InLabel, UObject* InAsset) const;
	TSharedRef<SWidget> BuildTargetOptionsPanel();
	TSharedRef<SWidget> BuildMoveOptionsPanel();
	TSharedRef<SWidget> BuildBoolPropertyRow(const FText& InLabel, ECheckBoxState (SAssetBundlerWindow::*InGetter)() const, void (SAssetBundlerWindow::*InSetter)(ECheckBoxState));
	TSharedRef<SWidget> BuildFooterPanel();
	TSharedRef<SWidget> BuildAssetListPanel(const FText& InTitle, const TSharedRef<SWidget>& InList);
	TSharedRef<ITableRow> GenerateAssetRow(TSharedPtr<FDisplayedAssetItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable);
	void HandleAssetDoubleClick(TSharedPtr<FDisplayedAssetItem> InItem);
	FReply SyncToAsset(UObject* InAsset) const;
	FReply OnBundleClicked();
	FReply OnCancelClicked();
	bool CanBundle() const;
	bool CanEditOptions() const;
	void SetCreateSubfolder(ECheckBoxState InState);
	ECheckBoxState GetCreateSubfolderCheckState() const;
	void OnSubfolderNameChanged(const FText& InText);
	FText GetSubfolderNameText() const;
	FText GetSubfolderTargetText() const;
	void HandleMoveProgress(int32 InCurrent, int32 InTotal);
	void HandleMoveFinished(const FAssetBundlerRunResult& InResult);
	FText GetProgressText() const;
	TOptional<float> GetProgressFraction() const;
	FText BuildConfirmationText(const FAssetBundlerMovePlan& InPlan) const;
	void SetMovePhysicsAsset(ECheckBoxState InState);
	ECheckBoxState GetMovePhysicsAssetCheckState() const;
	void SetMoveSkeleton(ECheckBoxState InState);
	ECheckBoxState GetMoveSkeletonCheckState() const;
	void RefreshPropertyPanel();
	void RefreshAssetLists();
	void RefreshDefaultSubfolderName();
	void LoadSettings();
	void SaveSettings() const;
	FString GetDefaultSubfolderName() const;
	FString GetTargetRootFolder() const;
	TArray<TWeakObjectPtr<class UMaterialInterface>> GetMaterialAssets() const;
	TArray<TWeakObjectPtr<class UTexture>> GetTextureAssets() const;

private:
	EAssetBundlerMode mode = EAssetBundlerMode::None;
	TWeakObjectPtr<USkeletalMesh> skeletalMesh;
	TWeakObjectPtr<UStaticMesh> staticMesh;
	TWeakObjectPtr<UPhysicsAsset> physicsAsset;
	TWeakObjectPtr<USkeleton> skeleton;
	bool bCreateSubfolder = false;
	bool bMovePhysicsAsset = true;
	bool bMoveSkeleton = false;
	bool bIsRunning = false;
	int32 progressCurrent = 0;
	int32 progressTotal = 0;
	FString subfolderName;
	TArray<TSharedPtr<FDisplayedAssetItem>> materialItems;
	TArray<TSharedPtr<FDisplayedAssetItem>> textureItems;
	TSharedPtr<SVerticalBox> propertyBox;
	TSharedPtr<FAssetThumbnailPool> thumbnailPool;
	TSharedPtr<SListView<TSharedPtr<FDisplayedAssetItem>>> materialsView;
	TSharedPtr<SListView<TSharedPtr<FDisplayedAssetItem>>> texturesView;
	TSharedPtr<FAssetBundlerMoveRunner> moveRunner;
	TWeakPtr<SDockTab> ownerTab;
};

TSharedRef<SWidget> CreateAssetBundlerWindow(const TSharedPtr<SDockTab>& InOwnerTab, TWeakPtr<SAssetBundlerWindow>* OutWindow = nullptr);
bool SetAssetsToAssetBundlerWindow(const TWeakPtr<SAssetBundlerWindow>& InWindow, const TArray<FAssetData>& InAssets);
