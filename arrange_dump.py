"""Dump graph structure for analysis without moving nodes."""
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

    # Build exec flow
    exec_ids = set()
    data_ids = set()
    for n in nodes:
        if any(p["type"] == "exec" for p in n["pins"]):
            exec_ids.add(n["node_id"])
        else:
            data_ids.add(n["node_id"])

    exec_children = {}
    exec_parents = {}
    for nid in exec_ids:
        exec_children[nid] = []
        exec_parents[nid] = []

    for n in nodes:
        if n["node_id"] not in exec_ids:
            continue
        for pin in n["pins"]:
            if pin["type"] == "exec" and pin["direction"] == "Output":
                for conn in pin.get("connections", []):
                    cid = conn["node_id"]
                    if cid in exec_ids:
                        exec_children[n["node_id"]].append(cid)
                        exec_parents[cid].append(n["node_id"])

    roots = [nid for nid in exec_ids if not exec_parents[nid]]
    by_id = {n["node_id"]: n for n in nodes}

    def get_name(nid):
        n = by_id[nid]
        return n.get("title", n.get("node_class", nid[:8]))

    # Print exec chains from each root
    for root in roots:
        print(f"\n{'='*60}")
        print(f"ROOT: {get_name(root)}")
        print(f"{'='*60}")

        visited = set()
        def print_chain(nid, indent=0):
            if nid in visited:
                print(f"{'  '*indent}-> (already visited) {get_name(nid)}")
                return
            visited.add(nid)
            n = by_id[nid]
            pin_count = len(n["pins"])
            children = exec_children.get(nid, [])

            # Count data inputs
            data_inputs = []
            for pin in n["pins"]:
                if pin["direction"] == "Input" and pin["type"] != "exec":
                    for conn in pin.get("connections", []):
                        if conn["node_id"] in data_ids:
                            data_inputs.append(get_name(conn["node_id"]))

            data_str = f" <- [{', '.join(data_inputs)}]" if data_inputs else ""
            branch_str = f" ({len(children)} outputs)" if len(children) > 1 else ""
            print(f"{'  '*indent}{get_name(nid)} [{pin_count}pins]{branch_str}{data_str}")

            for i, child in enumerate(children):
                if len(children) > 1:
                    print(f"{'  '*indent}  [branch {i}]:")
                print_chain(child, indent + 1)

        print_chain(root)

    # Print data nodes
    print(f"\n{'='*60}")
    print(f"DATA NODES ({len(data_ids)})")
    print(f"{'='*60}")
    for nid in data_ids:
        n = by_id[nid]
        consumers = []
        for pin in n["pins"]:
            if pin["direction"] == "Output":
                for conn in pin.get("connections", []):
                    consumers.append(get_name(conn["node_id"]))
        print(f"  {get_name(nid)} -> [{', '.join(consumers)}]")
