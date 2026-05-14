// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetBundlerWindow.h"

#include "AssetBundlerService.h"
#include "Algo/Sort.h"
#include "AssetThumbnail.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Animation/Skeleton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SAssetBundlerWindow"

namespace
{
	UPhysicsAsset* ResolvePhysicsAssetFromAssetData(const FAssetData& InAssetData)
	{
		FString objectPathText;
		if (!InAssetData.GetTagValue(USkeletalMesh::GetPhysicsAssetMemberName(), objectPathText) || objectPathText.IsEmpty())
		{
			return nullptr;
		}

		if (FSoftObjectPath softPath(objectPathText); softPath.IsValid())
		{
			if (UPhysicsAsset* loadedAsset = Cast<UPhysicsAsset>(softPath.TryLoad()))
			{
				return loadedAsset;
			}
		}

		const FString objectPath = FPackageName::ExportTextPathToObjectPath(objectPathText);
		return objectPath.IsEmpty() ? nullptr : LoadObject<UPhysicsAsset>(nullptr, *objectPath);
	}

	UPhysicsAsset* ResolvePhysicsAssetFromPreviewMesh(USkeletalMesh* InSkeletalMesh)
	{
		if (!InSkeletalMesh)
		{
			return nullptr;
		}

		FARFilter filter;
		filter.ClassPaths.Add(UPhysicsAsset::StaticClass()->GetClassPathName());
		filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PreviewSkeletalMesh), FSoftObjectPath(InSkeletalMesh).ToString());
		filter.PackagePaths.Add(*FPackageName::GetLongPackagePath(InSkeletalMesh->GetOutermost()->GetName()));
		filter.bRecursivePaths = false;

		TArray<FAssetData> assets;
		FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		assetRegistryModule.Get().GetAssets(filter, assets);

		if (assets.IsEmpty())
		{
			filter.PackagePaths.Reset();
			assetRegistryModule.Get().GetAssets(filter, assets);
		}

		return assets.IsEmpty() ? nullptr : Cast<UPhysicsAsset>(assets[0].GetAsset());
	}

	class SAssetPreviewRow final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAssetPreviewRow) {}
			SLATE_ARGUMENT(FText, Label)
			SLATE_ARGUMENT(UObject*, Asset)
			SLATE_ARGUMENT(TSharedPtr<FAssetThumbnailPool>, ThumbnailPool)
			SLATE_EVENT(FOnClicked, OnDoubleClicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			asset = InArgs._Asset;
			onDoubleClicked = InArgs._OnDoubleClicked;
			SetCursor(EMouseCursor::Hand);

			TSharedRef<SWidget> thumbnailWidget = SNew(SBox)
				.WidthOverride(48.0f)
				.HeightOverride(48.0f);

			if (asset.IsValid() && InArgs._ThumbnailPool.IsValid())
			{
				thumbnail = MakeShared<FAssetThumbnail>(FAssetData(asset.Get()), 48, 48, InArgs._ThumbnailPool);
				thumbnailWidget = thumbnail->MakeThumbnailWidget();
			}

			ChildSlot
			[
				SAssignNew(border, SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.BorderBackgroundColor(this, &SAssetPreviewRow::GetBorderColor)
				.Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(InArgs._Label)
						.MinDesiredWidth(110.0f)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						thumbnailWidget
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(asset.IsValid() ? FText::FromName(asset->GetFName()) : LOCTEXT("NoneAssetName", "None"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(asset.IsValid() ? FText::FromString(asset->GetPathName()) : LOCTEXT("NoneValue", "None"))
						]
					]
				]
			];
		}

		virtual FReply OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
		{
			if (asset.IsValid())
			{
				return onDoubleClicked.IsBound() ? onDoubleClicked.Execute() : FReply::Handled();
			}

			return SCompoundWidget::OnMouseButtonDoubleClick(InGeometry, InMouseEvent);
		}

		virtual void OnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
		{
			SCompoundWidget::OnMouseEnter(InGeometry, InMouseEvent);
			bIsHovered = true;
		}

		virtual void OnMouseLeave(const FPointerEvent& InMouseEvent) override
		{
			SCompoundWidget::OnMouseLeave(InMouseEvent);
			bIsHovered = false;
		}

		FSlateColor GetBorderColor() const
		{
			if (!asset.IsValid())
			{
				return FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
			}

			return bIsHovered
				? FLinearColor(0.18f, 0.32f, 0.55f, 1.0f)
				: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
		}

	private:
		TWeakObjectPtr<UObject> asset;
		FOnClicked onDoubleClicked;
		TSharedPtr<FAssetThumbnail> thumbnail;
		TSharedPtr<SBorder> border;
		bool bIsHovered = false;
	};
}

