"""
Compact arrange script v11.
Two-pass: place exec chains first, then data nodes, then shift
subgraphs apart based on actual bounding boxes for consistent gaps.
"""
import socket, json, struct, uuid
from collections import defaultdict

# ============ TUNING PARAMS ============
PARAMS = {
    "h_spacing": 400,       # horizontal gap between consecutive exec nodes
    "branch_gap": 200,      # vertical gap when a branch creates a new row
    "subgraph_gap": 150,    # visual gap between bottom of one subgraph and top of next
    "data_x_offset": 160,   # how far left of consumer for data nodes
    "data_y_offset": 60,    # how far below consumer's Y for data nodes
    "data_v_gap": 35,       # stacking gap for multiple data nodes on same parent
    "est_node_height": 100, # estimated rendered height of a node
}

ASSET = "/Game/Blueprints/Player/BP_PlayerCharacter"
GRAPH = "EventGraph"

# ============ TCP ============
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

# ============ ARRANGE ============
def arrange(nodes):
    P = PARAMS
    by_id = {n["node_id"]: n for n in nodes}

    # Classify exec vs data
    exec_ids = set()
    data_ids = set()
    for n in nodes:
        if any(p["type"] == "exec" for p in n["pins"]):
            exec_ids.add(n["node_id"])
        else:
            data_ids.add(n["node_id"])

    # Build exec flow
    exec_children = {nid: [] for nid in exec_ids}
    exec_parents = {nid: [] for nid in exec_ids}
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

    # Find roots
    roots = [nid for nid in exec_ids if not exec_parents[nid]]

    # ======= PASS 1: Place exec nodes (relative to subgraph origin 0,0) =======
    # Each subgraph gets its own local coordinate space first
    subgraphs = []  # list of (root_name, {nid: (x,y)} )

    def place_exec_chain(root):
        """Place exec chain starting from root. Returns dict of {nid: (x,y)} in local coords."""
        local_pos = {}
        placed = set()

        def place(nid, col, y):
            if nid in placed:
                return y
            placed.add(nid)
            local_pos[nid] = (col * P["h_spacing"], y)
            bottom_y = y

            children = [c for c in exec_children.get(nid, []) if c not in placed]
            if not children:
                return bottom_y

            bottom_y = place(children[0], col + 1, y)

            for child in children[1:]:
                if child not in placed:
                    branch_y = bottom_y + P["branch_gap"]
                    bottom_y = place(child, col + 1, branch_y)

            return bottom_y

        place(root, 0, 0)
        return local_pos

    placed_roots = set()
    for root in roots:
        if root in placed_roots:
            continue
        placed_roots.add(root)
        local = place_exec_chain(root)
        # Mark all placed nodes so other roots don't re-place
        for nid in local:
            placed_roots.add(nid)
        name = by_id[root].get("title", by_id[root].get("class", "?"))
        subgraphs.append((name, root, local))

    # Orphan exec nodes
    for nid in exec_ids:
        if nid not in placed_roots:
            local = place_exec_chain(nid)
            for nid2 in local:
                placed_roots.add(nid2)
            name = by_id[nid].get("title", by_id[nid].get("class", "?"))
            subgraphs.append((name, nid, local))

    # ======= PASS 2: Place data nodes for each subgraph (local coords) =======
    placed_data = set()

    def get_data_inputs(nid):
        n = by_id[nid]
        result = []
        for pin in n["pins"]:
            if pin["direction"] != "Input" or pin["type"] == "exec":
                continue
            for conn in pin.get("connections", []):
                did = conn["node_id"]
                if did in data_ids and did not in placed_data:
                    result.append(did)
        return result

    def place_data_tree(nid, local_pos):
        data_inputs = get_data_inputs(nid)
        if not data_inputs:
            return
        ex, ey = local_pos[nid]
        dy = ey + P["data_y_offset"]
        for did in data_inputs:
            local_pos[did] = (ex - P["data_x_offset"], dy)
            placed_data.add(did)
            dy += P["data_v_gap"]
            place_data_tree(did, local_pos)

    for name, root, local_pos in subgraphs:
        # Place data for exec nodes, sorted left to right
        exec_nids = sorted(
            [nid for nid in local_pos if nid in exec_ids],
            key=lambda nid: local_pos[nid][0]
        )
        for nid in exec_nids:
            place_data_tree(nid, local_pos)

    # ======= PASS 3: Calculate bounding boxes and stack subgraphs =======
    positions = {}
    current_y = 0
    subgraph_info = []

    for name, root, local_pos in subgraphs:
        if not local_pos:
            continue

        # Find bounding box of this subgraph
        min_y = min(y for _, y in local_pos.values())
        max_y = max(y for _, y in local_pos.values())
        bbox_height = (max_y - min_y) + P["est_node_height"]

        # Shift all nodes to global position
        y_offset = current_y - min_y
        for nid, (lx, ly) in local_pos.items():
            positions[nid] = (lx, ly + y_offset)

        subgraph_info.append((name, current_y, current_y + bbox_height))
        current_y += bbox_height + P["subgraph_gap"]

    # Any truly orphaned data nodes
    max_y = max(y for _, y in positions.values()) if positions else 0
    for nid in data_ids:
        if nid not in positions:
            n = by_id[nid]
            found = False
            for pin in n["pins"]:
                for conn in pin.get("connections", []):
                    if conn["node_id"] in positions:
                        cx, cy = positions[conn["node_id"]]
                        positions[nid] = (cx - P["data_x_offset"], cy + P["data_y_offset"])
                        found = True
                        break
                if found:
                    break
            if not found:
                max_y += 200
                positions[nid] = (0, max_y)

    return positions, subgraph_info

