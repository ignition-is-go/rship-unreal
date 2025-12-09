#!/usr/bin/env python3
"""
Test script for UltimateControl JSON-RPC API
Run this with UE5 Editor open and the plugin loaded.
"""

import json
import requests
from typing import Any

BASE_URL = "http://localhost:7777/rpc"

def call_rpc(method: str, params: dict = None, id: int = 1) -> dict:
    """Make a JSON-RPC 2.0 call to the UltimateControl server."""
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params or {},
        "id": id
    }
    try:
        response = requests.post(BASE_URL, json=payload, timeout=10)
        return response.json()
    except requests.exceptions.ConnectionError:
        return {"error": {"code": -1, "message": "Connection failed - is UE5 running?"}}
    except Exception as e:
        return {"error": {"code": -1, "message": str(e)}}

def test_method(method: str, params: dict = None, description: str = "") -> bool:
    """Test a single method and print result."""
    print(f"\n{'='*60}")
    print(f"Testing: {method}")
    if description:
        print(f"  {description}")
    print(f"Params: {json.dumps(params or {})}")

    result = call_rpc(method, params)

    if "error" in result:
        print(f"❌ ERROR: {result['error']}")
        return False
    else:
        print(f"✅ SUCCESS")
        # Pretty print result (truncated)
        result_str = json.dumps(result.get("result"), indent=2)
        if len(result_str) > 500:
            result_str = result_str[:500] + "\n... (truncated)"
        print(f"Result: {result_str}")
        return True

def main():
    print("=" * 60)
    print("UltimateControl API Test Suite")
    print("=" * 60)

    passed = 0
    failed = 0

    # Test categories
    tests = [
        # System
        ("system.listMethods", {}, "List all available methods"),
        ("system.getVersion", {}, "Get plugin version"),
        ("system.getStatus", {}, "Get server status"),

        # Asset
        ("asset.list", {"path": "/Game"}, "List assets in /Game"),
        ("asset.exists", {"path": "/Game"}, "Check if /Game exists"),

        # Level
        ("level.getCurrent", {}, "Get current level info"),
        ("level.listActors", {}, "List actors in level"),

        # Viewport
        ("viewport.list", {}, "List all viewports"),
        ("viewport.getCamera", {"viewportIndex": 0}, "Get viewport 0 camera"),

        # Blueprint
        ("blueprint.list", {"path": "/Game"}, "List blueprints"),

        # Console
        ("console.getHistory", {}, "Get console history"),
        ("console.getVariables", {"filter": "r."}, "Get render variables"),

        # Project
        ("project.getInfo", {}, "Get project information"),
        ("project.getConfig", {"section": "Engine"}, "Get engine config"),

        # Editor
        ("editor.listWindows", {}, "List editor windows"),
        ("editor.getEditorMode", {}, "Get current editor mode"),
        ("editor.getTransformMode", {}, "Get transform mode"),
        ("editor.getGridSettings", {}, "Get grid settings"),

        # Transaction
        ("transaction.getCurrent", {}, "Get current transaction"),
        ("transaction.getHistory", {}, "Get transaction history"),

        # Outliner
        ("outliner.getSelection", {}, "Get selected actors"),
        ("outliner.getHierarchy", {}, "Get actor hierarchy"),

        # Session
        ("session.getInfo", {}, "Get session info"),
        ("session.getConnectedClients", {}, "Get connected clients"),

        # Automation
        ("automation.listTests", {}, "List automation tests"),

        # Profiling
        ("profiling.getStats", {}, "Get profiling stats"),
        ("profiling.getFrameStats", {}, "Get frame statistics"),

        # Render
        ("render.getSettings", {}, "Get render settings"),
        ("render.getStats", {}, "Get render statistics"),

        # Source Control
        ("sourcecontrol.getStatus", {}, "Get source control status"),
        ("sourcecontrol.getProvider", {}, "Get SC provider info"),

        # Live Coding
        ("livecoding.getStatus", {}, "Get live coding status"),
    ]

    for method, params, description in tests:
        if test_method(method, params, description):
            passed += 1
        else:
            failed += 1

    print("\n" + "=" * 60)
    print(f"TEST RESULTS: {passed} passed, {failed} failed")
    print("=" * 60)

    # Count total methods
    result = call_rpc("system.listMethods")
    if "result" in result and "methods" in result["result"]:
        total = len(result["result"]["methods"])
        print(f"\nTotal registered methods: {total}")

    return failed == 0

if __name__ == "__main__":
    import sys
    success = main()
    sys.exit(0 if success else 1)
