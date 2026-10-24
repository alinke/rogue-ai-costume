"""ROGUE.AI photo service (Cloud Run): the costume and the researcher's
scanner upload JPEGs here; the Halloween page lists them; a private admin
page deletes them. Temporary -- see ../teardown.sh.

  POST /upload?set=ai|lab   header X-Key: UPLOAD_KEY, body: JPEG (optionally followed by
                            X-Face-Bytes of plain photo, used only for the face check)
                            optional X-Label (shown under the photo) and X-Id
                            (the device's id for this photo: a retry of the
                            same id never makes a second copy)
  GET  /photos              {"ai": [...], "lab": [...]}, newest first
  GET  /admin?k=ADMIN_KEY   phone page: every photo, delete / show buttons
  POST /delete              JSON {"k": ADMIN_KEY, "name": "ai/....jpg"}
  POST /show                JSON {"k": ADMIN_KEY, "name": "ai-hidden/....jpg"}
  POST /clear               header X-Key: ADMIN_KEY -- deletes every photo and the agent
                            leaderboard (operator console)
  GET  /p/<id>              a guest's own photo (the QR code on the iPad checkpoint): the
                            photo with a save button; "still uploading" until it's in

Guests' phones (the LAB-7 app, after joining the event) use GUEST_KEY instead of
UPLOAD_KEY, with header X-Device (a random id per phone): rate-limited, and
GUEST_KEY can be changed after the party (./deploy.sh with ROTATE_GUEST=1)
without touching the costume or the scanners.
  POST /stats               X-Key: UPLOAD_KEY or GUEST_KEY, JSON {"device", "name",
                            "contains", "tests"}: an agent's score on the leaderboard
  GET  /leaders             {"agents": [{"name", "contains", "tests"}, ...]}, best first
  POST /report              X-Key + X-Device, JSON {"name": "lab/....jpg"}: a guest reports a
                            photo -- it goes to the hidden set until the admin page shows it
  POST /delete-own          X-Key + X-Device, JSON {"name": "lab/....jpg"}: a guest deletes a
                            photo their own phone uploaded (any other photo: 403)
  POST /claim-agent         X-Key + X-Device, JSON {"want": 0-99}: this phone's agent id (its
                            kill-switch code), unique at the event -- "want" if it's free
  GET  /game                X-Key + X-Device: the traitor game as this phone sees it (one guest
                            is secretly the AI's agent; the others try to find out who)
  POST /accuse              X-Key + X-Device, JSON {"id": player id}: one accusation per phone
  POST /game/admin          JSON {"k": ADMIN_KEY, "action": "start"|"reset"|"end"|"infect"|"cureall", "id"}:
                            "infect" orders that phone (or a random one) infected, "cureall" cures all
  GET  /game/players?k=ADMIN_KEY   every phone at the party: id, name, infected (the admin page)

Photos without a face (Cloud Vision face detection) go to <set>-hidden/
instead: not on the page, but on the admin page with a SHOW button.
"""
import datetime
import hashlib
import hmac
import json
import os
import re
import secrets
import threading
import time
from collections import defaultdict, deque

from flask import Flask, Response, jsonify, request
from google.cloud import storage, vision

BUCKET = os.environ["BUCKET"]
UPLOAD_KEY = os.environ["UPLOAD_KEY"]
ADMIN_KEY = os.environ["ADMIN_KEY"]
GUEST_KEY = os.environ.get("GUEST_KEY", "")   # the guest app's key ("" = no guest app)
# The traitor game: picked automatically at GAME_START (seconds since 1970, "" = only from the
# admin page), the agent wins if nobody exposes them by GAME_END.
GAME_START = float(os.environ.get("GAME_START", "0") or 0)
GAME_END = float(os.environ.get("GAME_END", "0") or 0)
GAME_MIN_PLAYERS = 4
PLAYER_ACTIVE_S = 3 * 3600   # a phone counts as at the party if it sent a score this recently
SETS = ("ai", "lab")
MAX_BYTES = 3 * 1024 * 1024
LIST_LIMIT = 300
GUEST_UPLOADS_PER_HOUR = 30
GUEST_REPORTS_PER_HOUR = 20
STATS_MIN_GAP = 3.0           # s between one phone's leaderboard updates
LEADERS_SHOWN = 15

