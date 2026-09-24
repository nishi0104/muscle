"""Agent3: 静止画を動画クリップ (1080x1920) に変換する。

python tools/video_client.py --run <run_id> [--scenes 1,2] [--force] [--fallback-only]

- fal.ai の image-to-video モデル（FAL_VIDEO_MODEL、既定 Kling）を使用
- 出力が正方形などでも ffmpeg で 1080x1920 に正規化（ぼかし背景で余白を埋める）
- API 失敗・コンテンツポリシー違反時は ffmpeg の Ken Burns スライドショーで代替
"""
from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import FPS, H, W, env, is_mock, load_scenario, log, require_env, run_agent_over_scenes, run_dir, run_ffmpeg, select_scenes  # noqa: E402

AGENT = "video-generator"
MOTION_SUFFIX = "smooth subtle camera motion, anime style animation, consistent character"


def normalize(src: Path, out: Path) -> None:
    """任意の解像度・アスペクト比を 1080x1920/30fps に揃える（音声は捨てる）。"""
    vf = (
        f"[0:v]split=2[a][b];"
        f"[a]scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H},boxblur=30:2[bg];"
        f"[b]scale={W}:{H}:force_original_aspect_ratio=decrease[fg];"
        f"[bg][fg]overlay=(W-w)/2:(H-h)/2,setsar=1,fps={FPS},format=yuv420p[v]"
    )
    run_ffmpeg(["-i", str(src), "-filter_complex", vf, "-map", "[v]", "-an",
                "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", str(out)])


def ken_burns(image: Path, out: Path, duration: float) -> None:
    """フォールバック: 静止画にゆっくりズームをかけた動画を作る。"""
    frames = int(duration * FPS) + 1
    vf = (
        f"scale={W * 2}:{H * 2},"
        f"zoompan=z='min(zoom+0.0007,1.12)':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d={frames}:s={W}x{H}:fps={FPS},"
        f"setsar=1,format=yuv420p"
    )
    run_ffmpeg(["-i", str(image), "-vf", vf, "-t", f"{duration:.2f}",
                "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", str(out)])


def generate_fal(image: Path, prompt: str, motion: str, out: Path) -> None:
    import fal_client
    import requests

    require_env("FAL_KEY")
    model = env("FAL_VIDEO_MODEL", "fal-ai/kling-video/v1/standard/image-to-video")
    arguments = {
        "prompt": f"{prompt}. {motion}",
        "image_url": fal_client.upload_file(str(image)),
        "duration": env("FAL_VIDEO_DURATION", "5"),
    }
    arguments.update(json.loads(env("FAL_VIDEO_EXTRA_ARGS", "{}")))
    result = fal_client.subscribe(model, arguments=arguments)
    video = result.get("video") or (result.get("videos") or [None])[0]
    if not video or not video.get("url"):
        raise RuntimeError(f"動画 URL が返りませんでした: {str(result)[:300]}")
    with tempfile.NamedTemporaryFile(suffix=".mp4", delete=False) as tmp:
        resp = requests.get(video["url"], timeout=300)
        resp.raise_for_status()
        tmp.write(resp.content)
    try:
        normalize(Path(tmp.name), out)
    finally:
        Path(tmp.name).unlink(missing_ok=True)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--scenes")
    p.add_argument("--force", action="store_true")
    p.add_argument("--fallback-only", action="store_true", help="API を使わずスライドショーで作る")
    args = p.parse_args()

    scenario = load_scenario(args.run)
    motion = scenario.get("motion") or MOTION_SUFFIX
    rd = run_dir(args.run)
    out_dir = rd / "clips"
    out_dir.mkdir(parents=True, exist_ok=True)

    def work(scene: dict) -> str | None:
        sid = int(scene["id"])
        image = rd / "images" / f"scene_{sid:02d}.png"
        if not image.exists():
            raise RuntimeError(f"{image} がありません（image-generator 未完了）")
        out = out_dir / f"scene_{sid:02d}.mp4"
        if is_mock() or args.fallback_only:
            ken_burns(image, out, float(scene["duration"]))
            return "fallback: slideshow"
        try:
            generate_fal(image, scene.get("motion_prompt") or scene["image_prompt"], motion, out)
            return None
        except Exception as e:  # noqa: BLE001 - ポリシー違反・モデル未公開なども含め代替する
            log(f"  scene {sid} API 失敗 → スライドショーで代替: {e}")
            ken_burns(image, out, float(scene["duration"]))
            return f"fallback: slideshow ({str(e)[:120]})"

    run_agent_over_scenes(args.run, AGENT, select_scenes(scenario, args.scenes), args.force, work)


if __name__ == "__main__":
    main()
