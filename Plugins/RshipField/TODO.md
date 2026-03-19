# RshipField TODO

## Plugin Rename / Decouple from Rship
The field system (compute, atlas, samplers, material function, deformer integration) is functionally independent of rship. The only rship-related requirement is the `RS_` property prefix convention for executor discovery. Consider renaming to `PulseField` and making rship integration an optional layer.

## Light Sampler GPU Optimization
Currently uses a GPU point sample pass + tiny RT readback. Works well but could be further optimized with async readback for zero-stall operation.

## Spline Effectors
Add a spline-based effector type that propagates waves along arbitrary paths. Would sample the spline into a point buffer on CPU, upload to GPU, and evaluate distance-to-spline per voxel. Covers rings (closed spline), vertical lines, and organic paths.

## Ring Wave Effector
Dedicated effector type for cylindrical/orbital wave patterns (center, axis, ring radius, lobe count, speed). Maps directly to the rotunda's orbital displacement pattern. Simpler than spline effectors for this specific use case.

## Kernel Refactor: DG_MullionsDeformer
Refactor the MateoKernel to sample from the shared field system via SampleFieldScalar/SampleFieldVector instead of computing its own waves. Remove orbital/vertical wave parameters from RotundaDeformerComponent — those move into field effectors. Keep structural/shaping params (amplitude, vertical profile, anchor width, crest sharpness, decay, rest threshold).

## RS_Enabled Flag
Add RS_Enabled variable to deformer graph (default 0) and RotundaDeformerComponent (default 1) so only actors with the component get displaced.
