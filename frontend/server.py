import argparse
import json
import os
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import time
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlparse
from urllib.request import Request, urlopen


FRONTEND_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = Path(os.environ.get("ROUTEMIND_ROOT", FRONTEND_DIR.parent)).resolve()
DATA_DIR = PROJECT_ROOT / "data"
USER_REQUESTS = FRONTEND_DIR / "user_requests.json"
RUNTIME_DIR = FRONTEND_DIR / ".runtime"
LOCAL_PLANNER = RUNTIME_DIR / "route_planner"
GEOCODE_CACHE = RUNTIME_DIR / "geocode_cache.json"
GEOCODER_URL = os.environ.get("ROUTEMIND_GEOCODER_URL", "https://nominatim.openstreetmap.org/search")
PLANNER_LOCK = threading.Lock()
GEOCODE_LOCK = threading.Lock()
LAST_GEOCODE_REQUEST = 0.0
SYSTEM_CA_FILE = Path(os.environ.get("SSL_CERT_FILE", "/etc/ssl/cert.pem"))
SSL_CONTEXT = ssl.create_default_context(cafile=str(SYSTEM_CA_FILE) if SYSTEM_CA_FILE.exists() else None)

CPP_SOURCES = [
    "cpp/src/main.cpp",
    "cpp/src/planner/Planner.cpp",
    "cpp/src/planner/Repairer.cpp",
    "cpp/src/planner/Scheduler.cpp",
    "cpp/src/routing/Router.cpp",
    "cpp/src/metrics/Metrics.cpp",
    "cpp/src/io/JsonLoader.cpp",
    "cpp/src/io/JsonWriter.cpp",
]


