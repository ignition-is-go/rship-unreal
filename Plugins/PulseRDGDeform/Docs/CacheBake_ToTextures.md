# Baking Existing Blueprint Cache Arrays to GPU Textures

## Fast Path (Now Implemented)

Use the built-in editor action:

1. Select your legacy cache asset(s) in Content Browser (assets containing `RestPosList`, `RestNormalList`, `ColorList`).
2. Right-click and choose `Bake Pulse Legacy Cache To Textures`.
3. The tool creates/updates:
   - `T_PulseRestPos_<AssetName>`
   - `T_PulseRestNrm_<AssetName>`
   - `T_PulseMask_<AssetName>`
   - `DA_PulseRDGCache_<AssetName>` (`UPulseDeformCacheAsset`)

This is the recommended route to stay fully GPU-native at runtime.

## Auto Apply (Now Implemented)

After baking, you can assign cache assets in-editor without manual component wiring:

1. Select actors/components in the level.
2. Use one of:
   - `Level Actor Context Menu -> Auto-Apply Generated Pulse Caches`
   - Select one `UPulseDeformCacheAsset` in Content Browser, then run:
     `Apply Pulse RDG Cache To Selected Actors`

Auto-apply lookup uses: `DA_PulseRDGCache_<StaticMeshName>`.

Your current data asset has:

- `RestPosList`
- `RestNormalList`
- `ColorList`
- `UVList_Channel3`

For RDG, bake to textures once in editor.

## Mapping

Use a fixed 2D layout `(x, y)` for each vertex index:

- `x = Index % GridWidth`
- `y = Index / GridWidth`
- `Index = y * GridWidth + x`

For a full grid:

- `GridWidth = 2000`
- `GridHeight = 2000`

## Texture Channels

- `RestPositionTex (RGBA16F or RGBA32F)`:
  - `rgb = RestPosList[Index]` (local space)
  - `a = 1`
- `RestNormalTex (RGBA16F)`:
  - `rgb = normalize(RestNormalList[Index])`
  - `a = 1`
- `MaskTex (R8)`:
  - `r = saturate(ColorList[Index].R)` or your chosen packed mask channel

`UVList_Channel3` is not needed as a separate texture when UV0 already maps the grid and `V` comes from sampled UV in shader.

## Important Import/Storage Flags

- sRGB off
- No mip maps
- Compression disabled or set to HDR/Vector-safe mode
- Nearest filtering when sampling exact vertex texels

## Why This Is Faster

- Removes per-frame CPU array reads/writes.
- Removes CPU mesh position updates.
- Converts work into two linear GPU compute dispatches.