void SAssetBundlerWindow::Construct(const FArguments& InArgs)
{
	ownerTab = InArgs._OwnerTab;
	thumbnailPool = MakeShared<FAssetThumbnailPool>(32);

	materialsView = SNew(SListView<TSharedPtr<FDisplayedAssetItem>>)
		.ListItemsSource(&materialItems)
		.OnGenerateRow(this, &SAssetBundlerWindow::GenerateAssetRow)
		.OnMouseButtonDoubleClick(this, &SAssetBundlerWindow::HandleAssetDoubleClick)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(TEXT("Name")).DefaultLabel(LOCTEXT("NameColumn", "Name")).FillWidth(0.35f)
			+ SHeaderRow::Column(TEXT("Path")).DefaultLabel(LOCTEXT("PathColumn", "Path")).FillWidth(0.65f));

	texturesView = SNew(SListView<TSharedPtr<FDisplayedAssetItem>>)
		.ListItemsSource(&textureItems)
		.OnGenerateRow(this, &SAssetBundlerWindow::GenerateAssetRow)
		.OnMouseButtonDoubleClick(this, &SAssetBundlerWindow::HandleAssetDoubleClick)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(TEXT("Name")).DefaultLabel(LOCTEXT("NameColumn", "Name")).FillWidth(0.35f)
			+ SHeaderRow::Column(TEXT("Path")).DefaultLabel(LOCTEXT("PathColumn", "Path")).FillWidth(0.65f));

	ChildSlot
	[
		SNew(SBorder)
		.Padding(16.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildPropertyPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				BuildMoveOptionsPanel()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					BuildAssetListPanel(LOCTEXT("MaterialsTitle", "Used Materials"), materialsView.ToSharedRef())
				]
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					BuildAssetListPanel(LOCTEXT("TexturesTitle", "Used Textures"), texturesView.ToSharedRef())
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				BuildFooterPanel()
			]
		]
	];
}

