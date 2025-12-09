"""
HTTP client for communicating with the UltimateControl plugin's JSON-RPC API.
"""

import asyncio
import json
from typing import Any, Optional
from uuid import uuid4

import httpx


class UltimateControlError(Exception):
    """Exception raised when the UltimateControl API returns an error."""

    def __init__(self, code: int, message: str, data: Any = None):
        self.code = code
        self.message = message
        self.data = data
        super().__init__(f"[{code}] {message}")


class UltimateControlClient:
    """
    Client for the UltimateControl HTTP JSON-RPC API.

    This client communicates with the Unreal Engine 5.7 editor via the
    UltimateControl plugin's HTTP server.

    Example:
        ```python
        async with UltimateControlClient() as client:
            info = await client.call("system.getInfo")
            print(info)
        ```
    """

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 7777,
        token: Optional[str] = None,
        timeout: float = 30.0,
    ):
        """
        Initialize the client.

        Args:
            host: Hostname of the UltimateControl server
            port: Port number of the UltimateControl server
            token: Authentication token (X-Ultimate-Control-Token header)
            timeout: Request timeout in seconds
        """
        self.base_url = f"http://{host}:{port}/rpc"
        self.token = token
        self.timeout = timeout
        self._client: Optional[httpx.AsyncClient] = None

    async def __aenter__(self) -> "UltimateControlClient":
        """Enter async context manager."""
        self._client = httpx.AsyncClient(timeout=self.timeout)
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """Exit async context manager."""
        if self._client:
            await self._client.aclose()
            self._client = None

    def _get_headers(self) -> dict[str, str]:
        """Get HTTP headers for requests."""
        headers = {"Content-Type": "application/json"}
        if self.token:
            headers["X-Ultimate-Control-Token"] = self.token
        return headers

    async def call(
        self,
        method: str,
        params: Optional[dict[str, Any]] = None,
    ) -> Any:
        """
        Call a JSON-RPC method.

        Args:
            method: The method name (e.g., "asset.list")
            params: Optional parameters for the method

        Returns:
            The result from the JSON-RPC response

        Raises:
            UltimateControlError: If the API returns an error
            httpx.HTTPError: If there's a network error
        """
        if self._client is None:
            # Create a temporary client for one-off calls
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                return await self._do_call(client, method, params)
        return await self._do_call(self._client, method, params)

    async def _do_call(
        self,
        client: httpx.AsyncClient,
        method: str,
        params: Optional[dict[str, Any]] = None,
    ) -> Any:
        """Execute the actual HTTP request."""
        request_id = str(uuid4())

        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "id": request_id,
        }
        if params:
            payload["params"] = params

        response = await client.post(
            self.base_url,
            json=payload,
            headers=self._get_headers(),
        )
        response.raise_for_status()

        result = response.json()

        if "error" in result:
            error = result["error"]
            raise UltimateControlError(
                code=error.get("code", -1),
                message=error.get("message", "Unknown error"),
                data=error.get("data"),
            )

        return result.get("result")

    # Convenience methods for common operations

    async def get_info(self) -> dict:
        """Get server information."""
        return await self.call("system.getInfo")

    async def list_methods(self) -> list[dict]:
        """List all available methods."""
        return await self.call("system.listMethods")

    async def echo(self, data: dict) -> dict:
        """Echo back data (for testing)."""
        return await self.call("system.echo", data)

    # Asset methods
    async def list_assets(
        self,
        path: str = "/Game",
        class_name: Optional[str] = None,
        recursive: bool = True,
        limit: int = 1000,
    ) -> dict:
        """List assets in a path."""
        params = {"path": path, "recursive": recursive, "limit": limit}
        if class_name:
            params["class"] = class_name
        return await self.call("asset.list", params)

    async def get_asset(self, path: str) -> dict:
        """Get detailed asset information."""
        return await self.call("asset.get", {"path": path})

    async def search_assets(
        self,
        query: str,
        class_name: Optional[str] = None,
        limit: int = 100,
    ) -> dict:
        """Search for assets by name."""
        params = {"query": query, "limit": limit}
        if class_name:
            params["class"] = class_name
        return await self.call("asset.search", params)

    # Level/Actor methods
    async def get_current_level(self) -> dict:
        """Get current level information."""
        return await self.call("level.getCurrent")

    async def open_level(self, path: str, prompt_save: bool = True) -> dict:
        """Open a level."""
        return await self.call("level.open", {"path": path, "promptSave": prompt_save})

    async def list_actors(
        self,
        class_filter: Optional[str] = None,
        tag_filter: Optional[str] = None,
        limit: int = 1000,
    ) -> dict:
        """List actors in the current level."""
        params = {"limit": limit}
        if class_filter:
            params["class"] = class_filter
        if tag_filter:
            params["tag"] = tag_filter
        return await self.call("actor.list", params)

    async def get_actor(self, actor: str) -> dict:
        """Get detailed actor information."""
        return await self.call("actor.get", {"actor": actor})

    async def spawn_actor(
        self,
        class_name: str,
        name: Optional[str] = None,
        location: Optional[dict] = None,
        rotation: Optional[dict] = None,
    ) -> dict:
        """Spawn a new actor."""
        params = {"class": class_name}
        if name:
            params["name"] = name
        if location:
            params["location"] = location
        if rotation:
            params["rotation"] = rotation
        return await self.call("actor.spawn", params)

    async def destroy_actor(self, actor: str) -> dict:
        """Destroy an actor."""
        return await self.call("actor.destroy", {"actor": actor})

    async def set_actor_transform(
        self,
        actor: str,
        location: Optional[dict] = None,
        rotation: Optional[dict] = None,
        scale: Optional[dict] = None,
    ) -> dict:
        """Set an actor's transform."""
        params = {"actor": actor}
        if location:
            params["location"] = location
        if rotation:
            params["rotation"] = rotation
        if scale:
            params["scale"] = scale
        return await self.call("actor.setTransform", params)

    # Blueprint methods
    async def list_blueprints(self, path: str = "/Game", limit: int = 500) -> dict:
        """List blueprints."""
        return await self.call("blueprint.list", {"path": path, "limit": limit})

    async def get_blueprint(self, path: str) -> dict:
        """Get blueprint details."""
        return await self.call("blueprint.get", {"path": path})

    async def compile_blueprint(self, path: str) -> dict:
        """Compile a blueprint."""
        return await self.call("blueprint.compile", {"path": path})

    # PIE methods
    async def play(self, mode: str = "SelectedViewport") -> dict:
        """Start Play In Editor."""
        return await self.call("pie.play", {"mode": mode})

    async def stop(self) -> dict:
        """Stop Play In Editor."""
        return await self.call("pie.stop")

    async def get_pie_state(self) -> dict:
        """Get PIE state."""
        return await self.call("pie.getState")

    # Console methods
    async def execute_console(self, command: str) -> dict:
        """Execute a console command."""
        return await self.call("console.execute", {"command": command})

    async def get_console_variable(self, name: str) -> dict:
        """Get a console variable."""
        return await self.call("console.getVariable", {"name": name})

    async def set_console_variable(self, name: str, value: str) -> dict:
        """Set a console variable."""
        return await self.call("console.setVariable", {"name": name, "value": value})