# ============ DIAGNOSTICS ============
def print_diagnostics(nodes, positions, subgraph_info):
    by_id = {n["node_id"]: n for n in nodes}

    print("\n=== SUBGRAPH SUMMARY ===")
    for name, top, bottom in subgraph_info:
        print(f"  {name}: Y={top:.0f} to {bottom:.0f} (height={bottom-top:.0f})")

    xs = [x for x, y in positions.values()]
    ys = [y for x, y in positions.values()]
    print(f"\n=== LAYOUT BOUNDS ===")
    print(f"  X range: {min(xs)} to {max(xs)} ({max(xs)-min(xs)} wide)")
    print(f"  Y range: {min(ys)} to {max(ys)} ({max(ys)-min(ys)} tall)")
    print(f"  Total nodes positioned: {len(positions)}")

    exec_count = sum(1 for n in nodes if any(p["type"] == "exec" for p in n["pins"]))
    data_count = len(nodes) - exec_count
    print(f"  Exec nodes: {exec_count}, Data nodes: {data_count}")

    # Check for overlaps
    pos_map = {}
    overlaps = 0
    for n in nodes:
        nid = n["node_id"]
        if nid in positions:
            key = positions[nid]
            if key in pos_map:
                overlaps += 1
                name1 = by_id[pos_map[key]].get("title", "?")[:30]
                name2 = by_id[nid].get("title", "?")[:30]
                print(f"  OVERLAP at {key}: {name1} & {name2}")
            else:
                pos_map[key] = nid
    if overlaps == 0:
        print("  No overlapping positions!")

# ============ MAIN ============
if __name__ == "__main__":
    print("Getting nodes...")
    resp = send_cmd("get_graph_nodes", {"asset_path": ASSET, "graph_name": GRAPH})
    if "data" not in resp:
        print(f"Error: {resp}")
        exit(1)
    nodes = resp["data"]
    print(f"  {len(nodes)} nodes")

    print("Arranging...")
    pos, subgraph_info = arrange(nodes)
    print_diagnostics(nodes, pos, subgraph_info)

    pos_list = [{"node_id": nid, "x": x, "y": y} for nid, (x, y) in pos.items()]

    print("\nSetting positions...")
    r = send_cmd("set_node_positions", {"asset_path": ASSET, "graph_name": GRAPH, "positions": pos_list})
    print(f"  {r.get('data', r.get('error', '?'))}")
    print("Done!")
