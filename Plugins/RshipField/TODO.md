# RshipField TODO

## Plugin Rename / Decouple from Rship
The field system (compute, atlas, samplers, material function, deformer integration) is functionally independent of rship. The only rship-related requirement is the `RS_` property prefix convention for executor discovery. Consider renaming to `PulseField` and making rship integration an optional layer.

## Light Sampler GPU Optimization
Currently uses a GPU point sample pass + tiny RT readback. Works well but could be further optimized with async readback for zero-stall operation.

## Wave Mode Architecture
Wave effectors support two physically-correct modes:

### Standing (`sin(kx) · cos(ωt)`)
Fixed spatial pattern, amplitude breathes in place. Nodes at fixed radii that never move. Stateless — pure function of position and time.

### Traveling (`sin(kx - ωt)`)
Wavefronts expand outward from source with Gaussian envelope. Stateful — CPU manages a wavefront ring buffer (max 16 per effector), GPU evaluates per-wavefront envelope. Non-dispersive (single wave speed).

- **RadiusCm** sets max travel distance. Wavefronts are culled on CPU when they exceed it.
- **FalloffExponent** controls amplitude decay over distance — same `pow(1 - d/r, exp)` curve used by standing waves, applied uniformly to both modes.
- **EnvelopeWidthCm** sets the Gaussian spatial width of each wavefront pulse.
- **bAutoEmit / RepeatHz** controls continuous wavefront emission. Higher RepeatHz approaches continuous flow.
- **EmitWavefront(index)** — blueprint-callable manual trigger for event-driven ripples.

### Dispersion Lock (`v = f · λ`)
Frequency, wavelength, and wave speed are related by the dispersion relation `v = f · λ`. Only two are independent. The `DispersionLock` enum lets the artist choose which parameter to derive:

| DispersionLock | Artist controls | Derived | Mental model |
|---|---|---|---|
| **Derive Frequency** (default) | Speed + Wavelength | `f = v/λ` | "Ripples expand at this speed with this ring spacing" |
| **Derive Speed** | Frequency + Wavelength | `v = f·λ` | "This many oscillations per second at this spacing" |
| **Derive Wavelength** | Speed + Frequency | `λ = v/f` | "Ripples expand at this speed, oscillating this fast" |

The dispersion relation is resolved on CPU before GPU upload. The shader always receives consistent wavelength and speed values.

### Multi-Transport
Currently a single transport clock (BPM + BeatPhase) drives all sync groups via tempo multipliers. A D3-style multi-transport system (independent clocks per group, each with their own BPM/phase/play state) could enable polyrhythmic and cross-tempo relationships. Worth investigating if single-transport proves limiting.

### Future considerations
- Dispersion (frequency-dependent wave speed) — would require per-frequency evaluation, significant cost increase.
- Wavefront shape beyond spherical (planar, cylindrical).

## Spline Effectors
Add a spline-based effector type that propagates waves along arbitrary paths. Would sample the spline into a point buffer on CPU, upload to GPU, and evaluate distance-to-spline per voxel. Covers rings (closed spline), vertical lines, and organic paths.

## Ring Wave Effector
Dedicated effector type for cylindrical/orbital wave patterns (center, axis, ring radius, lobe count, speed). Maps directly to the rotunda's orbital displacement pattern. Simpler than spline effectors for this specific use case.

## Kernel Refactor: DG_MullionsDeformer
Refactor the MateoKernel to sample from the shared field system via SampleFieldScalar/SampleFieldVector instead of computing its own waves. Remove orbital/vertical wave parameters from RotundaDeformerComponent — those move into field effectors. Keep structural/shaping params (amplitude, vertical profile, anchor width, crest sharpness, decay, rest threshold).

## RS_Enabled Flag
Add RS_Enabled variable to deformer graph (default 0) and RotundaDeformerComponent (default 1) so only actors with the component get displaced.