FACE_MIN_CONFIDENCE = 0.5
NAME_RE = r"(ai|lab)(-hidden)?/[A-Za-z0-9-]{6,48}\.jpg"

app = Flask(__name__)
bucket = storage.Client().bucket(BUCKET)
faces_client = vision.ImageAnnotatorClient()


def face_count(data):
    """Faces Cloud Vision finds; -1 if it couldn't tell (then the photo is shown)."""
    try:
        r = faces_client.face_detection(image=vision.Image(content=data), max_results=10)
        if r.error.message:
            raise RuntimeError(r.error.message)
        return sum(1 for f in r.face_annotations if f.detection_confidence >= FACE_MIN_CONFIDENCE)
    except Exception as e:  # noqa: BLE001 -- never lose a photo over this
        print(f"face detection failed: {e}")
        return -1


def key_ok(given, expected):
    return bool(given) and bool(expected) and hmac.compare_digest(given.encode(), expected.encode())


def caller():
    """'device' (the costume / scanners: UPLOAD_KEY), 'guest' (GUEST_KEY) or None."""
    k = request.headers.get("X-Key", "")
    if key_ok(k, UPLOAD_KEY):
        return "device"
    if key_ok(k, GUEST_KEY):
        return "guest"
    return None


def device_tag():
    """A short, stable tag for the calling phone (never the raw id), or ''."""
    d = request.headers.get("X-Device", "")
    if not re.fullmatch(r"[A-Za-z0-9-]{8,64}", d):
        return ""
    return hashlib.sha256(d.encode()).hexdigest()[:12]


# Per-phone rate limits (per instance; max 3 instances, so roughly).
_hits = defaultdict(deque)
_hits_lock = threading.Lock()


def allowed(kind, tag, per_hour):
    now = time.time()
    with _hits_lock:
        q = _hits[(kind, tag)]
        while q and now - q[0] > 3600:
            q.popleft()
        if len(q) >= per_hour:
            return False
        q.append(now)
        return True


@app.after_request
def cors(resp):
    resp.headers["Access-Control-Allow-Origin"] = "*"
    resp.headers["Access-Control-Allow-Headers"] = "Content-Type, X-Key, X-Device"
    return resp


@app.post("/upload")
def upload():
    who = caller()
    if not who:
        return "forbidden", 403
    s = request.args.get("set", "")
    if s not in SETS:
        return "bad set", 400
    dev = device_tag()
    if who == "guest":
        if not dev:
            return "missing device", 400
        if s != "lab":
            return "bad set", 400
        if not allowed("upload", dev, GUEST_UPLOADS_PER_HOUR):
            return "slow down", 429
    data = request.get_data(cache=False)
    if not (2 < len(data) <= MAX_BYTES) or data[:2] != b"\xff\xd8":
        return "not a jpeg", 400
    label = re.sub(r"[^A-Za-z0-9 #:.\-]", "", request.headers.get("X-Label", ""))[:40]
    pid = request.headers.get("X-Id", "")
    if not re.fullmatch(r"[A-Za-z0-9-]{6,48}", pid):
        now = datetime.datetime.now(datetime.timezone.utc)
        pid = f"{now:%Y%m%d-%H%M%S}-{secrets.token_hex(3)}"
    for prefix in (s, s + "-hidden"):
        if bucket.blob(f"{prefix}/{pid}.jpg").exists():  # a retry: already have it
            return jsonify(name=f"{prefix}/{pid}.jpg", duplicate=True)
    # X-Face-Bytes: the request ends with a plain copy of the photo, for the
    # face check only (the costume's red scan-line dossier fools it).
    check = data
    try:
        fb = int(request.headers.get("X-Face-Bytes", "0"))
    except ValueError:
        fb = 0
    if 0 < fb < len(data) and data[len(data) - fb:len(data) - fb + 2] == b"\xff\xd8":
        check = data[len(data) - fb:]
        data = data[:len(data) - fb]
    faces = face_count(check)
    name = f"{s if faces != 0 else s + '-hidden'}/{pid}.jpg"
    blob = bucket.blob(name)
    blob.metadata = {"label": label, "faces": str(faces), "dev": dev, "by": who}
    blob.cache_control = "public, max-age=86400"
    blob.upload_from_string(data, content_type="image/jpeg")
    print(f"stored {name} ({len(data)} bytes, faces={faces})")
    return jsonify(name=name)