def read_json(path, default=None):
    if not path.exists():
        return default
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def write_json_atomic(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
    temporary.replace(path)


def request_locations():
    locations = read_json(DATA_DIR / "locations.json", [])
    return {
        str(item["request_id"]): item
        for item in locations
        if item.get("request_id") is not None
    }


def brigade_locations():
    locations = read_json(DATA_DIR / "brigade_locations.json", [])
    return {
        item.get("address", ""): item
        for item in locations
        if item.get("address")
    }


def all_requests():
    requests = read_json(DATA_DIR / "requests.json", [])
    requests.extend(read_json(USER_REQUESTS, []))
    locations = request_locations()
    result = []
    for item in requests:
        request = dict(item)
        location = locations.get(str(request.get("request_id")))
        if location:
            request["lat"] = location.get("latitude")
            request["lon"] = location.get("longitude")
        request.setdefault("duration_minutes", 60)
        request.setdefault("status", "new")
        result.append(request)
    return result


def all_brigades():
    locations = brigade_locations()
    result = []
    for item in read_json(DATA_DIR / "brigades.json", []):
        brigade = dict(item)
        location = locations.get(brigade.get("start_address", ""))
        if location:
            brigade["start_lat"] = location.get("latitude")
            brigade["start_lon"] = location.get("longitude")
        brigade.setdefault("shift_start", "08:00")
        brigade.setdefault("shift_end", "20:00")
        result.append(brigade)
    return result


def next_request_id(existing):
    numeric = [int(str(item.get("request_id"))) for item in existing if str(item.get("request_id", "")).isdigit()]
    return str(max(numeric, default=int(time.time()) % 100000) + 1)


def geocode_address(address):
    address = " ".join(str(address or "").split())
    if len(address) < 5:
        raise ValueError("Введите полный адрес")

    key = address.casefold()
    known = list(read_json(DATA_DIR / "locations.json", [])) + list(read_json(USER_REQUESTS, []))
    for item in known:
        if str(item.get("address", "")).strip().casefold() != key:
            continue
        lat = item.get("lat", item.get("latitude"))
        lon = item.get("lon", item.get("longitude"))
        if lat not in (None, "") and lon not in (None, ""):
            return {
                "lat": float(lat),
                "lon": float(lon),
                "display_name": item.get("address", address),
                "address_details": {},
                "source": "local",
            }

    global LAST_GEOCODE_REQUEST
    with GEOCODE_LOCK:
        cache = read_json(GEOCODE_CACHE, {}) or {}
        if key in cache:
            return cache[key]

        delay = 1.05 - (time.monotonic() - LAST_GEOCODE_REQUEST)
        if delay > 0:
            time.sleep(delay)

        query = urlencode({
            "q": address,
            "format": "jsonv2",
            "addressdetails": 1,
            "countrycodes": "ru",
            "limit": 3,
            "accept-language": "ru",
        })
        request = Request(
            f"{GEOCODER_URL}?{query}",
            headers={
                "User-Agent": "RouteMind-Hackathon/1.0 (local educational prototype)",
                "Accept": "application/json",
                "Accept-Language": "ru",
            },
        )
        LAST_GEOCODE_REQUEST = time.monotonic()
        try:
            with urlopen(request, timeout=12, context=SSL_CONTEXT) as response:
                results = json.loads(response.read().decode("utf-8"))
        except (HTTPError, URLError, TimeoutError) as error:
            raise RuntimeError("Сервис определения координат временно недоступен") from error

        if not results:
            raise ValueError("Адрес не найден. Уточните город, улицу и номер дома")

        result = {
            "lat": float(results[0]["lat"]),
            "lon": float(results[0]["lon"]),
            "display_name": results[0].get("display_name", address),
            "address_details": results[0].get("address", {}),
            "source": "nominatim",
        }
        RUNTIME_DIR.mkdir(exist_ok=True)
        cache[key] = result
        write_json_atomic(GEOCODE_CACHE, cache)
        return result


def create_request(payload):
    required = ["address", "window_start", "window_end", "required_skill"]
    missing = [field for field in required if not payload.get(field)]
    if missing:
        raise ValueError("Не заполнены обязательные поля: " + ", ".join(missing))

    existing = all_requests()
    request = {
        "request_id": next_request_id(existing),
        "region": payload.get("region", "Югоцентр"),
        "district": payload.get("district", ""),
        "address": payload["address"],
        "bk_type": payload.get("bk_type", "Локальная заявка"),
        "hd_type": payload.get("hd_type", ""),
        "required_skill": payload["required_skill"],
        "required_specialization": payload.get("required_specialization", ""),
        "required_transport": payload.get("required_transport"),
        "window_start": payload["window_start"],
        "window_end": payload["window_end"],
        "duration_minutes": int(payload.get("duration_minutes", 60)),
        "comment": payload.get("comment", ""),
    }

    lat = payload.get("lat")
    lon = payload.get("lon")
    if lat not in (None, "") and lon not in (None, ""):
        request["lat"] = float(lat)
        request["lon"] = float(lon)
    else:
        by_address = {
            item.get("address", "").strip().casefold(): item
            for item in read_json(DATA_DIR / "locations.json", [])
        }
        location = by_address.get(request["address"].strip().casefold())
        if location:
            request["lat"] = location.get("latitude")
            request["lon"] = location.get("longitude")

    user_requests = read_json(USER_REQUESTS, [])
    user_requests.append(request)
    write_json_atomic(USER_REQUESTS, user_requests)
    return {**request, "status": "new"}


def ensure_planner():
    sources = [PROJECT_ROOT / item for item in CPP_SOURCES]
    missing = [str(path) for path in sources if not path.exists()]
    if missing:
        raise RuntimeError("Не найдены исходники C++: " + ", ".join(missing))

    newest_source = max(path.stat().st_mtime for path in sources)
    if LOCAL_PLANNER.exists() and LOCAL_PLANNER.stat().st_mtime >= newest_source:
        return LOCAL_PLANNER

    compiler = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("Не найден компилятор C++. Установите Xcode Command Line Tools")

    json_include = PROJECT_ROOT / "build" / "_deps" / "json-src" / "include"
    if not json_include.exists():
        raise RuntimeError("Не найдена библиотека nlohmann/json. Выполните: cmake -S . -B build")

    RUNTIME_DIR.mkdir(exist_ok=True)
    system_flags = []
    if sys.platform == "darwin" and shutil.which("xcrun"):
        sdk_result = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False)
        sdk = sdk_result.stdout.strip()
        if sdk:
            system_flags = ["-isysroot", sdk, "-isystem", str(Path(sdk) / "usr" / "include" / "c++" / "v1")]

    command = [
        compiler,
        "-std=c++20",
        "-O2",
        *system_flags,
        "-I",
        str(PROJECT_ROOT / "cpp" / "include"),
        "-I",
        str(json_include),
        *[str(path) for path in sources],
        "-o",
        str(LOCAL_PLANNER),
    ]
    result = subprocess.run(command, capture_output=True, text=True, timeout=180, check=False)
    if result.returncode != 0:
        raise RuntimeError((result.stderr or result.stdout or "Не удалось собрать C++ планировщик")[-2000:])
    return LOCAL_PLANNER