void SAssetBundlerWindow::SetSelectedAssets(const TArray<FAssetData>& InAssets)
{
	skeletalMesh = nullptr;
	staticMesh = nullptr;
	physicsAsset = nullptr;
	FAssetData skeletalMeshAssetData;
	bool bHasSkeletalMeshAssetData = false;
	mode = EAssetBundlerMode::None;

	for (const FAssetData& assetData : InAssets)
	{
		if (!skeletalMesh.IsValid())
		{
			skeletalMesh = Cast<USkeletalMesh>(assetData.GetAsset());
			if (skeletalMesh.IsValid())
			{
				skeletalMeshAssetData = assetData;
				bHasSkeletalMeshAssetData = true;
			}
		}

		if (!staticMesh.IsValid())
		{
			staticMesh = Cast<UStaticMesh>(assetData.GetAsset());
			if (staticMesh.IsValid())
			{
				mode = EAssetBundlerMode::StaticMesh;
			}
		}

		if (!physicsAsset.IsValid())
		{
			physicsAsset = Cast<UPhysicsAsset>(assetData.GetAsset());
		}
	}

	if (skeletalMesh.IsValid())
	{
		mode = EAssetBundlerMode::SkeletalMesh;
		staticMesh = nullptr;
	}

	if (!skeletalMesh.IsValid() && physicsAsset.IsValid())
	{
		skeletalMesh = physicsAsset->GetPreviewMesh();
		if (skeletalMesh.IsValid())
		{
			mode = EAssetBundlerMode::SkeletalMesh;
		}
	}

	if (!physicsAsset.IsValid() && skeletalMesh.IsValid())
	{
		physicsAsset = skeletalMesh->GetPhysicsAsset();
		if (!physicsAsset.IsValid() && bHasSkeletalMeshAssetData)
		{
			physicsAsset = ResolvePhysicsAssetFromAssetData(skeletalMeshAssetData);
		}
		if (!physicsAsset.IsValid())
		{
			physicsAsset = ResolvePhysicsAssetFromPreviewMesh(skeletalMesh.Get());
		}
	}

	skeleton = skeletalMesh.IsValid() ? skeletalMesh->GetSkeleton() : nullptr;
	RefreshPropertyPanel();
	RefreshAssetLists();
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildPropertyPanel()
{
	SAssignNew(propertyBox, SVerticalBox);
	RefreshPropertyPanel();
	return propertyBox.ToSharedRef();
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildMoveOptionsPanel()
{
	if (mode != EAssetBundlerMode::SkeletalMesh)
	{
		return SNew(SBox);
	}

	return SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MoveOptionsTitle", "Move Options"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildBoolPropertyRow(LOCTEXT("MovePhysicsAssetLabel", "Move Physics Asset"), &SAssetBundlerWindow::GetMovePhysicsAssetCheckState, &SAssetBundlerWindow::SetMovePhysicsAsset)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				BuildBoolPropertyRow(LOCTEXT("MoveSkeletonLabel", "Move Skeleton"), &SAssetBundlerWindow::GetMoveSkeletonCheckState, &SAssetBundlerWindow::SetMoveSkeleton)
			]
		];
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildAssetPreviewRow(const FText& InLabel, UObject* InAsset) const
{
	return SNew(SAssetPreviewRow)
		.Label(InLabel)
		.Asset(InAsset)
		.ThumbnailPool(thumbnailPool)
		.OnDoubleClicked(FOnClicked::CreateSP(this, &SAssetBundlerWindow::SyncToAsset, InAsset));
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildBoolPropertyRow(const FText& InLabel, ECheckBoxState (SAssetBundlerWindow::*InGetter)() const, void (SAssetBundlerWindow::*InSetter)(ECheckBoxState))
{
	return SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InLabel)
				.MinDesiredWidth(110.0f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, InGetter)
				.OnCheckStateChanged(this, InSetter)
				.IsEnabled(this, &SAssetBundlerWindow::CanEditOptions)
			]
		];
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildFooterPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SProgressBar)
			.Percent(this, &SAssetBundlerWindow::GetProgressFraction)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetBundlerWindow::GetProgressText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(6.0f, 0.0f))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("BundleButtonLabel", "Bundle"))
				.IsEnabled(this, &SAssetBundlerWindow::CanBundle)
				.OnClicked(this, &SAssetBundlerWindow::OnBundleClicked)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
				.OnClicked(this, &SAssetBundlerWindow::OnCancelClicked)
			]
		];
}

TSharedRef<SWidget> SAssetBundlerWindow::BuildAssetListPanel(const FText& InTitle, const TSharedRef<SWidget>& InList)
{
	return SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(InTitle)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				InList
			]
		];
}

TSharedRef<ITableRow> SAssetBundlerWindow::GenerateAssetRow(TSharedPtr<FDisplayedAssetItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(STableRow<TSharedPtr<FDisplayedAssetItem>>, InOwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.35f)
			.VAlign(VAlign_Center)
			.Padding(4.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem.IsValid() && InItem->Asset.IsValid() ? InItem->Asset->GetFName() : NAME_None))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.65f)
			.VAlign(VAlign_Center)
			.Padding(4.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem.IsValid() && InItem->Asset.IsValid() ? InItem->Asset->GetPathName() : FString()))
			]
		];
}

