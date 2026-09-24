"""scenario.json のスキーマ・尺チェック。問題があれば exit 1。

python tools/validate_scenario.py --run <run_id>
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import env, load_scenario  # noqa: E402

REQUIRED = ["id", "title", "image_prompt", "narration", "subtitle", "duration"]
CHARS_PER_SEC = 7.5  # 日本語ナレーションの目安（1.0倍速）


def validate(scenario: dict) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    max_total = float(env("MAX_TOTAL_SEC", "60"))

    for key in ("title", "scenes", "youtube"):
        if key not in scenario:
            errors.append(f"トップレベルに '{key}' がありません")
    scenes = scenario.get("scenes", [])
    if not 3 <= len(scenes) <= 12:
        errors.append(f"シーン数が {len(scenes)} です（3〜12、推奨 10）")

    ids = [s.get("id") for s in scenes]
    if ids != list(range(1, len(scenes) + 1)):
        errors.append(f"シーン id は 1 からの連番にしてください: {ids}")

    total = 0.0
    for s in scenes:
        sid = s.get("id", "?")
        for key in REQUIRED:
            if key not in s or s[key] in ("", None):
                errors.append(f"scene {sid}: '{key}' がありません")
        dur = float(s.get("duration") or 0)
        total += dur
        if not 3 <= dur <= 10:
            errors.append(f"scene {sid}: duration {dur} は 3〜10 秒にしてください")
        narration = s.get("narration", "")
        if dur and len(narration) > dur * CHARS_PER_SEC * 1.2:
            errors.append(f"scene {sid}: ナレーション {len(narration)} 文字は {dur} 秒に収まりません（1.2倍速でも超過）")
        elif dur and len(narration) > dur * CHARS_PER_SEC:
            warnings.append(f"scene {sid}: ナレーションが長め（速度調整が入る可能性）")
        if len(s.get("subtitle", "")) > 40:
            warnings.append(f"scene {sid}: 字幕が 40 文字超（読みにくい）")
        if not str(s.get("image_prompt", "")).isascii():
            warnings.append(f"scene {sid}: image_prompt は英語推奨")

    if total > max_total:
        errors.append(f"合計尺 {total:.1f} 秒が上限 {max_total:.0f} 秒を超えています")
    yt = scenario.get("youtube", {})
    for key in ("title", "description", "tags"):
        if key not in yt:
            errors.append(f"youtube.{key} がありません")
    return errors, warnings


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    args = p.parse_args()
    errors, warnings = validate(load_scenario(args.run))
    for w in warnings:
        print(f"WARN  {w}")
    for e in errors:
        print(f"ERROR {e}")
    if errors:
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
