# Content Mapping End-to-End Manual (CineCam to Output)

Last updated: 2026-02-13

## Scope

This workflow keeps the existing nDisplay/cluster/frame-boundary stack and adds a `custom-matrix` projection path in Rship content mapping.

- No source-engine rebuild required.
- No movie render queue/post pass required.
- Live runtime path in editor and play.
- Single content-mapping shader/material path (same pipeline used by other projection modes).

## 1) Create Input

1. Open Content Mapping panel.
2. In `Create Mapping`, choose input source (`camera` or `asset-store`).
3. Select/enter target screen.
4. Choose mode:
`Matrix` (custom projection matrix)
5. Click `Create Mapping`.

Result:
- Context + screen surface are created/reused.
- Mapping is created as `surface-projection`.
- `config.projectionType = "custom-matrix"`.
- Identity `config.customProjectionMatrix` is initialized.

## 2) Author Matrix

1. Open mapping form (`Edit` on mapping row).
2. Set mode to `Matrix`.
3. Edit `Custom Projection Matrix (4x4)` values (`m00..m33`).
4. Optional: use `Reset Identity`.
5. Save mapping.

Stored config fields:
- `projectionType: "custom-matrix"`
- `customProjectionMatrix: { m00..m33 }`

Backward-compatible alias accepted at runtime:
- `matrix: { m00..m33 }`

## 3) Runtime Application Path

At runtime, for projection mappings:

1. Projection type resolves to index:
`custom-matrix -> 8`
2. Material params:
- `RshipProjectionType = 8`
- `RshipProjectorRow0..3` loaded from provided 4x4 matrix
3. Fallback shader branch:
- Handles `projIdx == 8` in the same projection pipeline.

If no matrix object is present, projection falls back to existing perspective behavior.

## 4) nDisplay + Cluster + Frame Boundaries

This path does not replace nDisplay.

- Keep nDisplay config, clustering, frame boundaries, and output topology as-is.
- Content mapping computes UVs/projection in the mapped material pass.
- 2110 stream manager can remain transport/output ownership layer.

## 5) Live Editor UX

- Mode selector includes `Matrix`.
- Mapping badges/labels include matrix mode.
- Projection edit and preview continue through existing mapping UX.
- Inline `Save Config` respects `projectionType = custom-matrix` and preserves/creates matrix config.

## 6) JSON Example

```json
{
  "type": "surface-projection",
  "config": {
    "projectionType": "custom-matrix",
    "customProjectionMatrix": {
      "m00": 1.0, "m01": 0.0, "m02": 0.0, "m03": 0.0,
      "m10": 0.0, "m11": 1.0, "m12": 0.0, "m13": 0.0,
      "m20": 0.0, "m21": 0.0, "m22": 1.0, "m23": 0.0,
      "m30": 0.0, "m31": 0.0, "m32": 0.0, "m33": 1.0
    }
  }
}
```

## 7) Operational Notes

- For cylindrical or non-pinhole projection with strict geometry requirements, compute the matrix externally and paste values directly.
- Use one mapping per output surface group where matrix behavior must be deterministic and isolated.
