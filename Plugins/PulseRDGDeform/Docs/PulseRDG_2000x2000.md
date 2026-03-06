# Pulse RDG Deform (UE5.7) - 2000x2000 Setup

This plugin skeleton moves deformation to GPU via RDG compute and keeps runtime CPU work near zero.

## 1. What It Does

- Reads cached per-vertex data from textures:
  - `RestPositionTex` (local-space rest position)
  - `RestNormalTex` (local-space rest normal)
  - `MaskTex` (0..1 influence)
- Dispatches compute each update:
  - pass 1: computes deformed position
  - pass 2: recomputes normals from deformed position
- Writes outputs to render targets:
  - `DeformedPositionRT`
  - `DeformedNormalRT`
- Material samples these RTs for WPO + shading.

## 2. Required Asset Formats

For `2000x2000`, all cache and output textures must be exactly `2000 x 2000`.

- `RestPositionTex`:
  - linear (sRGB off)
  - high precision float (RGBA16F or RGBA32F)
  - no mips
- `RestNormalTex`:
  - linear (sRGB off)
  - high precision float (RGBA16F)
  - no mips
- `MaskTex`:
  - linear (sRGB off)
  - R8 or equivalent single-channel
  - no mips
- `DeformedPositionRT` / `DeformedNormalRT`:
  - `RTF_RGBA16f`
  - `bCanCreateUAV = true`
  - auto-generate mips disabled

## 3. Material Wiring

On the target mesh material:

- Add texture params:
  - `RestPositionTex`
  - `DeformedPositionTex`
  - `DeformedNormalTex`
- Compute local offset:
  - `OffsetLocal = DeformedPositionTex(UV0) - RestPositionTex(UV0)`
- Feed `OffsetLocal` to `World Position Offset` (convert local to world if your graph uses world space).
- For accurate lighting:
  - disable tangent-space normal in the material if feeding world-space normal
  - feed normal from `DeformedNormalTex(UV0)` with proper space transform
- For accurate shadows:
  - keep deformation active in shadow passes
  - if ray tracing is enabled, enable WPO evaluation for the RT path

## 4. Component Usage

Attach `UPulseRDGDeformComponent` to an actor and set:

- `DeformCache` (`UPulseDeformCacheAsset`)
- `DeformedPositionRT`
- `DeformedNormalRT`
- `TargetMeshComponent`
- Animation params: `Speed`, `Amplitude`, `HeightFrequency`, `CenterWidth`
- Performance params: `UpdateRateHz`, `bUseAsyncCompute`

The component dispatches RDG compute at `UpdateRateHz` and binds texture params on dynamic material instances.

## 5. Performance Notes for 2000x2000

- Vertex count: `4,000,000`, triangles: `~8,000,000`.
- Compute cost is usually not the bottleneck at this scale; raster + shadow cost is.
- Start with:
  - `UpdateRateHz = 30` for heavy scenes, then push to `60` if headroom allows.
  - `bUseAsyncCompute = true` (verify in profiler).
- Use GPU profiling (`stat gpu`, RDG profiler) to confirm overlap and identify shadow bottlenecks.
- Keep texture formats as low as possible while preserving quality.

## 6. Limitations

- This skeleton assumes UV-to-grid mapping is coherent for neighbor-based normal reconstruction.
- Non-grid meshes need a different adjacency strategy (index-buffer-based normal pass).
- Runtime collision is intentionally not supported in this path.
