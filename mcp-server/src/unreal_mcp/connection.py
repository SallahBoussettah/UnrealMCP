"""TCP connection manager for communicating with the UE5 plugin."""

import asyncio
import json
import struct
import uuid
import logging

logger = logging.getLogger(__name__)

# 4-byte big-endian length prefix
HEADER_SIZE = 4
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 55555
CONNECT_TIMEOUT = 5.0
READ_TIMEOUT = 30.0


class UnrealConnection:
    """Manages TCP connection to the UnrealMCP plugin running inside UE5 Editor."""

    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT):
        self.host = host
        self.port = port
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._lock = asyncio.Lock()

    @property
    def connected(self) -> bool:
        return self._writer is not None and not self._writer.is_closing()

    async def connect(self) -> None:
        """Establish TCP connection to the UE5 plugin."""
        if self.connected:
            return
        try:
            self._reader, self._writer = await asyncio.wait_for(
                asyncio.open_connection(self.host, self.port),
                timeout=CONNECT_TIMEOUT,
            )
            logger.info(f"Connected to UE5 at {self.host}:{self.port}")
        except (ConnectionRefusedError, OSError, asyncio.TimeoutError) as e:
            raise ConnectionError(
                f"Cannot connect to UE5 plugin at {self.host}:{self.port}. "
                f"Make sure Unreal Editor is running with the UnrealMCP plugin enabled. "
                f"Error: {e}"
            ) from e

    async def disconnect(self) -> None:
        """Close the TCP connection."""
        if self._writer and not self._writer.is_closing():
            self._writer.close()
            try:
                await self._writer.wait_closed()
            except Exception:
                pass
        self._reader = None
        self._writer = None
        logger.info("Disconnected from UE5")

    async def send_command(self, command: str, params: dict | None = None) -> dict:
        """Send a command to UE5 and wait for the response.

        Args:
            command: The command name (e.g., 'create_blueprint')
            params: Command parameters

        Returns:
            Response dict with 'success', 'data', and optionally 'error' keys
        """
        async with self._lock:
            if not self.connected:
                await self.connect()

            request_id = str(uuid.uuid4())[:8]
            message = {
                "id": request_id,
                "command": command,
                "params": params or {},
            }

            try:
                # Serialize and send with length prefix
                payload = json.dumps(message).encode("utf-8")
                header = struct.pack(">I", len(payload))
                self._writer.write(header + payload)
                await self._writer.drain()

                # Read response with length prefix
                raw_header = await asyncio.wait_for(
                    self._reader.readexactly(HEADER_SIZE),
                    timeout=READ_TIMEOUT,
                )
                response_length = struct.unpack(">I", raw_header)[0]
                raw_response = await asyncio.wait_for(
                    self._reader.readexactly(response_length),
                    timeout=READ_TIMEOUT,
                )
                response = json.loads(raw_response.decode("utf-8"))

                if response.get("id") != request_id:
                    logger.warning(
                        f"Response ID mismatch: expected {request_id}, got {response.get('id')}"
                    )

                return response

            except (ConnectionResetError, BrokenPipeError, asyncio.IncompleteReadError) as e:
                logger.error(f"Connection lost: {e}")
                await self.disconnect()
                raise ConnectionError(
                    "Lost connection to UE5. The editor may have closed or the plugin was disabled."
                ) from e
            except asyncio.TimeoutError as e:
                logger.error("Command timed out")
                raise TimeoutError(
                    f"Command '{command}' timed out after {READ_TIMEOUT}s. "
                    "The operation may be taking too long in UE5."
                ) from e


# Global connection instance
_connection: UnrealConnection | None = None


def get_connection() -> UnrealConnection:
    """Get or create the global connection instance."""
    global _connection
    if _connection is None:
        _connection = UnrealConnection()
    return _connection


async def send_command(command: str, params: dict | None = None) -> dict:
    """Convenience function to send a command using the global connection."""
    conn = get_connection()
    return await conn.send_command(command, params)
