"""
UE5 MCP Bridge - MCP server for controlling Unreal Engine 5.7

This package provides a Model Context Protocol (MCP) server that bridges
AI agents to the Unreal Engine 5.7 editor via the UltimateControl plugin.
"""

__version__ = "0.1.0"
__author__ = "Rocketship"

from .server import create_server, main
from .client import UltimateControlClient

__all__ = ["create_server", "main", "UltimateControlClient", "__version__"]
