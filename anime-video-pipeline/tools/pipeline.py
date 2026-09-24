"""Claude を介さずに scenario.json 以降の DAG を実行するランナー（再実行・CI 用）。

python tools/pipeline.py --run <run_id> [--upload]

state.json を見て未完了のエージェントだけを実行する。
画像生成 → 動画生成 の流れと音声生成を並列に走らせる。
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import state as st  # noqa: E402
from common import log  # noqa: E402

TOOLS = Path(__file__).resolve().parent
COMMANDS = {
    "image-generator": ["image_client.py"],
    "video-generator": ["video_client.py"],
    "audio-generator": ["tts_client.py"],
    "video-editor": ["editor.py"],
    "youtube-uploader": ["youtube_upload.py"],
}


def run(agent: str, run_id: str) -> bool:
    if st.load_state(run_id)["agents"][agent]["status"] in ("completed", "skipped"):
        log(f"== {agent}: 完了済み")
        return True
    log(f"== {agent}: 開始")
    ok = subprocess.run([sys.executable, str(TOOLS / COMMANDS[agent][0]), "--run", run_id]).returncode == 0
    log(f"== {agent}: {'完了' if ok else '失敗'}")
    return ok


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--upload", action="store_true", help="最後に YouTube へアップロードする")
    args = p.parse_args()
    rid = args.run

    if st.load_state(rid)["agents"]["scene-planner"]["status"] != "completed":
        raise SystemExit("scene-planner が未完了です。Claude Code で /make-video を実行してください")

    with ThreadPoolExecutor(max_workers=2) as pool:
        visual = pool.submit(lambda: run("image-generator", rid) and run("video-generator", rid))
        audio = pool.submit(run, "audio-generator", rid)
        ok = visual.result() & audio.result()
    if not ok or not run("video-editor", rid):
        raise SystemExit(f"失敗しました。状態: python tools/state.py show {rid}")
    if args.upload and not run("youtube-uploader", rid):
        raise SystemExit(1)
    log(f"完成: runs/{rid}/output/final.mp4")


if __name__ == "__main__":
    main()
