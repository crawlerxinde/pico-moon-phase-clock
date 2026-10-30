#!/usr/bin/env python3
"""Fetch timeanddate moon renderings for the offline orientation test.

This is a fixture generator, not part of the normal build.  It needs a
running Chrome with the timeanddate page open and a remote-debugging port.
By default it generates four samples per month for the current year.

Example:

    google-chrome --remote-debugging-port=9227 \
        'https://www.timeanddate.com/moon/phases/china/beijing'
    python tests/fetch_moon_references.py --port 9227

The timeanddate renderer takes illumination (i), terminator position angle
(p, radians) and lunar-north position angle (r, radians).  The values are
computed by tests/moon_model.py so the renderings cover a full year.
"""

from __future__ import annotations

import argparse
import base64
import json
import math
import sys
import urllib.request
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import websocket

sys.path.insert(0, str(Path(__file__).parent))
import moon_model  # noqa: E402

BJT = timezone(timedelta(hours=8))
SAMPLE_DAYS = (4, 11, 18, 25)


def page_ws(port: int) -> str:
    pages = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json"))
    for page in pages:
        if page.get("type") == "page" and "timeanddate.com" in page.get("url", ""):
            return page["webSocketDebuggerUrl"]
    raise SystemExit("open the timeanddate moon page in Chrome first")


def cdp_call(ws: websocket.WebSocket, method: str, params: dict) -> dict:
    ws.send(json.dumps({"id": 1, "method": method, "params": params}))
    while True:
        response = json.loads(ws.recv())
        if response.get("id") == 1:
            return response


def fetch_image(ws: websocket.WebSocket, url: str, output: Path) -> None:
    expression = f"""(async () => {{
        const r = await fetch({json.dumps(url)});
        const b = await r.blob();
        return await new Promise(resolve => {{
            const f = new FileReader();
            f.onload = () => resolve(f.result);
            f.readAsDataURL(b);
        }});
    }})()"""
    response = cdp_call(
        ws,
        "Runtime.evaluate",
        {"expression": expression, "awaitPromise": True, "returnByValue": True},
    )
    data_url = response["result"]["result"].get("value")
    if not data_url or "," not in data_url:
        raise RuntimeError(f"could not fetch {url}")
    output.write_bytes(base64.b64decode(data_url.split(",", 1)[1]))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9227)
    parser.add_argument("--year", type=int, default=date.today().year)
    args = parser.parse_args()

    root = Path(__file__).parent / f"moon_reference_{args.year}"
    root.mkdir(parents=True, exist_ok=True)
    ws = websocket.create_connection(page_ws(args.port), timeout=15)
    manifest = []
    try:
        for month in range(1, 13):
            for index, day in enumerate(SAMPLE_DAYS):
                hour = 9 if index % 2 == 0 else 21
                minute = 50 if hour == 9 else 0
                local = datetime(
                    args.year, month, day, hour, minute, tzinfo=BJT
                )
                jd = moon_model.julian_from_utc(local)
                values = moon_model.screen_terms(jd)
                p = math.radians(values["terminator"]) % (2.0 * math.pi)
                r = math.radians(
                    values["north"] - values["parallactic"]
                ) % (2.0 * math.pi)
                name = local.strftime("%Y%m%d-%H%M") + ".png"
                url = (
                    "https://www.timeanddate.com/scripts/moon.php"
                    f"?i={values['illumination']:.5f}&p={p:.5f}&r={r:.5f}"
                )
                fetch_image(ws, url, root / name)
                manifest.append(
                    {
                        "local": local.isoformat(),
                        "frame": values["frame"],
                        "terminator": round(values["terminator"], 4),
                        "rotation": round(values["rotation"], 4),
                        "illumination": round(values["illumination"], 5),
                        "reference": name,
                        "source": url,
                    }
                )
                print(name)
    finally:
        ws.close()
    (root / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