void SAssetBundlerWindow::HandleAssetDoubleClick(TSharedPtr<FDisplayedAssetItem> InItem)
{
	if (!InItem.IsValid() || !InItem->Asset.IsValid())
	{
		return;
	}

	SyncToAsset(InItem->Asset.Get());
}

FReply SAssetBundlerWindow::SyncToAsset(UObject* InAsset) const
{
	if (!InAsset)
	{
		return FReply::Handled();
	}

	TArray<UObject*> assetsToSync;
	assetsToSync.Add(InAsset);

	FContentBrowserModule& contentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	contentBrowserModule.Get().SyncBrowserToAssets(assetsToSync);
	return FReply::Handled();
}

FReply SAssetBundlerWindow::OnBundleClicked()
{
	const FAssetBundlerMovePlan plan = FAssetBundlerService::BuildMovePlan(
		skeletalMesh.Get(),
		staticMesh.Get(),
		physicsAsset.Get(),
		skeleton.Get(),
		GetMaterialAssets(),
		GetTextureAssets(),
		bMovePhysicsAsset,
		bMoveSkeleton);

	if (!plan.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BundleInvalidPlan", "A Skeletal Mesh or Static Mesh must be valid to bundle related assets."));
		return FReply::Handled();
	}

	if (plan.ItemsToMove.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BundleNothingToMove", "Nothing needs to be moved."));
		return FReply::Handled();
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, BuildConfirmationText(plan)) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	bIsRunning = true;
	progressCurrent = 0;
	progressTotal = plan.ItemsToMove.Num();
	moveRunner = MakeShared<FAssetBundlerMoveRunner>(plan);
	moveRunner->OnProgress.BindSP(this, &SAssetBundlerWindow::HandleMoveProgress);
	moveRunner->OnFinished.BindSP(this, &SAssetBundlerWindow::HandleMoveFinished);
	moveRunner->Start();
	return FReply::Handled();
}

FReply SAssetBundlerWindow::OnCancelClicked()
{
	if (moveRunner.IsValid() && bIsRunning)
	{
		moveRunner->Cancel();
		return FReply::Handled();
	}

	if (ownerTab.IsValid())
	{
		ownerTab.Pin()->RequestCloseTab();
	}

	return FReply::Handled();
}

bool SAssetBundlerWindow::CanBundle() const
{
	return !bIsRunning && (skeletalMesh.IsValid() || staticMesh.IsValid());
}

bool SAssetBundlerWindow::CanEditOptions() const
{
	return !bIsRunning;
}

void SAssetBundlerWindow::HandleMoveProgress(int32 InCurrent, int32 InTotal)
{
	progressCurrent = InCurrent;
	progressTotal = InTotal;
}

void SAssetBundlerWindow::HandleMoveFinished(const FAssetBundlerRunResult& InResult)
{
	bIsRunning = false;
	progressCurrent = InResult.TotalCount;
	progressTotal = InResult.TotalCount;
	moveRunner.Reset();
	RefreshPropertyPanel();
	RefreshAssetLists();

	FString text = FString::Printf(
		TEXT("Moved: %d\nFailed: %d\nSkipped: %d"),
		InResult.MovedCount,
		InResult.FailedCount,
		InResult.SkippedCount);

	if (InResult.bCanceled)
	{
		text = FString::Printf(TEXT("Canceled.\n\n%s"), *text);
	}

	if (InResult.Errors.Num() > 0)
	{
		text += TEXT("\n\nErrors:\n");
		for (const FString& error : InResult.Errors)
		{
			text += error + TEXT("\n");
		}
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(text));
}

FText SAssetBundlerWindow::GetProgressText() const
{
	if (bIsRunning)
	{
		return FText::Format(
			LOCTEXT("ProgressRunningText", "Moving {0}/{1}"),
			FText::AsNumber(progressCurrent),
			FText::AsNumber(progressTotal));
	}

	if (progressTotal > 0)
	{
		return FText::Format(
			LOCTEXT("ProgressDoneText", "Ready. Last run processed {0} assets"),
			FText::AsNumber(progressTotal));
	}

	return LOCTEXT("ProgressIdleText", "Ready");
}

