# UltimateControl Agent Experience Audit

Date: 2026-02-12

## Scope

Audit objective:
- evaluate how the UltimateControl MCP product actually behaves for multi-agent teams
- close key end-to-end gaps for managing complex Unreal projects

## Key Findings (Before Changes)

1. MCP mapping drift
- Several MCP tools mapped to non-existent JSON-RPC methods (tool call failures at runtime).

2. No built-in agent coordination plane
- Existing `session.*` APIs are tied to Concert/Multi-User and include stub/placeholder behavior when not connected.
- There was no always-on control plane for agent identity, leasing, and task assignment.

3. JSON-RPC robustness gaps
- Server assumed `params` was always an object and could fail hard on malformed calls.
- Batch JSON-RPC requests were not actually implemented.

4. Operator visibility gaps
- No single health/readiness endpoint dedicated to orchestration loops.

## Changes Implemented

### 1) New `agent.*` orchestration APIs (real, in-plugin)

Added a new `FUltimateControlAgentHandler` with methods:

- lifecycle:
  - `agent.register`
  - `agent.heartbeat`
  - `agent.list`
  - `agent.unregister`
- resource coordination:
  - `agent.claimResource`
  - `agent.releaseResource`
  - `agent.listClaims`
- task queue:
  - `agent.createTask`
  - `agent.assignTask`
  - `agent.takeTask`
  - `agent.updateTask`
  - `agent.listTasks`
- monitoring:
  - `agent.getDashboard`

Behavior highlights:
- lease-based resource claims with expiry
- stale/online agent tracking
- shared prioritized task queue
- assignee/task state synchronization
- persisted state in `Saved/UltimateControl/AgentOrchestrationState.json`

### 2) JSON-RPC core hardening

- implemented batch request support for JSON-RPC arrays
- added safe `params` handling with explicit `InvalidParams` error when non-object
- added `system.health` endpoint for readiness checks
- extended `system.getInfo` with:
  - `supportsBatchRequests`
  - `supportsAgentOrchestration`

### 3) MCP usability fixes

- repaired tool-to-method mapping drift (0 missing mappings after patch)
- added tool argument normalization for compatibility (e.g., renamed fields, method selection)
- added generic passthrough tool:
  - `ue5_rpc_call` for any raw JSON-RPC method
- added orchestration tools:
  - `ue5_agent_*`
  - `ue5_system_health`

## Resulting End-to-End Team Flow

1. Coordinator registers workers (`ue5_agent_register`).
2. Coordinator creates tasks (`ue5_agent_create_task`).
3. Workers heartbeat and pull tasks (`ue5_agent_heartbeat`, `ue5_agent_take_task`).
4. Workers claim resources before edits (`ue5_agent_claim_resource`).
5. Workers execute UE methods (assets/actors/lights/etc.).
6. Workers update task outcomes and release leases (`ue5_agent_update_task`, `ue5_agent_release_resource`).
7. Coordinator monitors global state (`ue5_agent_dashboard`, `ue5_system_health`).

## Remaining Known Risks

- Some domain handlers still return “not fully implemented” for advanced Unreal features (for example some Sequencer binding operations).
- Agent orchestration state uses local JSON persistence and is not multi-host transactional (no distributed consensus/locking).
- Concert/Multi-User server administrative operations still depend on external server configuration and privileges.
