"""state.json によるステート管理（途中再開用）。

状態遷移: pending → in_progress → completed / failed
画像・動画・音声はシーン単位で scenes_completed に記録する。

CLI:
  python tools/state.py init <run_id> --input "<URL or テキスト>"
  python tools/state.py show <run_id>
  python tools/state.py next <run_id>          # 今実行できるエージェント一覧
  python tools/state.py set <run_id> <agent> <status> [--error MSG]
  python tools/state.py reset <run_id> <agent> # 完了済みを再実行したいとき
"""
from __future__ import annotations

import argparse
import contextlib
import datetime as dt
import json
import sys
from pathlib import Path
from typing import Iterator

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import load_json, run_dir, save_json  # noqa: E402

AGENTS = [
    "scene-planner",
    "image-generator",
    "audio-generator",
    "video-generator",
    "video-editor",
    "youtube-uploader",
]
# DAG: 画像生成と音声生成は scene-planner のみに依存するので並列実行できる
DEPS = {
    "scene-planner": [],
    "image-generator": ["scene-planner"],
    "audio-generator": ["scene-planner"],
    "video-generator": ["image-generator"],
    "video-editor": ["video-generator", "audio-generator"],
    "youtube-uploader": ["video-editor"],
}
STATUSES = {"pending", "in_progress", "completed", "failed", "skipped"}


def _now() -> str:
    return dt.datetime.now().isoformat(timespec="seconds")


def state_path(run_id: str) -> Path:
    return run_dir(run_id) / "state.json"


@contextlib.contextmanager
def _locked(run_id: str) -> Iterator[None]:
    """並列エージェントが同時に書き込んでも壊れないようファイルロックする。"""
    lock = run_dir(run_id) / ".state.lock"
    lock.parent.mkdir(parents=True, exist_ok=True)
    with open(lock, "w") as f:
        try:
            import fcntl

            fcntl.flock(f, fcntl.LOCK_EX)
        except ImportError:  # Windows
            pass
        yield


def init_state(run_id: str, source: str) -> dict:
    path = state_path(run_id)
    if path.exists():
        return load_json(path)
    state = {
        "run_id": run_id,
        "input": source,
        "created_at": _now(),
        "agents": {
            name: {"status": "pending", "scenes_completed": [], "error": None, "updated_at": None, "notes": {}}
            for name in AGENTS
        },
    }
    save_json(path, state)
    return state


def load_state(run_id: str) -> dict:
    path = state_path(run_id)
    if not path.exists():
        raise SystemExit(f"{path} がありません。先に init してください")
    return load_json(path)


def _update(run_id: str, agent: str, fn) -> dict:
    with _locked(run_id):
        state = load_state(run_id)
        fn(state["agents"][agent])
        state["agents"][agent]["updated_at"] = _now()
        save_json(state_path(run_id), state)
        return state


def set_status(run_id: str, agent: str, status: str, error: str | None = None) -> None:
    assert status in STATUSES, status

    def fn(a: dict) -> None:
        a["status"] = status
        a["error"] = error

    _update(run_id, agent, fn)


def mark_scene(run_id: str, agent: str, scene_id: int, note: str | None = None) -> None:
    def fn(a: dict) -> None:
        if scene_id not in a["scenes_completed"]:
            a["scenes_completed"].append(scene_id)
            a["scenes_completed"].sort()
        if note:
            a["notes"][str(scene_id)] = note

    _update(run_id, agent, fn)


def scene_done(run_id: str, agent: str, scene_id: int) -> bool:
    return scene_id in load_state(run_id)["agents"][agent]["scenes_completed"]


def reset(run_id: str, agent: str) -> None:
    def fn(a: dict) -> None:
        a.update(status="pending", scenes_completed=[], error=None, notes={})

    _update(run_id, agent, fn)


def runnable(run_id: str) -> list[str]:
    agents = load_state(run_id)["agents"]
    done = {n for n, a in agents.items() if a["status"] in ("completed", "skipped")}
    return [n for n in AGENTS if n not in done and all(d in done for d in DEPS[n])]


def main() -> None:
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("init")
    s.add_argument("run_id")
    s.add_argument("--input", required=True)
    for name in ("show", "next"):
        sub.add_parser(name).add_argument("run_id")
    s = sub.add_parser("set")
    s.add_argument("run_id")
    s.add_argument("agent", choices=AGENTS)
    s.add_argument("status", choices=sorted(STATUSES))
    s.add_argument("--error")
    s = sub.add_parser("reset")
    s.add_argument("run_id")
    s.add_argument("agent", choices=AGENTS)
    args = p.parse_args()

    if args.cmd == "init":
        print(json.dumps(init_state(args.run_id, args.input), ensure_ascii=False, indent=2))
    elif args.cmd == "show":
        print(json.dumps(load_state(args.run_id), ensure_ascii=False, indent=2))
    elif args.cmd == "next":
        print(json.dumps(runnable(args.run_id)))
    elif args.cmd == "set":
        set_status(args.run_id, args.agent, args.status, args.error)
    elif args.cmd == "reset":
        reset(args.run_id, args.agent)


if __name__ == "__main__":
    main()