def entries(prefix):
    blobs = [b for b in bucket.list_blobs(prefix=prefix + "/") if b.name.endswith(".jpg")]
    blobs.sort(key=lambda b: b.time_created, reverse=True)
    return [
        {
            "name": b.name,
            "url": f"https://storage.googleapis.com/{BUCKET}/{b.name}",
            "t": b.time_created.isoformat(),
            "label": (b.metadata or {}).get("label", ""),
            "dev": (b.metadata or {}).get("dev", ""),
        }
        for b in blobs[:LIST_LIMIT]
    ]


def listing(hidden=False):
    out = {s: entries(s) for s in SETS}
    if hidden:
        out.update({s + "-hidden": entries(s + "-hidden") for s in SETS})
    return out


@app.get("/photos")
def photos():
    resp = jsonify(listing())
    resp.headers["Cache-Control"] = "public, max-age=10"
    return resp


@app.post("/delete")
def delete():
    body = request.get_json(silent=True) or {}
    if not key_ok(body.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    name = body.get("name", "")
    if not re.fullmatch(NAME_RE, name):
        return "bad name", 400
    blob = bucket.blob(name)
    if blob.exists():
        blob.delete()
    return jsonify(deleted=name)


@app.post("/show")
def show():
    """A hidden (no face found) photo onto the page after all."""
    body = request.get_json(silent=True) or {}
    if not key_ok(body.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    name = body.get("name", "")
    if not re.fullmatch(NAME_RE, name) or "-hidden/" not in name:
        return "bad name", 400
    src = bucket.blob(name)
    if not src.exists():
        return "gone", 404
    dest = name.replace("-hidden/", "/")
    bucket.copy_blob(src, bucket, dest)
    src.delete()
    return jsonify(shown=dest)


@app.post("/clear")
def clear():
    """Every photo, both sets, hidden ones too (a fresh party). Header X-Key: ADMIN_KEY."""
    if not key_ok(request.headers.get("X-Key", ""), ADMIN_KEY):
        return "forbidden", 403
    n = 0
    for prefix in ("ai/", "lab/", "ai-hidden/", "lab-hidden/", "stats/", "game/"):
        for b in bucket.list_blobs(prefix=prefix):
            b.delete()
            n += 1
    print(f"cleared {n} photos")
    return jsonify(cleared=n)


_stats_last = {}
_leaders_cache = {"t": 0.0, "data": None}


@app.post("/stats")
def stats():
    """An agent's score for the leaderboard: the phone sends its running totals."""
    if not caller():
        return "forbidden", 403
    body = request.get_json(silent=True) or {}
    dev = body.get("device", "")
    if not re.fullmatch(r"[A-Za-z0-9-]{8,64}", dev):
        return "bad device", 400
    tag = hashlib.sha256(dev.encode()).hexdigest()[:16]
    now = time.time()
    if now - _stats_last.get(tag, 0) < STATS_MIN_GAP:
        return "slow down", 429
    _stats_last[tag] = now
    name = re.sub(r"[^A-Z0-9 .\-]", "", str(body.get("name", "")).upper()).strip()[:20] or "UNKNOWN AGENT"
    try:
        contains = max(0, min(100000, int(body.get("contains", 0))))
        tests = max(0, min(100000, int(body.get("tests", 0))))
        squads = max(0, min(1000, int(body.get("squads", 0))))
    except (TypeError, ValueError):
        return "bad numbers", 400
    infected = bool(body.get("infected", False))
    blob = bucket.blob(f"stats/{tag}.json")
    try:
        since = float(json.loads(blob.download_as_bytes()).get("since", now))   # when this phone joined
    except Exception:  # noqa: BLE001 -- its first score
        since = now
    blob.cache_control = "no-store"
    blob.upload_from_string(json.dumps({"name": name, "contains": contains, "tests": tests, "squads": squads,
                                        "infected": infected, "t": now, "since": since}),
                            content_type="application/json")
    _leaders_cache["t"] = 0
    return jsonify(ok=True)


@app.get("/leaders")
def leaders():
    if _leaders_cache["data"] is None or time.time() - _leaders_cache["t"] > 15:
        agents, infected = [], 0
        for a in players(active_only=False):
            agents.append({"name": a["name"], "contains": a["contains"], "tests": a["tests"], "squads": a["squads"]})
            if a["infected"] and time.time() - a["t"] < 600:
                infected += 1
        agents.sort(key=lambda a: (a["squads"], a["contains"], a["tests"]), reverse=True)
        _leaders_cache.update(t=time.time(), data={"agents": agents[:LEADERS_SHOWN], "total": len(agents),
                                                   "infected": infected, "game": game_public()})
    resp = jsonify(_leaders_cache["data"])
    resp.headers["Cache-Control"] = "public, max-age=10"
    return resp


@app.post("/claim-agent")
def claim_agent():
    """One agent id (00-99: the kill-switch code) per phone, no two alike."""
    if not caller():
        return "forbidden", 403
    me = my_id()
    if not me:
        return "missing device", 400
    try:
        want = int((request.get_json(silent=True) or {}).get("want", -1))
    except (TypeError, ValueError):
        want = -1
    with _game_lock:
        b = bucket.blob("game/agents.json")
        try:
            taken = json.loads(b.download_as_bytes())
        except Exception:  # noqa: BLE001 -- the first claim
            taken = {}
        if me not in taken:
            used = set(taken.values())
            free = [i for i in range(100) if i not in used]
            if not free:
                return jsonify(id=want if 0 <= want < 100 else 0)   # 100+ phones: share
            taken[me] = want if want in free else secrets.choice(free)
            b.cache_control = "no-store"
            b.upload_from_string(json.dumps(taken), content_type="application/json")
    return jsonify(id=taken[me])


def players(active_only=True):
    """Every phone with a score: {"id" (the stats tag), "name", ...}. Names that two phones
    share get " (2)", " (3)"... in the order they joined."""
    out = []
    for b in bucket.list_blobs(prefix="stats/"):
        try:
            a = json.loads(b.download_as_bytes())
            p = {"id": b.name[6:-5], "name": a["name"], "contains": int(a["contains"]), "tests": int(a["tests"]),
                 "squads": int(a.get("squads", 0)), "infected": bool(a.get("infected")), "t": float(a.get("t", 0)),
                 "since": float(a.get("since", a.get("t", 0)))}
        except Exception:  # noqa: BLE001 -- skip a broken entry
            continue
        out.append(p)
    out.sort(key=lambda p: p["since"])
    seen = {}
    for p in out:
        n = seen.get(p["name"], 0) + 1
        seen[p["name"]] = n
        if n > 1:
            p["name"] = f"{p['name']} ({n})"
    if active_only:
        out = [p for p in out if time.time() - p["t"] < PLAYER_ACTIVE_S]
    return out


# ---- The traitor game ----------------------------------------------------------------------
# game/state.json: {"agent": id, "started": t, "ends": t, "exposedBy": id|None, "exposedAt": t,
#                   "accused": {id: target id}, "names": {id: name}}
_game_lock = threading.Lock()


def game_load():
    b = bucket.blob("game/state.json")
    try:
        return json.loads(b.download_as_bytes())
    except Exception:  # noqa: BLE001 -- no game yet
        return {}


def game_save(g):
    b = bucket.blob("game/state.json")
    b.cache_control = "no-store"
    b.upload_from_string(json.dumps(g), content_type="application/json")


def game_start(g):
    ps = players()
    if not ps:
        return g
    agent = secrets.choice(ps)
    now = time.time()
    g = {"agent": agent["id"], "started": now, "ends": GAME_END if GAME_END > now else 0,
         "exposedBy": None, "exposedAt": 0, "accused": {}, "names": {p["id"]: p["name"] for p in ps}}
    game_save(g)
    print(f"traitor game started: {len(ps)} players")
    return g


def game_phase(g):
    if not g.get("agent"):
        return "waiting"
    if g.get("exposedBy"):
        return "exposed"
    if g.get("ended") or (g.get("ends") and time.time() >= g["ends"]):
        return "agentWon"
    return "active"


def game_public():
    """What everyone may know (the wall, the iPad ticker): never who the agent is, until it's over."""
    g = game_load()
    ph = game_phase(g)
    out = {"phase": ph, "ends": g.get("ends", 0)}
    names = g.get("names", {})
    if ph in ("exposed", "agentWon"):
        out["agentName"] = names.get(g["agent"], "UNKNOWN")
    if ph == "exposed":
        out["byName"] = names.get(g["exposedBy"], "UNKNOWN")
    return out


def my_id():
    d = request.headers.get("X-Device", "")
    if not re.fullmatch(r"[A-Za-z0-9-]{8,64}", d):
        return ""
    return hashlib.sha256(d.encode()).hexdigest()[:16]   # the same tag as stats/<id>.json


@app.get("/game")
def game():
    if not caller():
        return "forbidden", 403
    me = my_id()
    if not me:
        return "missing device", 400
    with _game_lock:
        g = game_load()
        if not g.get("agent") and GAME_START and time.time() >= GAME_START and len(players()) >= GAME_MIN_PLAYERS:
            g = game_start(g)
    ph = game_phase(g)
    out = game_public() if g else {"phase": "waiting", "ends": 0}
    out["you"] = "agent" if g.get("agent") == me else "player"
    order = take_order(me)          # the admin page's infect / cure, delivered once
    if order:
        out["order"] = order
    out["accused"] = me in g.get("accused", {})
    if ph == "active":
        names = dict(g.get("names", {}))
        for p in players():
            names[p["id"]] = p["name"]
        out["players"] = sorted(({"id": i, "name": n} for i, n in names.items() if i != me), key=lambda p: p["name"])
    resp = jsonify(out)
    resp.headers["Cache-Control"] = "no-store"
    return resp


@app.post("/accuse")
def accuse():
    if not caller():
        return "forbidden", 403
    me = my_id()
    if not me:
        return "missing device", 400
    target = str((request.get_json(silent=True) or {}).get("id", ""))
    with _game_lock:
        g = game_load()
        if game_phase(g) != "active":
            return jsonify(result="over", game=game_public())
        if me in g["accused"]:
            return jsonify(result="used")
        if me == g["agent"]:
            return jsonify(result="agent")      # the agent can't accuse (it would give them away)
        g["accused"][me] = target
        correct = target == g["agent"]
        if correct:
            g["exposedBy"] = me
            g["exposedAt"] = time.time()
        for p in players():
            g.setdefault("names", {})[p["id"]] = p["name"]
        game_save(g)
    _leaders_cache["t"] = 0
    return jsonify(result="correct" if correct else "wrong", game=game_public())


# ---- Orders from the admin page (infect / cure), picked up by the phone's next GET /game ----
def orders_load():
    try:
        return json.loads(bucket.blob("game/orders.json").download_as_bytes())
    except Exception:  # noqa: BLE001 -- none yet
        return {}


def orders_save(o):
    b = bucket.blob("game/orders.json")
    b.cache_control = "no-store"
    b.upload_from_string(json.dumps(o), content_type="application/json")


def take_order(me):
    with _game_lock:
        o = orders_load()
        order = o.pop(me, None)
        if order:
            orders_save(o)
    return order


@app.get("/game/players")
def game_players():
    if not key_ok(request.args.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    ps = [{"id": p["id"], "name": p["name"], "infected": p["infected"] and time.time() - p["t"] < 600}
          for p in players()]
    resp = jsonify(players=sorted(ps, key=lambda p: p["name"]))
    resp.headers["Cache-Control"] = "no-store"
    return resp


@app.post("/game/admin")
def game_admin():
    body = request.get_json(silent=True) or {}
    if not key_ok(body.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    action = body.get("action")
    with _game_lock:
        g = game_load()
        if action == "start":
            g = game_start({})
        elif action == "reset":
            bucket.blob("game/state.json").delete() if bucket.blob("game/state.json").exists() else None
            g = {}
        elif action == "end" and g.get("agent"):
            g["ended"] = True
            game_save(g)
        elif action in ("infect", "cureall"):
            ps = players()
            o = orders_load()
            if action == "infect":
                target = str(body.get("id", ""))
                if not target:   # a random guest who isn't infected yet
                    healthy = [p["id"] for p in ps if not p["infected"]] or [p["id"] for p in ps]
                    if not healthy:
                        return "no players", 400
                    target = secrets.choice(healthy)
                o[target] = "infect"
            else:
                for p in ps:
                    o[p["id"]] = "cure"
            orders_save(o)
            names = {p["id"]: p["name"] for p in ps}
            return jsonify(ok=True, target=names.get(body.get("id") or (target if action == "infect" else ""), ""),
                           game=game_public(), players=len(ps))
        else:
            return "bad action", 400
    _leaders_cache["t"] = 0
    return jsonify(game=game_public(), players=len(players()))


@app.post("/report")
def report():
    """A guest reports a photo: hidden at once; the admin page can SHOW it again."""
    if not caller():
        return "forbidden", 403
    dev = device_tag()
    if not dev:
        return "missing device", 400
    if not allowed("report", dev, GUEST_REPORTS_PER_HOUR):
        return "slow down", 429
    name = (request.get_json(silent=True) or {}).get("name", "")
    if not re.fullmatch(NAME_RE, name) or "-hidden/" in name:
        return "bad name", 400
    src = bucket.blob(name)
    if not src.exists():
        return jsonify(hidden=name)
    s, rest = name.split("/", 1)
    dest = f"{s}-hidden/{rest}"
    bucket.copy_blob(src, bucket, dest)
    src.delete()
    print(f"reported {name} by {dev}")
    return jsonify(hidden=dest)


@app.post("/delete-own")
def delete_own():
    """A guest deletes their own photo: only the phone that uploaded it (same X-Device)."""
    if not caller():
        return "forbidden", 403
    dev = device_tag()
    if not dev:
        return "missing device", 400
    name = (request.get_json(silent=True) or {}).get("name", "")
    if not re.fullmatch(NAME_RE, name):
        return "bad name", 400
    s, rest = name.split("/", 1)
    base = s.replace("-hidden", "")
    for cand in (f"{base}/{rest}", f"{base}-hidden/{rest}"):   # it may have been reported (hidden) since
        b = bucket.get_blob(cand)
        if b is None:
            continue
        if (b.metadata or {}).get("dev", "") != dev:
            return "not yours", 403
        b.delete()
        print(f"deleted own {cand} by {dev}")
        return jsonify(deleted=cand)
    return jsonify(deleted=name)   # already gone


@app.get("/admin/photos")
def admin_photos():
    if not key_ok(request.args.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    resp = jsonify(listing(hidden=True))
    resp.headers["Cache-Control"] = "no-store"
    return resp


ADMIN_HTML = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex"><title>ROGUE.AI photo admin</title>
<style>
:root{--bg:#000;--fg:#ebfff0;--green:#28ff5a;--dim:#00823a;--red:#ff1e28}
body{background:var(--bg);color:var(--fg);font:15px ui-monospace,Menlo,monospace;margin:0;padding:16px}
h1{color:var(--green);font-size:18px;margin:0 0 4px}h2{color:var(--green);font-size:15px;margin:24px 0 8px}
p{color:var(--dim);margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.c{border:1px solid var(--dim);padding:6px}.c img{width:100%;display:block}
.c small{display:block;color:var(--dim);margin:4px 0;font-size:12px}
button{width:100%;background:none;border:1px solid var(--red);color:var(--red);font:inherit;padding:8px;cursor:pointer}
button.sure{background:var(--red);color:#000}
button.show{border-color:var(--green);color:var(--green);margin-bottom:6px}
</style></head><body>
<h1>ROGUE.AI // PHOTO ADMIN</h1><p>Tap DELETE twice to remove a photo from the site. Photos with no face found, or reported by a guest, aren't on the site -- SHOW puts one there. Private page -- don't share the link.</p>
<h2>THE TRAITOR GAME</h2><p id="gs">...</p>
<div class="grid" style="grid-template-columns:repeat(3,1fr)">
<button class="show" onclick="game('start')">START NOW</button>
<button class="show" onclick="game('end')">END (AGENT WINS)</button>
<button onclick="game('reset')">RESET</button></div>
<h2>THE INFECTION</h2><p>Infect a guest's scanner (their phone gets it within about 30 s), or cure everyone.</p>
<div class="grid" style="grid-template-columns:repeat(2,1fr)">
<button class="show" onclick="inf('')">INFECT A RANDOM GUEST</button>
<button class="show" onclick="cureall()">CURE EVERYONE</button></div>
<div id="pl" style="margin-top:10px"></div>
<div id="out">Loading...</div>
<script>
var K=new URLSearchParams(location.search).get('k');
function el(t,c,x){var e=document.createElement(t);if(c)e.className=c;if(x)e.textContent=x;return e}
function post(path,name){return fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},
 body:JSON.stringify({k:K,name:name})})}
function load(){fetch('admin/photos?k='+encodeURIComponent(K)).then(function(r){return r.json()}).then(function(d){
 var out=document.getElementById('out');out.textContent='';
 [['ai','ROGUE.AI (costume) -- on the site'],['lab','LAB-7 (scanner) -- on the site'],
  ['ai-hidden','ROGUE.AI -- hidden (no face found, or reported)'],['lab-hidden','LAB-7 -- hidden (no face found, or reported)']].forEach(function(s){
  var list=d[s[0]];out.appendChild(el('h2','',s[1]+' -- '+list.length));
  var g=el('div','grid');out.appendChild(g);
  list.forEach(function(p){
   var c=el('div','c');var i=el('img');i.src=p.url;i.loading='lazy';c.appendChild(i);
   c.appendChild(el('small','',new Date(p.t).toLocaleString()+(p.label?' '+p.label:'')));
   var b=el('button','','DELETE');b.onclick=function(){
    if(!b.classList.contains('sure')){b.classList.add('sure');b.textContent='SURE?';
     setTimeout(function(){b.classList.remove('sure');b.textContent='DELETE'},3000);return}
    b.disabled=true;post('delete',p.name).then(function(r){
      if(r.ok)c.remove();else{b.textContent='FAILED ('+r.status+')';b.disabled=false}})};
   if(s[0].indexOf('-hidden')>0){var sh=el('button','show','SHOW ON SITE');sh.onclick=function(){
    sh.disabled=true;post('show',p.name).then(function(r){if(r.ok)load();else{sh.textContent='FAILED';sh.disabled=false}})};
    c.appendChild(sh)}
   c.appendChild(b);g.appendChild(c)})})})}
function gshow(g,n){var t={waiting:'Not started.',active:'Running: one guest is the AI agent.',
 exposed:'Over: '+(g.agentName||'')+' exposed by '+(g.byName||''),agentWon:'Over: the agent '+(g.agentName||'')+' won.'}[g.phase]||g.phase;
 document.getElementById('gs').textContent=t+(n!=null?' Players at the party: '+n+'.':'')}
function game(a){if(a!=='start'&&!confirm('Sure? ('+a+')'))return;
 fetch('game/admin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({k:K,action:a})})
 .then(function(r){return r.json()}).then(function(d){gshow(d.game,d.players)})}
fetch('leaders').then(function(r){return r.json()}).then(function(d){if(d.game)gshow(d.game,null)});
function gadmin(body){body.k=K;return fetch('game/admin',{method:'POST',headers:{'Content-Type':'application/json'},
 body:JSON.stringify(body)}).then(function(r){return r.json()})}
function inf(id){gadmin({action:'infect',id:id}).then(function(d){
 document.getElementById('pl').firstChild&&0;alert0('Infection ordered'+(d.target?': '+d.target:'')+'. Their phone gets it within ~30 s.');players()})}
function cureall(){gadmin({action:'cureall'}).then(function(){alert0('Cure ordered for everyone.');players()})}
function alert0(t){var e=document.getElementById('pmsg');if(e)e.textContent=t}
function players(){fetch('game/players?k='+encodeURIComponent(K)).then(function(r){return r.json()}).then(function(d){
 var pl=document.getElementById('pl');pl.textContent='';var m=el('p','',' ');m.id='pmsg';pl.appendChild(m);
 if(!d.players.length){pl.appendChild(el('p','','No guests with the app yet.'));return}
 d.players.forEach(function(p){var row=el('div','');row.style.cssText='display:flex;gap:8px;align-items:center;margin:4px 0';
  var n=el('span','',(p.infected?'\u2623 ':'')+p.name);n.style.cssText='flex:1;color:'+(p.infected?'#ff1e28':'#ebfff0');
  var b=el('button','show','INFECT');b.style.width='auto';b.style.padding='6px 12px';b.onclick=function(){inf(p.id)};
  row.appendChild(n);row.appendChild(b);pl.appendChild(row)})})}
players();
load();
</script></body></html>"""


@app.get("/admin")
def admin():
    if not key_ok(request.args.get("k", ""), ADMIN_KEY):
        return "forbidden", 403
    return Response(ADMIN_HTML, headers={"Cache-Control": "no-store"})


CARD_HTML = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">{refresh}
<title>Your LAB-7 card</title>
<style>body{{margin:0;background:#0B1520;color:#EEF2F5;font:16px -apple-system,Helvetica,sans-serif;text-align:center}}
h1{{font-size:18px;letter-spacing:.08em;margin:18px 12px 4px}}p{{color:#7FB3D5;margin:6px 16px}}
img{{max-width:94vw;max-height:72vh;margin:14px auto;display:block;border:2px solid #1F4E79}}
a.b{{display:inline-block;margin:6px 0 24px;padding:12px 22px;background:#1F4E79;color:#fff;border-radius:8px;text-decoration:none;font-weight:700}}</style>
</head><body><h1>LAB-7 CONTAINMENT DIVISION</h1>{body}</body></html>"""


@app.get("/p/<pid>")
def card(pid):
    """A guest's own photo, by the id the device uploaded it under."""
    if not re.fullmatch(r"[A-Za-z0-9-]{6,48}", pid):
        return "not found", 404
    for prefix in ("lab", "lab-hidden", "ai", "ai-hidden"):
        b = bucket.blob(f"{prefix}/{pid}.jpg")
        if b.exists():
            if request.args.get("img"):  # the photo itself, same origin (for the share sheet)
                return Response(b.download_as_bytes(), mimetype="image/jpeg",
                                headers={"Cache-Control": "public, max-age=3600"})
            if request.args.get("dl"):  # download (Android, desktop)
                return Response(b.download_as_bytes(), mimetype="image/jpeg",
                                headers={"Content-Disposition": f'attachment; filename="lab7-{pid}.jpg"'})
            # SAVE TO PHOTOS opens the phone's share sheet with the photo ("Save Image" puts it in
            # Photos on an iPhone); where sharing files isn't supported it downloads it instead.
            body = (f'<p>Your humanity screening result.</p><img src="?img=1" alt="Your LAB-7 card">'
                    f'<a class="b" id="save" href="?dl=1">SAVE TO PHOTOS</a>'
                    f'<p>iPhone: tap the button, then <b>Save Image</b>. Or tap and hold the picture.</p>'
                    # The photo is fetched as the page loads: Safari opens the share sheet only
                    # straight from the tap, not after waiting for a download.
                    '<script>var file=null;fetch("?img=1").then(function(r){return r.blob()}).then(function(b){'
                    f'file=new File([b],"lab7-{pid}.jpg",{{type:"image/jpeg"}})}});'
                    'document.getElementById("save").onclick=function(e){'
                    'if(file&&navigator.canShare&&navigator.canShare({files:[file]})){'
                    'e.preventDefault();navigator.share({files:[file]}).catch(function(){})}};</script>')
            return Response(CARD_HTML.format(refresh="", body=body), headers={"Cache-Control": "no-store"})
    body = "<p>Your card is still uploading to the lab.</p><p>This page checks again every few seconds.</p>"
    return Response(CARD_HTML.format(refresh='<meta http-equiv="refresh" content="4">', body=body),
                    headers={"Cache-Control": "no-store"})


@app.get("/")
def root():
    return "ROGUE.AI photo service"
