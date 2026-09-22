import json
import re
import time
from pathlib import Path

import requests


NOMINATIM_URL = "https://nominatim.openstreetmap.org/search"

HEADERS = {
    "User-Agent": "RouteMind-hackathon/1.0 (geocoding script)"
}

BASE_DIR = Path(__file__).resolve().parent.parent
INPUT_FILE = BASE_DIR / "data" / "requests.json"
OUTPUT_FILE = BASE_DIR / "data" / "locations.json"
FAILED_FILE = BASE_DIR / "data" / "geocode_failed.json"

# ---------------------------------------------------------
# Нормализация текста
# ---------------------------------------------------------

def normalize_text(value):
    if not value:
        return ""

    value = value.lower().strip()
    value = value.replace("ё", "е")
    value = re.sub(r"\s+", " ", value)

    return value


# ---------------------------------------------------------
# Определяем город
# ---------------------------------------------------------

def detect_city(address):
    text = normalize_text(address)

    if "москва" in text:
        return "Москва"

    if "домодедово" in text:
        return "Домодедово"

    if "кашир" in text:
        return "Кашира"

    if "ступино" in text:
        return "Ступино"

    return None


# ---------------------------------------------------------
# Извлекаем улицу и дом
# ---------------------------------------------------------

def parse_address(address):
    original = address.strip()
    city = detect_city(original)

    if not city:
        return {
            "original": original,
            "city": None,
            "street": None,
            "house": None,
        }

    text = original

    # Убираем различные префиксы
    prefixes = [
        r"^г\.\s*город\s+москва\s*,?\s*",
        r"^г\.\s*москва\s*,?\s*",
        r"^город\s+москва\s*,?\s*",
        r"^москва\s*,?\s*",

        r"^г\.\s*домодедово\s*,?\s*",
        r"^домодедово\s*,?\s*",

        r"^г\.\s*кашир[аы]\s*,?\s*",
        r"^кашир[аы]\s*,?\s*",

        r"^г\.\s*ступино\s*,?\s*",
        r"^ступино\s*,?\s*",

        r"^мо\s*,?\s*",
        r"^обл\.\s*московская\s+область\s*,?\s*",
    ]

    for pattern in prefixes:
        text = re.sub(
            pattern,
            "",
            text,
            flags=re.IGNORECASE,
        )

    text = text.strip(" ,")

    # Если после МО остался "г. Кашира" / "г. Ступино" / "г. Домодедово
    text = re.sub(
        r"^г\.\s*(?:москва|домодедово|кашир[аы]|ступино)\s*,?\s*",
        "",
        text,
        flags=re.IGNORECASE,
    )

    # Убираем населенный пункт типа пгт.Востряково-1
    text = re.sub(
        r"^пгт\.\s*[^,]+,?\s*",
        "",
        text,
        flags=re.IGNORECASE,
    )

    # Нормализуем сокращения типов улиц
    replacements = [
        (r"\bул\.", "улица"),
        (r"\bул\b", "улица"),
        (r"\bпр-кт\.", "проспект"),
        (r"\bпр-кт\b", "проспект"),
        (r"\bбул\.", "бульвар"),
        (r"\bбул\b", "бульвар"),
        (r"\bнаб\.", "набережная"),
        (r"\bнаб\b", "набережная"),
        (r"\bпер\.", "переулок"),
        (r"\bпер\b", "переулок"),
        (r"\bш\.", "шоссе"),
        (r"\bш\b", "шоссе"),
        (r"\bпл\.", "площадь"),
        (r"\bпл\b", "площадь"),
        (r"\bпр-зд\.", "проезд"),
        (r"\bпроезд\.", "проезд"),
    ]

    for pattern, replacement in replacements:
        text = re.sub(
            pattern,
            replacement,
            text,
            flags=re.IGNORECASE,
        )

    text = re.sub(r"\s+", " ", text).strip(" ,")

    # -----------------------------------------------------
    # Вариант:
    # улица Боровая, д. 12
    # улица Окская, д. 6 к 1
    # -----------------------------------------------------

    match = re.search(
        r"""
        (?P<type>
            улица|
            проспект|
            бульвар|
            набережная|
            переулок|
            проезд|
            шоссе|
            площадь
        )
        \s*
        (?P<street>.+?)
        \s*,?\s*
        д\.?\s*
        (?P<house>\d+[А-Яа-яA-Za-z]?(?:[/-]\d+[А-Яа-яA-Za-z]?)?)
        (?P<rest>.*)
        $
        """,
        text,
        flags=re.IGNORECASE | re.VERBOSE,
    )

    if match:
        street_type = match.group("type")
        street = match.group("street").strip()
        house = match.group("house").strip()
        rest = match.group("rest").strip()

        return {
            "original": original,
            "city": city,
            "street": f"{street_type} {street}",
            "house": house,
            "rest": rest,
        }

    # -----------------------------------------------------
    # Вариант:
    # Андропова ул. д. 37
    # Центральная ул. д. 21
    # -----------------------------------------------------

    match = re.search(
        r"""
        (?P<street>.+?)
        \s+
        (?P<type>
            улица|
            проспект|
            бульвар|
            набережная|
            переулок|
            проезд|
            шоссе|
            площадь
        )
        \s*,?\s*
        д\.?\s*
        (?P<house>\d+[А-Яа-яA-Za-z]?(?:[/-]\d+[А-Яа-яA-Za-z]?)?)
        (?P<rest>.*)
        $
        """,
        text,
        flags=re.IGNORECASE | re.VERBOSE,
    )

    if match:
        street = match.group("street").strip()
        street_type = match.group("type")
        house = match.group("house").strip()
        rest = match.group("rest").strip()

        return {
            "original": original,
            "city": city,
            "street": f"{street_type} {street}",
            "house": house,
            "rest": rest,
        }

    return {
        "original": original,
        "city": city,
        "street": None,
        "house": None,
        "rest": text,
    }


