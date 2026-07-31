"""Minimal Blockbench MCP HTTP client (when Cursor MCP bridge is down)."""
from __future__ import annotations

import json
import sys
import urllib.request
from typing import Any

URL = "http://127.0.0.1:3000/bb-mcp"


class BlockbenchMcp:
    def __init__(self, url: str = URL) -> None:
        self.url = url
        self.session_id: str | None = None
        self._id = 0

    def _rpc(self, method: str, params: dict[str, Any] | None = None, *, notify: bool = False) -> Any:
        self._id += 1
        payload: dict[str, Any] = {"jsonrpc": "2.0", "method": method}
        if not notify:
            payload["id"] = self._id
        if params is not None:
            payload["params"] = params
        data = json.dumps(payload).encode()
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = urllib.request.Request(self.url, data=data, headers=headers)
        with urllib.request.urlopen(req, timeout=180) as resp:
            sid = resp.headers.get("Mcp-Session-Id")
            if sid:
                self.session_id = sid
            body = resp.read().decode("utf-8", "replace")
        if notify:
            return None
        if body.startswith("event:") or body.lstrip().startswith("data:"):
            lines = [ln[5:].strip() for ln in body.splitlines() if ln.startswith("data:")]
            body = lines[-1] if lines else body
        parsed = json.loads(body)
        if "error" in parsed:
            raise RuntimeError(json.dumps(parsed["error"], indent=2))
        return parsed.get("result")

    def connect(self) -> dict[str, Any]:
        result = self._rpc(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "goodplayer-rerig", "version": "0.1"},
            },
        )
        self._rpc("notifications/initialized", notify=True)
        return result

    def call_tool(self, name: str, arguments: dict[str, Any] | None = None) -> Any:
        result = self._rpc("tools/call", {"name": name, "arguments": arguments or {}})
        # MCP tool results are usually {content:[{type:text,text:...}], isError?}
        if isinstance(result, dict) and result.get("isError"):
            raise RuntimeError(json.dumps(result, indent=2)[:4000])
        content = result.get("content") if isinstance(result, dict) else None
        if isinstance(content, list) and content:
            texts = [c.get("text", "") for c in content if isinstance(c, dict)]
            joined = "\n".join(texts)
            try:
                return json.loads(joined)
            except Exception:
                return joined
        return result

    def eval(self, code: str) -> Any:
        return self.call_tool("risky_eval", {"code": code})


def main() -> None:
    code = sys.argv[1] if len(sys.argv) > 1 else "Project.name"
    client = BlockbenchMcp()
    info = client.connect()
    print("connected", info.get("serverInfo"))
    out = client.eval(code)
    if isinstance(out, (dict, list)):
        print(json.dumps(out, indent=2))
    else:
        print(out)


if __name__ == "__main__":
    main()
