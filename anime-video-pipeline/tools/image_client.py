"""Agent2: Gemini でシーンごとのアニメ風画像 (1080x1920) を生成する。

python tools/image_client.py --run <run_id> [--scenes 1,2] [--force]
失敗時はプロンプトを段階的に簡略化して最大 3 回試行する。
"""
from __future__ import annotations

import argparse
import io
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import H, W, env, find_font, is_mock, load_scenario, log, require_env, run_agent_over_scenes, run_dir, select_scenes  # noqa: E402

AGENT = "image-generator"
DEFAULT_STYLE = (
    "Japanese anime style illustration, vibrant colors, clean line art, cinematic lighting, "
    "vertical 9:16 composition, no text, no letters, no watermark"
)


def prompt_variants(scene: dict, style: str) -> list[str]:
    full = scene["image_prompt"]
    first = full.split(".")[0]
    return [
        f"{full}. {style}",
        f"{first}. {style}",  # 簡略化
        f"{scene.get('title_en') or first[:80]}, simple scene. Japanese anime style, vertical 9:16, no text",
    ]


def to_vertical(data: bytes, out: Path) -> None:
    """任意サイズの画像を中央クロップで 1080x1920 にする。"""
    from PIL import Image

    img = Image.open(io.BytesIO(data)).convert("RGB")
    scale = max(W / img.width, H / img.height)
    img = img.resize((int(img.width * scale + 0.5), int(img.height * scale + 0.5)), Image.LANCZOS)
    left, top = (img.width - W) // 2, (img.height - H) // 2
    img.crop((left, top, left + W, top + H)).save(out, "PNG")


def generate_gemini(prompt: str) -> bytes:
    from google import genai
    from google.genai import types

    client = genai.Client(api_key=require_env("GEMINI_API_KEY"))
    model = env("GEMINI_IMAGE_MODEL", "gemini-2.5-flash-image")
    try:
        config = types.GenerateContentConfig(
            response_modalities=["IMAGE"],
            image_config=types.ImageConfig(aspect_ratio="9:16"),
        )
    except (AttributeError, TypeError, ValueError):  # 古い SDK には image_config が無い
        config = types.GenerateContentConfig(response_modalities=["IMAGE", "TEXT"])
    resp = client.models.generate_content(model=model, contents=prompt, config=config)
    for cand in resp.candidates or []:
        for part in (cand.content.parts if cand.content else []) or []:
            if getattr(part, "inline_data", None) and part.inline_data.data:
                return part.inline_data.data
    reason = getattr(resp, "prompt_feedback", None) or getattr((resp.candidates or [None])[0], "finish_reason", None)
    raise RuntimeError(f"画像が返りませんでした (reason={reason})")


def generate_mock(scene: dict) -> bytes:
    from PIL import Image, ImageDraw, ImageFont

    sid = int(scene["id"])
    img = Image.new("RGB", (W, H))
    draw = ImageDraw.Draw(img)
    for y in range(H):  # シーンごとに色の違うグラデーション
        t = y / H
        draw.line([(0, y), (W, y)], fill=(int(40 + 150 * t), int(60 + 20 * sid % 180), int(200 - 120 * t)))
    font = ImageFont.truetype(find_font(), 80)
    draw.text((W // 2, H // 3), f"SCENE {sid}", font=font, fill="white", anchor="mm")
    draw.text((W // 2, H // 3 + 120), scene["title"][:12], font=font, fill="white", anchor="mm")
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--scenes")
    p.add_argument("--force", action="store_true")
    args = p.parse_args()

    scenario = load_scenario(args.run)
    style = scenario.get("style") or DEFAULT_STYLE
    out_dir = run_dir(args.run) / "images"
    out_dir.mkdir(parents=True, exist_ok=True)

    def work(scene: dict) -> str | None:
        out = out_dir / f"scene_{int(scene['id']):02d}.png"
        if is_mock():
            to_vertical(generate_mock(scene), out)
            return "mock"
        last: Exception | None = None
        for attempt, prompt in enumerate(prompt_variants(scene, style), 1):
            try:
                to_vertical(generate_gemini(prompt), out)
                return None if attempt == 1 else f"簡略化プロンプト(試行{attempt})で生成"
            except Exception as e:  # noqa: BLE001
                last = e
                log(f"  scene {scene['id']} 試行{attempt} 失敗: {e}")
                time.sleep(2 * attempt)
        raise RuntimeError(f"3 回失敗: {last}")

    run_agent_over_scenes(args.run, AGENT, select_scenes(scenario, args.scenes), args.force, work)


if __name__ == "__main__":
    main()