# ---------------------------------------------------------
# Нормализация номера дома
# ---------------------------------------------------------

def normalize_house(value):
    if not value:
        return ""

    value = normalize_text(value)

    value = re.sub(r"\s+", "", value)
    value = value.replace("корпус", "к")
    value = value.replace("строение", "с")

    return value


# ---------------------------------------------------------
# Проверяем результат Nominatim
# ---------------------------------------------------------

def result_matches(result, expected_city, expected_house):
    address = result.get("address", {})

    result_city = (
        address.get("city")
        or address.get("town")
        or address.get("municipality")
        or address.get("village")
        or ""
    )

    result_city = normalize_text(result_city)
    expected_city = normalize_text(expected_city)

    # Город должен совпадать
    if result_city != expected_city:
        return False

    # Если Nominatim дал номер дома —
    # проверяем и его.
    result_house = address.get("house_number")

    if expected_house and result_house:
        if normalize_house(result_house) != normalize_house(expected_house):
            return False

    return True


# ---------------------------------------------------------
# Запрос к Nominatim
# ---------------------------------------------------------

def nominatim_search(params):
    try:
        response = requests.get(
            NOMINATIM_URL,
            params=params,
            headers=HEADERS,
            timeout=20,
        )

        print(f"    HTTP {response.status_code}")

        if response.status_code != 200:
            print(f"    ERROR: {response.text[:200]}")
            return []

        return response.json()

    except requests.RequestException as e:
        print(f"    REQUEST ERROR: {e}")
        return []


# ---------------------------------------------------------
# Геокодирование одного адреса
# ---------------------------------------------------------

def geocode_address(parsed):
    city = parsed["city"]
    street = parsed["street"]
    house = parsed["house"]

    if not city:
        return None

    # -----------------------------------------------------
    # Попытка №1 — структурированный запрос
    # -----------------------------------------------------

    if street:
        street_query = street

        if house:
            street_query += f" {house}"

        print(f"    structured: {street_query}, {city}")

        params = {
            "street": street_query,
            "city": city,
            "country": "Россия",
            "countrycodes": "ru",
            "format": "jsonv2",
            "addressdetails": 1,
            "limit": 10,
        }

        results = nominatim_search(params)

        for result in results:
            if result_matches(result, city, house):
                return result

        time.sleep(1.2)

    # -----------------------------------------------------
    # Попытка №2 — обычный текстовый запрос
    # -----------------------------------------------------

    query_parts = []

    if street:
        query_parts.append(street)

    if house:
        query_parts.append(house)

    query_parts.append(city)
    query_parts.append("Россия")

    query = ", ".join(query_parts)

    print(f"    freeform: {query}")

    params = {
        "q": query,
        "countrycodes": "ru",
        "format": "jsonv2",
        "addressdetails": 1,
        "limit": 10,
    }

    results = nominatim_search(params)

    for result in results:
        if result_matches(result, city, house):
            return result

    return None


# ---------------------------------------------------------
# Основная функция
# ---------------------------------------------------------

def main():

    print("=" * 70)
    print("ROUTEMIND GEOCODER")
    print("=" * 70)

    print(f"INPUT : {INPUT_FILE}")
    print(f"OUTPUT: {OUTPUT_FILE}")
    print()

    failed_addresses = []

    if not INPUT_FILE.exists():
        print(f"ERROR: файл не найден: {INPUT_FILE}")
        return

    # Загружаем requests.json
    with open(INPUT_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)

    print(f"Загружен JSON: {len(data)} записей")
    print()

    results = []

    success = 0
    failed = 0

    for index, item in enumerate(data, start=1):

        # В requests.json адрес может находиться
        # в разных полях. Сначала ищем address.
        address = (
            item.get("address")
            or item.get("full_address")
            or item.get("location")
            or ""
        )

        print("-" * 70)
        print(f"[{index}/{len(data)}]")
        print(f"ADDRESS: {address}")

        if not address:
            print("  FAILED: адрес отсутствует")
            failed += 1
            continue

        parsed = parse_address(address)

        print(f"  CITY  : {parsed['city']}")
        print(f"  STREET: {parsed['street']}")
        print(f"  HOUSE : {parsed['house']}")

        result = geocode_address(parsed)

        if result:
            lat = float(result["lat"])
            lon = float(result["lon"])

            print(f"  SUCCESS: {lat}, {lon}")
            print(f"  FOUND: {result.get('display_name', '')}")

            results.append({
                "request_id": item.get("request_id"),
                "address": address,
                "latitude": lat,
                "longitude": lon,
            })

            success += 1

        else:
            print("  FAILED: не найдено точное совпадение")

            failed_addresses.append({
                "request_id": item.get("request_id"),
                "address": address,
            })

            failed += 1

        # Соблюдаем паузу между запросами
        time.sleep(1.2)

    # -----------------------------------------------------
    # Сохраняем результат
    # -----------------------------------------------------

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        json.dump(
            results,
            f,
            ensure_ascii=False,
            indent=2,
        )

    with open(FAILED_FILE, "w", encoding="utf-8") as f:
        json.dump(
            failed_addresses,
            f,
            ensure_ascii=False,
            indent=2,
        )

    print()
    print("=" * 70)
    print("ГОТОВО")
    print("=" * 70)

    print(f"TOTAL  : {len(data)}")
    print(f"SUCCESS: {success}")
    print(f"FAILED : {failed}")

    if data:
        print(f"RATE   : {success / len(data) * 100:.1f}%")

    print()
    print(f"Результат сохранён: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()