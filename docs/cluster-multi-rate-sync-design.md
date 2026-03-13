# Cluster Multi-Rate Sync Design

## Goal

Support higher local output pacing on some nodes while preserving deterministic cluster state apply.

## Core model

The system now uses two timelines:

1. Sync timeline (shared):
   - Drives authoritative apply frames (`ApplyFrame`) for control/state replication.
   - Must be configured identically across all nodes.
2. Local output timeline (per node):
   - Allows extra local substeps for output/transmit pacing.
   - Does not change authoritative cluster frame numbering.

This keeps deterministic behavior for replicated data while allowing faster local pacing where hardware allows it.

## Implemented in this phase

### RshipExec control-plane timing

- `URshipSettings::ControlSyncRateHz`
  - Controls `URshipSubsystem::TickSubsystems` timer rate.
  - This is the control/apply sync clock.
- `URshipSettings::InboundApplyLeadFrames`
  - Minimum lead before inbound payload apply.
  - Reduces missed-frame risk under network jitter.

### Rship2110 cluster timing

- `URship2110Settings::ClusterSyncRateHz`
  - Shared deterministic frame rate for cluster state/data apply.
- `URship2110Settings::LocalRenderSubsteps`
  - Per-node local output pacing multiplier.
  - Rivermax sender tick runs this many substeps per engine tick.
- `URship2110Settings::MaxSyncCatchupSteps`
  - Bounds catch-up work after stalls to avoid long hitches.

### Sync domains (phase 2)

- `FRship2110ClusterDataMessage::SyncDomainId`
  - Control payloads can target independent deterministic frame timelines.
- Default domain remains aligned with the cluster state timeline.
- Non-default domains maintain independent frame counters and rates.
- Local authority outbound domain defaults to `ActiveSyncDomainId` and can be overridden per payload.

### Performance improvement

- Render-context rebinding now uses an id->render-target map in `RefreshStreamRenderContextBindings` instead of nested stream x context scans.

## Operational rules

1. Keep `ControlSyncRateHz` and `ClusterSyncRateHz` identical across all nodes that need deterministic sync.
2. Increase `InboundApplyLeadFrames` when running larger clusters or higher transport jitter.
3. Use `LocalRenderSubsteps > 1` only on nodes that can sustain extra output work.
4. Do not use different sync rates across nodes in the same deterministic cluster domain.
5. Use separate `syncDomainId` values when you intentionally want independent deterministic frame domains.

## Live tuning commands

- `rship.sync.rate <hz>`
- `rship.sync.lead <frames>`
- `rship.cluster.timing.sync <hz>`
- `rship.cluster.timing.substeps <n>`
- `rship.cluster.timing.catchup <n>`
- `rship.cluster.domain.active <id>`
- `rship.cluster.domain.rate <id> <hz>`

## Known limits

- Local substeps can increase output pacing, but do not create a separate deterministic cluster clock.
- True mixed-sync domains require explicit partitioning by transport/control domain (future phase).

## Suggested rollout

1. Set all nodes to `ControlSyncRateHz=60`, `ClusterSyncRateHz=60`, `LocalRenderSubsteps=1`.
2. Validate deterministic apply and ownership/failover behavior.
3. Increase `LocalRenderSubsteps` on selected high-capacity nodes only.
4. Tune `InboundApplyLeadFrames` (typically 2-4) if control jitter appears.
