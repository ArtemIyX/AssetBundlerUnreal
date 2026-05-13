# AssetBundler

AssetBundler is an Unreal Engine editor plugin that bundles a skeletal mesh with its related assets into a single folder.

<img width="992" height="695" alt="image" src="https://github.com/user-attachments/assets/18577f0b-0b10-4bdd-b23d-41c66f5bb3c9" />


It is focused on cleanup and organization of character or weapon asset sets where the skeletal mesh, physics asset, skeleton, materials, and textures are spread across different locations.

## Features

- Content Browser context action: `Bundle Assets`
- Supports selection from:
  - `Skeletal Mesh`
  - `Physics Asset`
- Detects and displays:
  - `Skeletal Mesh`
  - `Physics Asset`
  - `Skeleton`
- Shows related assets:
  - used materials
  - used textures from those materials
- Double-click navigation to Content Browser for:
  - skeletal mesh
  - physics asset
  - skeleton
  - materials
  - textures
- Move options:
  - `Move Physics Asset` default: `true`
  - `Move Skeleton` default: `false`
- Always moves:
  - materials
  - textures
- Never moves:
  - skeletal mesh
- Moves assets into the same folder as the selected skeletal mesh
- Async bundling flow with:
  - confirmation dialog
  - progress display
  - cancel support
- Undo-safe asset moves
- Moved assets are marked dirty

## Current Behavior

When bundling starts, AssetBundler builds a move plan using the selected skeletal mesh as the target root.

Move rules:

- skeletal mesh: not moved
- physics asset: moved only if enabled
- skeleton: moved only if enabled
- materials: always moved
- textures used by those materials: always moved

Target folder:

- the folder containing the skeletal mesh

If an asset is already in the target folder, it is skipped.

## Usage


<img width="414" height="281" alt="image" src="https://github.com/user-attachments/assets/318af6d8-9d01-429d-9f60-ecd87c459eaf" />

1. In the Content Browser, right click a `Skeletal Mesh` or `Physics Asset`.
2. Click `Bundle Assets`.
3. Review detected assets in the Asset Bundler window.
4. Configure:
   - `Move Physics Asset`
   - `Move Skeleton`
5. Click `Bundle`.
6. Confirm the move summary.
7. Wait for bundling to finish or click `Cancel` while it is running.

## UI Overview

The window currently includes:

- preview rows for:
  - skeletal mesh
  - physics asset
  - skeleton
- move options block
- used materials list
- used textures list
- progress bar and status text
- `Bundle` and `Cancel` buttons

## Asset Detection

Physics asset resolution uses several fallbacks so it can work from either selected asset type:

1. direct skeletal mesh physics asset reference
2. skeletal mesh asset registry tag
3. physics asset lookup by `PreviewSkeletalMesh`

## Undo and Dirty State

Bundle operations are wrapped in an editor transaction.

This means:

- asset moves are undoable
- moved assets are explicitly modified
- moved assets and packages are marked dirty

## Installation

### As a project plugin

Copy the `AssetBundler` folder into:

```text
YourProject/Plugins/AssetBundler
```

Then regenerate project files if needed and open the project in Unreal Editor.

### Build requirements

- Unreal Engine 5
- Editor module build

## Repository Tags

Suggested GitHub topics:

- `unreal-engine`
- `ue5`
- `plugin`
- `editor-plugin`
- `asset-management`
- `content-browser`
- `skeletal-mesh`
- `physics-asset`
- `pipeline`
- `tools`

## Status

Current state:

- editor-only plugin
- focused on skeletal mesh asset bundling
- intended for internal tooling and workflow acceleration

## Limitations

- currently supports only skeletal mesh oriented workflows
- currently opens from:
  - skeletal mesh
  - physics asset
- move logic is limited to:
  - physics asset
  - skeleton
  - materials
  - textures
- no custom destination folder yet
- no preview diff panel yet
- no reference graph visualization yet

## Roadmap Ideas
- [ ] custom destination folder
- [ ] detailed move report
- [ ] reference graph preview
- [ ] support for animation assets
- [ ] filtering which materials or textures should be moved

## License

[MIT](LICENSE)