TOptional<float> SAssetBundlerWindow::GetProgressFraction() const
{
	if (progressTotal <= 0)
	{
		return 0.0f;
	}

	return static_cast<float>(progressCurrent) / static_cast<float>(progressTotal);
}

FText SAssetBundlerWindow::BuildConfirmationText(const FAssetBundlerMovePlan& InPlan) const
{
	return FText::Format(
		LOCTEXT("BundleConfirmText", "Are you sure?\n\nTarget folder:\n{0}\n\nWill move:\nPhysics Asset: {1}\nSkeleton: {2}\nMaterials: {3}\nTextures: {4}\nTotal assets to move: {5}\nSkipped already there: {6}"),
		FText::FromString(InPlan.TargetFolder),
		FText::AsNumber(InPlan.PhysicsAsset.IsValid() && bMovePhysicsAsset ? 1 : 0),
		FText::AsNumber(InPlan.Skeleton.IsValid() && bMoveSkeleton ? 1 : 0),
		FText::AsNumber(InPlan.Materials.Num()),
		FText::AsNumber(InPlan.Textures.Num()),
		FText::AsNumber(InPlan.ItemsToMove.Num()),
		FText::AsNumber(InPlan.SkippedCount));
}

void SAssetBundlerWindow::RefreshAssetLists()
{
	materialItems.Reset();
	textureItems.Reset();

	if (skeletalMesh.IsValid())
	{
		TSet<TObjectKey<UMaterialInterface>> uniqueMaterials;
		for (const FSkeletalMaterial& materialSlot : skeletalMesh->GetMaterials())
		{
			UMaterialInterface* material = materialSlot.MaterialInterface;
			if (!material || uniqueMaterials.Contains(material))
			{
				continue;
			}

			uniqueMaterials.Add(material);
			materialItems.Add(MakeShared<FDisplayedAssetItem>(FDisplayedAssetItem{ material }));
		}

		TSet<TObjectKey<UTexture>> uniqueTextures;
		for (const TSharedPtr<FDisplayedAssetItem>& materialItem : materialItems)
		{
			UMaterialInterface* material = Cast<UMaterialInterface>(materialItem->Asset.Get());
			if (!material)
			{
				continue;
			}

			TArray<UTexture*> usedTextures;
			material->GetUsedTextures(usedTextures);
			for (UTexture* texture : usedTextures)
			{
				if (!texture || uniqueTextures.Contains(texture))
				{
					continue;
				}

				uniqueTextures.Add(texture);
				textureItems.Add(MakeShared<FDisplayedAssetItem>(FDisplayedAssetItem{ texture }));
			}
		}
	}
	else if (staticMesh.IsValid())
	{
		TSet<TObjectKey<UMaterialInterface>> uniqueMaterials;
		for (const FStaticMaterial& materialSlot : staticMesh->GetStaticMaterials())
		{
			UMaterialInterface* material = materialSlot.MaterialInterface;
			if (!material || uniqueMaterials.Contains(material))
			{
				continue;
			}

			uniqueMaterials.Add(material);
			materialItems.Add(MakeShared<FDisplayedAssetItem>(FDisplayedAssetItem{ material }));
		}

		TSet<TObjectKey<UTexture>> uniqueTextures;
		for (const TSharedPtr<FDisplayedAssetItem>& materialItem : materialItems)
		{
			UMaterialInterface* material = Cast<UMaterialInterface>(materialItem->Asset.Get());
			if (!material)
			{
				continue;
			}

			TArray<UTexture*> usedTextures;
			material->GetUsedTextures(usedTextures);
			for (UTexture* texture : usedTextures)
			{
				if (!texture || uniqueTextures.Contains(texture))
				{
					continue;
				}

				uniqueTextures.Add(texture);
				textureItems.Add(MakeShared<FDisplayedAssetItem>(FDisplayedAssetItem{ texture }));
			}
		}
	}

	Algo::SortBy(materialItems, [](const TSharedPtr<FDisplayedAssetItem>& InItem) {
		return InItem.IsValid() && InItem->Asset.IsValid() ? InItem->Asset->GetName() : FString();
	});
	Algo::SortBy(textureItems, [](const TSharedPtr<FDisplayedAssetItem>& InItem) {
		return InItem.IsValid() && InItem->Asset.IsValid() ? InItem->Asset->GetName() : FString();
	});

	materialsView->RequestListRefresh();
	texturesView->RequestListRefresh();
}

