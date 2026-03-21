# 50k Target Registration Scale Plan

## Summary

- Goal: support `50,000` targets registering simultaneously without dropped topology, unbounded memory growth, or quadratic work on the Unreal game thread.
- Scope is end-to-end: Unreal plugin registration, websocket sender behavior, wire protocol, and server bulk ingest.
- Success criteria:
  - exactly one final registration snapshot per item id per sync cycle
  - bounded sender memory via chunk/window backpressure
  - zero dropped topology items in the 50k benchmark
  - explicit diagnostics for build cost, queue depth, socket backlog, and ack lag

## Implemented In This Repo

- Added topology build scoping in `URshipSubsystem`.
  - `BeginTopologyBuild()`
  - `EndTopologyBuild()`
  - `EnqueueTopologySnapshot(...)`
- Replaced append-only registration batching with keyed dedupe maps for:
  - `Target`
  - `Action`
  - `Emitter`
  - `TargetStatus`
- Changed actor/controller registration to build targets inside topology scopes so constructor and mutation publishes collapse into one flush per target.
- Removed the deferred controller re-register for actors that already have a `URshipActorRegistrationComponent`.
- Forced registration traffic to enqueue instead of using the connected direct-send fast path in `QueueMessage(...)`.
- Added a bulk topology sender in the plugin with:
  - byte chunking
  - inflight chunk windows
  - websocket buffered-byte backpressure
  - `ws:m:topology-chunk`
  - `ws:m:topology-ack`
  - legacy `ws:m:event-batch` fallback when ack is not observed
- Added topology diagnostics for:
  - topology queue depth
  - inflight chunk count
  - ack lag
  - websocket buffered bytes
- Added project settings for:
  - `bEnableTopologyChunkProtocol`
  - `MaxTopologyChunkBytes`
  - `MaxTopologyInflightChunks`
  - `MaxTopologyBufferedBytes`
  - `BulkTopologyThresholdItems`

## Still Required Outside This Repo

- Server bulk-ingest implementation for `ws:m:topology-chunk` and `ws:m:topology-ack`.
- Same-sync dedupe and parent-before-child apply in the main rship server repo.
- Reconnect resume semantics validated against the server bulk-ingest path.
- Synthetic 50k and mixed-shape benchmark scenarios executed in Unreal and against a bulk-aware server.

## Validation Plan

- Run a synthetic Unreal benchmark for `50,000` simple targets with `2` actions and `1` emitter each.
- Run mixed-shape benchmarks for material-like, niagara-like, and rig-like topologies.
- Verify:
  - one final snapshot per id in the emitted topology stream
  - bounded `pending_socket_bytes`
  - bounded topology queue depth
  - bounded inflight chunk count
  - stable ack lag under load

## Assumptions

- Server-side bulk ingest lands in the main rship server repo.
- The Unreal client remains backward compatible by falling back to legacy event batches if topology chunk acks are not observed.
- `bEnableTopologyChunkProtocol` currently defaults to `false` so the client uses the legacy event-batch path until the server supports chunk acks.
- Normal low-latency pulse traffic continues using the direct send path; topology registration and replay use the queued topology path.