def build_plan(requests, brigades):
    with PLANNER_LOCK:
        planner = ensure_planner()

    with PLANNER_LOCK, tempfile.TemporaryDirectory(prefix="routemind-") as directory:
        root = Path(directory)
        data = root / "data"
        output = root / "output"
        data.mkdir()
        output.mkdir()

        clean_requests = []
        generated_locations = request_locations()
        for source in requests:
            item = dict(source)
            for key in ["status", "brigade_id", "reason", "lat", "lon", "latitude", "longitude"]:
                item.pop(key, None)
            item.setdefault("duration_minutes", int(source.get("duration_minutes", 60)))
            clean_requests.append(item)
            lat = source.get("lat", source.get("latitude"))
            lon = source.get("lon", source.get("longitude"))
            if lat not in (None, "") and lon not in (None, ""):
                generated_locations[str(source["request_id"])] = {
                    "request_id": str(source["request_id"]),
                    "address": source.get("address", ""),
                    "latitude": float(lat),
                    "longitude": float(lon),
                }

        write_json_atomic(data / "requests.json", clean_requests)
        write_json_atomic(data / "brigades.json", brigades)
        write_json_atomic(data / "locations.json", list(generated_locations.values()))
        write_json_atomic(data / "brigade_locations.json", read_json(DATA_DIR / "brigade_locations.json", []))

        result = subprocess.run(
            [str(planner)],
            cwd=root,
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
        if result.returncode != 0:
            message = result.stderr.strip() or result.stdout.strip() or "Планировщик завершился с ошибкой"
            raise RuntimeError(message[-1200:])

        plan_path = output / "plan.json"
        if not plan_path.exists():
            raise RuntimeError("Планировщик не создал output/plan.json")
        return read_json(plan_path, {})


class RouteMindHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(FRONTEND_DIR), **kwargs)

    def log_message(self, format, *args):
        print(f"{self.address_string()} - {format % args}")

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_json(self, value, status=HTTPStatus.OK):
        body = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_body(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length > 10_000_000:
            raise ValueError("Слишком большой запрос")
        return json.loads(self.rfile.read(length).decode("utf-8") or "{}")

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/health":
            self.send_json({"status": "ok", "planner": LOCAL_PLANNER.exists(), "compiler": bool(shutil.which("c++") or shutil.which("clang++") or shutil.which("g++"))})
            return
        if parsed.path == "/api/requests":
            date = parse_qs(parsed.query).get("date", [""])[0]
            requests = all_requests()
            if date:
                requests = [item for item in requests if str(item.get("window_start", "")).startswith(date)]
            self.send_json(requests)
            return
        if parsed.path == "/api/brigades":
            self.send_json(all_brigades())
            return
        super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        try:
            payload = self.read_body()
            if parsed.path == "/api/geocode":
                self.send_json(geocode_address(payload.get("address")))
                return
            if parsed.path == "/api/requests":
                self.send_json(create_request(payload), HTTPStatus.CREATED)
                return
            if parsed.path == "/api/routes/build":
                requests = payload.get("requests")
                brigades = payload.get("brigades")
                if not isinstance(requests, list) or not isinstance(brigades, list):
                    raise ValueError("Ожидались массивы requests и brigades")
                self.send_json(build_plan(requests, brigades))
                return
            self.send_json({"detail": "Endpoint не найден"}, HTTPStatus.NOT_FOUND)
        except ValueError as error:
            self.send_json({"detail": str(error)}, HTTPStatus.BAD_REQUEST)
        except subprocess.TimeoutExpired:
            self.send_json({"detail": "Планировщик не завершил расчёт за 120 секунд"}, HTTPStatus.GATEWAY_TIMEOUT)
        except Exception as error:
            self.send_json({"detail": str(error)}, HTTPStatus.INTERNAL_SERVER_ERROR)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    if not DATA_DIR.exists():
        raise SystemExit(f"Не найдена папка данных: {DATA_DIR}")

    server = ThreadingHTTPServer((args.host, args.port), RouteMindHandler)
    print(f"RouteMind: http://{args.host}:{args.port}")
    print("Остановка: Ctrl+C")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
