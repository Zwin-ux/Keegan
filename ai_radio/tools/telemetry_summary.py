import argparse
import glob
import json
import os
from collections import Counter
from datetime import datetime


def default_paths():
    here = os.path.abspath(os.path.dirname(__file__))
    repo_root = os.path.abspath(os.path.join(here, ".."))
    candidates = []

    cache_path = os.path.join(repo_root, "cache", "telemetry.jsonl")
    if os.path.exists(cache_path):
        candidates.append(cache_path)

    server_glob = os.path.join(repo_root, "server", "data", "telemetry-*.jsonl")
    candidates.extend(sorted(glob.glob(server_glob)))
    return candidates


def load_events(paths):
    events = []
    for path in paths:
        try:
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        events.append(json.loads(line))
                    except json.JSONDecodeError:
                        continue
        except FileNotFoundError:
            continue
    return events


def fmt_ts(value):
    try:
        return datetime.fromtimestamp(int(value) / 1000).strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return "unknown"


def summarize(events):
    total = len(events)
    by_event = Counter()
    by_source = Counter()
    by_station = Counter()
    by_room = Counter()
    sessions = set()
    times = []

    for ev in events:
        name = str(ev.get("event", "unknown"))
        by_event[name] += 1
        source = ev.get("source")
        if source:
            by_source[str(source)] += 1
        station_id = ev.get("stationId")
        if station_id:
            by_station[str(station_id)] += 1
        room_id = ev.get("roomId")
        if room_id:
            by_room[str(room_id)] += 1
        session_id = ev.get("sessionId")
        if session_id:
            sessions.add(str(session_id))
        ts = ev.get("ts")
        if isinstance(ts, (int, float, str)):
            try:
                times.append(int(ts))
            except Exception:
                pass

    return {
        "total": total,
        "events": by_event,
        "sources": by_source,
        "stations": by_station,
        "rooms": by_room,
        "sessions": len(sessions),
        "time_start": min(times) if times else None,
        "time_end": max(times) if times else None,
    }


def print_summary(summary):
    print("Telemetry summary")
    print("-----------------")
    print(f"Total events: {summary['total']}")
    print(f"Unique sessions: {summary['sessions']}")
    if summary["time_start"] and summary["time_end"]:
        print(f"Time range: {fmt_ts(summary['time_start'])} -> {fmt_ts(summary['time_end'])}")
    print("")

    if summary["sources"]:
        print("By source:")
        for name, count in summary["sources"].most_common():
            print(f"  {name}: {count}")
        print("")

    if summary["events"]:
        print("Top events:")
        for name, count in summary["events"].most_common(12):
            print(f"  {name}: {count}")
        print("")

    if summary["stations"]:
        print("Top stations:")
        for name, count in summary["stations"].most_common(8):
            print(f"  {name}: {count}")
        print("")

    if summary["rooms"]:
        print("Top rooms:")
        for name, count in summary["rooms"].most_common(8):
            print(f"  {name}: {count}")


def main():
    parser = argparse.ArgumentParser(description="Summarize Frequency telemetry JSONL logs.")
    parser.add_argument("paths", nargs="*", help="Paths to telemetry .jsonl files")
    args = parser.parse_args()

    paths = args.paths or default_paths()
    if not paths:
        print("No telemetry files found. Enable KEEGAN_TELEMETRY=1 and try again.")
        return 1

    events = load_events(paths)
    summary = summarize(events)
    print_summary(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
