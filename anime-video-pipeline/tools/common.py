"""パイプライン共通ユーティリティ（パス・環境変数・ffmpeg・JSON）。"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
RUNS_DIR = ROOT / "runs"
W, H, FPS = 1080, 1920, 30

try:
    from dotenv import load_dotenv

    load_dotenv(ROOT / ".env")
except ImportError:  # python-dotenv 未導入でも環境変数だけで動く
    pass


def env(name: str, default: str | None = None) -> str | None:
    value = os.environ.get(name)
    return value if value not in (None, "") else default


def require_env(name: str) -> str:
    value = env(name)
    if not value:
        raise RuntimeError(f"環境変数 {name} が未設定です（.env を確認してください）")
    return value


def provider(kind: str, auto_order: list[tuple[str, list[str]]]) -> str:
    """<KIND>_PROVIDER を返す。auto なら必要なキーが揃っている最初の候補を選ぶ。"""
    chosen = env(f"{kind}_PROVIDER", "auto")
    if chosen != "auto":
        return chosen
    for name, keys in auto_order:
        if all(env(k) for k in keys):
            return name
    return auto_order[-1][0]


def is_mock() -> bool:
    return env("PIPELINE_MOCK", "0") == "1"


def log(msg: str) -> None:
    print(msg, file=sys.stderr, flush=True)


def run_dir(run_id: str) -> Path:
    return RUNS_DIR / run_id


def load_json(path: Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def save_json(path: Path, data: Any) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=path.parent, suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, path)


def load_scenario(run_id: str) -> dict:
    path = run_dir(run_id) / "scenario.json"
    if not path.exists():
        raise SystemExit(f"{path} がありません。先に scene-planner を実行してください")
    return load_json(path)


def select_scenes(scenario: dict, only: str | None) -> list[dict]:
    """--scenes "1,3" のような指定でシーンを絞り込む。"""
    scenes = scenario["scenes"]
    if not only:
        return scenes
    wanted = {int(x) for x in only.split(",") if x.strip()}
    return [s for s in scenes if int(s["id"]) in wanted]


FONT_CANDIDATES = [
    "/System/Library/Fonts/ヒラギノ角ゴシック W8.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
    "C:/Windows/Fonts/meiryob.ttc",
    "C:/Windows/Fonts/meiryo.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
]


def find_font() -> str:
    explicit = env("FONT_PATH")
    if explicit:
        return explicit
    for cand in FONT_CANDIDATES:
        if Path(cand).exists():
            return cand
    raise SystemExit("日本語フォントが見つかりません。.env の FONT_PATH を設定してください")


def run_agent_over_scenes(run_id: str, agent: str, scenes: list[dict], force: bool, fn) -> None:
    """シーン単位で fn(scene) を実行し state.json を更新する共通ループ。

    fn は成功時に任意の note(str|None) を返し、失敗時は例外を投げる。
    完了済みシーンはスキップするので、再実行時は失敗分だけが処理される。
    """
    import state as st

    st.set_status(run_id, agent, "in_progress")
    failures: list[str] = []
    for scene in scenes:
        sid = int(scene["id"])
        if not force and st.scene_done(run_id, agent, sid):
            log(f"[{agent}] scene {sid}: 完了済みのためスキップ")
            continue
        try:
            note = fn(scene)
            st.mark_scene(run_id, agent, sid, note)
            log(f"[{agent}] scene {sid}: OK" + (f" ({note})" if note else ""))
        except Exception as e:  # noqa: BLE001 - シーン単位で失敗を記録して続行
            failures.append(f"scene {sid}: {e}")
            log(f"[{agent}] scene {sid}: 失敗 {e}")

    all_ids = {int(s["id"]) for s in load_scenario(run_id)["scenes"]}
    done = set(st.load_state(run_id)["agents"][agent]["scenes_completed"])
    if failures or not all_ids <= done:
        st.set_status(run_id, agent, "failed", "; ".join(failures) or f"未完了シーン: {sorted(all_ids - done)}")
        raise SystemExit(1)
    st.set_status(run_id, agent, "completed")


def ffmpeg_bin() -> str:
    explicit = env("FFMPEG_BIN")
    if explicit:
        return explicit
    found = shutil.which("ffmpeg")
    if found:
        return found
    try:
        import imageio_ffmpeg

        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise SystemExit("ffmpeg が見つかりません。ffmpeg をインストールするか pip install imageio-ffmpeg")


def run_ffmpeg(args: list[str]) -> None:
    cmd = [ffmpeg_bin(), "-y", "-hide_banner", "-loglevel", "error", *args]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"ffmpeg 失敗: {' '.join(cmd)}\n{proc.stderr.strip()}")


def media_duration(path: Path) -> float:
    """ffprobe 無しでも動くよう ffmpeg -i の出力から尺を取得する。"""
    proc = subprocess.run([ffmpeg_bin(), "-hide_banner", "-i", str(path)], capture_output=True, text=True)
    m = re.search(r"Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?)", proc.stderr)
    if not m:
        raise RuntimeError(f"尺を取得できません: {path}")
    h, mnt, s = m.groups()
    return int(h) * 3600 + int(mnt) * 60 + float(s)
