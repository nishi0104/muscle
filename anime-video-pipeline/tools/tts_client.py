"""Agent4: ElevenLabs でシーンごとのナレーション音声 (MP3) を生成する。

python tools/tts_client.py --run <run_id> [--scenes 1,2] [--force]

シーン尺に収まらない場合は speed を上げて再生成（最大 1.2 倍）。
それでも超える場合は ffmpeg の atempo で最終調整する。
"""
from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import env, provider, is_mock, load_scenario, log, media_duration, require_env, run_agent_over_scenes, run_dir, run_ffmpeg, select_scenes  # noqa: E402

AGENT = "audio-generator"
MARGIN = 0.3  # シーン末尾に残す余白（秒）
MAX_API_SPEED = 1.2
MAX_ATEMPO = 1.35


def synthesize(text: str, out: Path, speed: float) -> None:
    import requests

    voice = require_env("ELEVENLABS_VOICE_ID")
    resp = requests.post(
        f"https://api.elevenlabs.io/v1/text-to-speech/{voice}",
        params={"output_format": "mp3_44100_128"},
        headers={"xi-api-key": require_env("ELEVENLABS_API_KEY"), "Content-Type": "application/json"},
        json={
            "text": text,
            "model_id": env("ELEVENLABS_MODEL", "eleven_multilingual_v2"),
            "voice_settings": {"stability": 0.5, "similarity_boost": 0.75, "speed": round(speed, 2)},
        },
        timeout=120,
    )
    if resp.status_code != 200:
        raise RuntimeError(f"ElevenLabs {resp.status_code}: {resp.text[:300]}")
    out.write_bytes(resp.content)


def synthesize_say(text: str, out: Path, speed: float) -> None:
    """macOS 標準の読み上げ（無料）。声は SAY_VOICE（Kyoko / Otoya など）。"""
    import subprocess

    if not shutil.which("say"):
        raise RuntimeError("say コマンドがありません（macOS 専用）。ElevenLabs のキーを設定してください")
    with tempfile.TemporaryDirectory() as tmp:
        aiff = Path(tmp) / "voice.aiff"
        rate = int(float(env("SAY_RATE", "200")) * speed)
        proc = subprocess.run(["say", "-v", env("SAY_VOICE", "Kyoko"), "-r", str(rate), "-o", str(aiff), text],
                              capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(f"say 失敗: {proc.stderr.strip()}（システム設定 > アクセシビリティ > 読み上げコンテンツ で声を追加）")
        run_ffmpeg(["-i", str(aiff), "-c:a", "libmp3lame", "-b:a", "128k", "-ar", "44100", str(out)])


def synthesize_mock(text: str, out: Path, speed: float) -> None:
    seconds = max(1.0, len(text) / 7.5 / speed)
    run_ffmpeg(["-f", "lavfi", "-i", f"sine=frequency=330:duration={seconds:.2f}",
                "-af", "volume=0.05", "-c:a", "libmp3lame", "-b:a", "128k", str(out)])


def atempo(src: Path, out: Path, factor: float) -> None:
    run_ffmpeg(["-i", str(src), "-filter:a", f"atempo={factor:.3f}", "-c:a", "libmp3lame", "-b:a", "128k", str(out)])


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--scenes")
    p.add_argument("--force", action="store_true")
    args = p.parse_args()

    scenario = load_scenario(args.run)
    out_dir = run_dir(args.run) / "audio"
    out_dir.mkdir(parents=True, exist_ok=True)
    backend = provider("TTS", [("elevenlabs", ["ELEVENLABS_API_KEY", "ELEVENLABS_VOICE_ID"]), ("say", [])])
    tts = synthesize_mock if is_mock() else {"elevenlabs": synthesize, "say": synthesize_say}[backend]
    log(f"[{AGENT}] provider: {'mock' if is_mock() else backend}")

    def work(scene: dict) -> str | None:
        out = out_dir / f"scene_{int(scene['id']):02d}.mp3"
        limit = float(scene["duration"]) - MARGIN
        tts(scene["narration"], out, 1.0)
        length = media_duration(out)
        if length <= limit:
            return f"{length:.1f}s"

        speed = min(MAX_API_SPEED, length / limit + 0.02)
        log(f"  scene {scene['id']}: {length:.1f}s > {limit:.1f}s → speed {speed:.2f} で再生成")
        tts(scene["narration"], out, speed)
        length = media_duration(out)
        if length <= limit:
            return f"{length:.1f}s (speed {speed:.2f})"

        factor = length / limit
        if factor > MAX_ATEMPO:
            raise RuntimeError(f"{length:.1f}s は {limit:.1f}s に収まりません。ナレーションを短くしてください")
        with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        try:
            atempo(out, tmp_path, factor)
            shutil.move(str(tmp_path), out)
        finally:
            tmp_path.unlink(missing_ok=True)
        return f"{media_duration(out):.1f}s (speed {speed:.2f} + atempo {factor:.2f})"

    run_agent_over_scenes(args.run, AGENT, select_scenes(scenario, args.scenes), args.force, work)


if __name__ == "__main__":
    main()
