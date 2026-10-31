"""Tiny MCP (streamable HTTP) client for Unreal Engine 5.8's built-in MCP server.

usage:
  python mcp.py wait                      # wait until the server answers
  python mcp.py list                      # tools/list
  python mcp.py call <tool> '<json args>' # tools/call
"""
import json
import os
import tempfile
import sys
import time
import urllib.request
import urllib.error

URL = "http://127.0.0.1:8000/mcp"
SESSION_FILE = os.path.join(tempfile.gettempdir(), "osf_ue_mcp.session")


def _post(payload, session=None, timeout=600):
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(URL, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json, text/event-stream")
    if session:
        req.add_header("Mcp-Session-Id", session)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        sid = resp.headers.get("Mcp-Session-Id")
        body = resp.read().decode("utf-8", errors="replace")
    if body.lstrip().startswith("event:") or body.lstrip().startswith("data:"):
        msgs = [json.loads(line[5:]) for line in body.splitlines() if line.startswith("data:") and line[5:].strip()]
        body_json = msgs[-1] if msgs else None
    else:
        body_json = json.loads(body) if body.strip() else None
    return sid, body_json


def session():
    try:
        with open(SESSION_FILE) as f:
            sid = f.read().strip()
        if sid:
            return sid
    except OSError:
        pass
    sid, res = _post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
        "protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "osf-mcp-client", "version": "0.1"}}})
    _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid)
    with open(SESSION_FILE, "w") as f:
        f.write(sid or "")
    return sid


def rpc(method, params=None):
    for attempt in range(2):
        sid = session()
        try:
            _, res = _post({"jsonrpc": "2.0", "id": int(time.time() * 1000) % 100000000, "method": method, "params": params or {}}, sid)
            return res
        except urllib.error.HTTPError as e:
            if e.code in (400, 404) and attempt == 0:
                open(SESSION_FILE, "w").close()
                continue
            raise


def main():
    cmd = sys.argv[1]
    if cmd == "wait":
        deadline = time.time() + float(sys.argv[2] if len(sys.argv) > 2 else 600)
        while time.time() < deadline:
            try:
                open(SESSION_FILE, "w").close()
                session()
                print("ready")
                return
            except Exception:
                time.sleep(3)
        print("timeout")
        sys.exit(1)
    if cmd == "list":
        res = rpc("tools/list")
        for tool in res.get("result", {}).get("tools", []):
            print(tool["name"], "-", (tool.get("description") or "")[:150].replace("\n", " "))
        return
    if cmd == "call":
        args = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        res = rpc("tools/call", {"name": sys.argv[2], "arguments": args})
        out = json.dumps(res, indent=1, ensure_ascii=False)
        limit = int(sys.argv[4]) if len(sys.argv) > 4 else 6000
        print(out[:limit])
        return
    print(__doc__)


if __name__ == "__main__":
    main()
