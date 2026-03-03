"""Show exact positions after arranging."""
import socket, json, struct, uuid

ASSET = "/Game/Blueprints/Player/BP_PlayerCharacter"
GRAPH = "EventGraph"

def send_cmd(cmd, params):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", 55555))
    req_id = str(uuid.uuid4())[:8]
    msg = json.dumps({"id": req_id, "command": cmd, "params": params}).encode("utf-8")
    sock.sendall(struct.pack(">I", len(msg)) + msg)
    raw_h = b""
    while len(raw_h) < 4:
        raw_h += sock.recv(4 - len(raw_h))
    resp_len = struct.unpack(">I", raw_h)[0]
    raw_r = b""
    while len(raw_r) < resp_len:
        raw_r += sock.recv(resp_len - len(raw_r))
    sock.close()
    return json.loads(raw_r.decode("utf-8"))

if __name__ == "__main__":
    nodes = send_cmd("get_graph_nodes", {"asset_path": ASSET, "graph_name": GRAPH})["data"]

    sorted_nodes = sorted(nodes, key=lambda n: (n.get("pos_y", 0), n.get("pos_x", 0)))

    print(f"{'TITLE':<55} {'X':>6} {'Y':>6}  TYPE")
    print("-" * 85)
    for n in sorted_nodes:
        name = n.get("title", n.get("class", "?"))[:54]
        x = n.get("pos_x", 0)
        y = n.get("pos_y", 0)
        has_exec = any(p["type"] == "exec" for p in n["pins"])
        marker = "EXEC" if has_exec else "DATA"
        print(f"  {name:<55} {x:>6} {y:>6}  {marker}")
