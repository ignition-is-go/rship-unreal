# Rship Cluster Authoritative Ingest Manual

## Goal

Guarantee deterministic state delivery and application across an nDisplay cluster by:

- Ingesting live websocket control data on exactly one authoritative node.
- Applying inbound state in a frame-latched phase.
- Replicating authoritative state/events to other nodes for synchronized apply.

## Runtime path (end-to-end)

1. `URshipSubsystem::OnWebSocketMessage` receives inbound websocket payload.
2. Payload is node-target filtered first:
- If payload contains `targetNodeId`, `targetNodeIds`, or `targetIds`, only matching nodes enqueue it.
- Non-matching messages are dropped at ingress (`target-filtered` diagnostics counter).
3. Payload is authority-gated:
- If local node is authoritative, message is queued.
- If local node is non-authoritative, live payload is dropped unless it arrives as replicated/targeted cluster payload.
3. Authority node emits `OnAuthoritativeInboundQueued(payload, applyFrame)` from `URshipSubsystem`.
4. `URship2110Subsystem` listens for that callback and emits `OnClusterDataOutbound(FRship2110ClusterDataMessage)`.
5. Cluster transport relays `FRship2110ClusterDataMessage` to non-authority nodes, which call `ReceiveClusterDataMessage(...)`.
6. `URship2110Subsystem` frame-latches received data and forwards to `URshipSubsystem::EnqueueReplicatedInboundMessage(payload, applyFrame)`.
7. `URshipSubsystem::TickSubsystems` applies both local authority and replicated payloads in deterministic sequence/frame order.

## Node authority configuration

Authority policy is controlled by cvars/launch config:

- `r.Rship.Inbound.AuthorityOnly` (`1` default): enforce single-node live ingest.
- `r.Rship.Inbound.MaxMessagesPerTick` (`256` default): cap deterministic apply workload per frame.
- `-dc_node=<NodeId>`: local node id (nDisplay launch arg).
- `-rship_authority_node=<NodeId>` or env `RSHIP_AUTHORITY_NODE`: authoritative ingest node id.

If no node id is provided, local node defaults to the configured authority node.

## Diagnostics for UX/editor tooling

Use subsystem diagnostics in UI:

- `GetInboundQueueLength()`
- `GetInboundDroppedMessages()`
- `GetInboundTargetFilteredMessages()`
- `GetInboundAverageApplyLatencyMs()`
- `IsAuthoritativeIngestNode()`

Recommended UX indicators:

- Authority badge per node.
- Inbound backlog meter.
- Average apply latency indicator.
- Target-filtered counter (non-local traffic pruned at ingress).
- Dropped-live-ingest counter (should remain `0` on authority node).
- Failover/ownership control actions:
- Promote local authority.
- Toggle failover enable.
- Toggle strict node ownership.

## Cluster sync contract

To keep render and engine state synchronized:

- Treat authoritative node as single writer for external/live control.
- Replicate authoritative payloads/events to other nodes with frame/epoch metadata using:
- `OnClusterDataOutbound(...)` on authority side.
- `ReceiveClusterDataMessage(...)` on receivers.
- Optionally set `FRship2110ClusterDataMessage::TargetNodeId` for node-specific delivery.
- Apply on all nodes in the same frame-latched phase.
- Avoid manager mutation from async callbacks outside the apply phase.

## Operational checklist

- Ensure only one authoritative ingest node is configured.
- Verify non-authority nodes report ingest disabled.
- Verify inbound backlog does not grow unbounded under expected load.
- Verify replicated payload delivery exists for non-authority nodes.
- Verify state-changing handlers do not execute outside `TickSubsystems` apply.