void SAssetBundlerWindow::RefreshPropertyPanel()
{
	if (!propertyBox.IsValid())
	{
		return;
	}

	propertyBox->ClearChildren();
	propertyBox->AddSlot()
	.AutoHeight()
	[
		mode == EAssetBundlerMode::StaticMesh
			? BuildAssetPreviewRow(LOCTEXT("StaticMeshLabel", "Static Mesh"), staticMesh.Get())
			: BuildAssetPreviewRow(LOCTEXT("SkeletalMeshLabel", "Skeletal Mesh"), skeletalMesh.Get())
	];
	if (mode == EAssetBundlerMode::SkeletalMesh)
	{
		propertyBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildAssetPreviewRow(LOCTEXT("PhysicsAssetLabel", "Physics Asset"), physicsAsset.Get())
		];
		propertyBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildAssetPreviewRow(LOCTEXT("SkeletonLabel", "Skeleton"), skeleton.Get())
		];
	}
}

void SAssetBundlerWindow::SetMovePhysicsAsset(ECheckBoxState InState)
{
	bMovePhysicsAsset = InState == ECheckBoxState::Checked;
}

ECheckBoxState SAssetBundlerWindow::GetMovePhysicsAssetCheckState() const
{
	return bMovePhysicsAsset ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAssetBundlerWindow::SetMoveSkeleton(ECheckBoxState InState)
{
	bMoveSkeleton = InState == ECheckBoxState::Checked;
}

ECheckBoxState SAssetBundlerWindow::GetMoveSkeletonCheckState() const
{
	return bMoveSkeleton ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TArray<TWeakObjectPtr<UMaterialInterface>> SAssetBundlerWindow::GetMaterialAssets() const
{
	TArray<TWeakObjectPtr<UMaterialInterface>> outMaterials;
	for (const TSharedPtr<FDisplayedAssetItem>& item : materialItems)
	{
		if (UMaterialInterface* material = Cast<UMaterialInterface>(item.IsValid() ? item->Asset.Get() : nullptr))
		{
			outMaterials.Add(material);
		}
	}

	return outMaterials;
}

TArray<TWeakObjectPtr<UTexture>> SAssetBundlerWindow::GetTextureAssets() const
{
	TArray<TWeakObjectPtr<UTexture>> outTextures;
	for (const TSharedPtr<FDisplayedAssetItem>& item : textureItems)
	{
		if (UTexture* texture = Cast<UTexture>(item.IsValid() ? item->Asset.Get() : nullptr))
		{
			outTextures.Add(texture);
		}
	}

	return outTextures;
}

TSharedRef<SWidget> CreateAssetBundlerWindow(const TSharedPtr<SDockTab>& InOwnerTab, TWeakPtr<SAssetBundlerWindow>* OutWindow)
{
	TSharedRef<SAssetBundlerWindow> window = SNew(SAssetBundlerWindow)
		.OwnerTab(InOwnerTab);
	if (OutWindow)
	{
		*OutWindow = window;
	}

	return window;
}

bool SetAssetsToAssetBundlerWindow(const TWeakPtr<SAssetBundlerWindow>& InWindow, const TArray<FAssetData>& InAssets)
{
	const TSharedPtr<SAssetBundlerWindow> window = InWindow.Pin();
	if (!window.IsValid())
	{
		return false;
	}

	window->SetSelectedAssets(InAssets);
	return true;
}

#undef LOCTEXT_NAMESPACE
