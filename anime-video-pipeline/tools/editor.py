"""Agent5: クリップ + ナレーション + テロップを合成して 1 本の縦型動画にする。

python tools/editor.py --run <run_id>
出力: runs/<run_id>/output/final.mp4
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import state as st  # noqa: E402
from common import FPS, H, W, env, find_font, load_scenario, log, media_duration, run_dir, run_ffmpeg  # noqa: E402

AGENT = "video-editor"
TELOP_FONT_SIZE = 72
TELOP_BOTTOM = int(H * 0.80)  # テロップ下端の位置
ENC = ["-c:v", "libx264", "-preset", "veryfast", "-crf", "20", "-pix_fmt", "yuv420p",
       "-c:a", "aac", "-b:a", "192k", "-ar", "44100", "-ac", "2"]


def wrap(text: str, font, max_width: int) -> list[str]:
    """日本語は単語区切りが無いので 1 文字ずつ幅を測って折り返す。"""
    lines: list[str] = []
    for para in text.split("\n"):
        line = ""
        for ch in para:
            if font.getlength(line + ch) > max_width and line:
                lines.append(line)
                line = ch
            else:
                line += ch
        lines.append(line)
    return lines


def render_telop(text: str, out: Path) -> None:
    """白文字 + 黒縁取りのテロップを透過 PNG (1080x1920) で描画する。"""
    from PIL import Image, ImageDraw, ImageFont

    font = ImageFont.truetype(find_font(), TELOP_FONT_SIZE)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    lines = wrap(text, font, int(W * 0.88))  # float を渡さないよう int() にする
    line_h = int(TELOP_FONT_SIZE * 1.35)
    y = TELOP_BOTTOM - line_h * len(lines)
    for line in lines:
        draw.text((W // 2, y), line, font=font, fill="white", anchor="ma",
                  stroke_width=8, stroke_fill="black")
        y += line_h
    img.save(out)


def build_segment(clip: Path, audio: Path | None, telop: Path | None, duration: float, out: Path) -> None:
    args = ["-stream_loop", "-1", "-i", str(clip)]
    idx = 1
    if telop:
        args += ["-loop", "1", "-i", str(telop)]
        telop_idx, idx = idx, idx + 1
    if audio and audio.exists():
        args += ["-i", str(audio)]
    else:
        args += ["-f", "lavfi", "-i", "anullsrc=r=44100:cl=stereo"]
    audio_idx = idx

    fc = f"[0:v]trim=duration={duration:.3f},setpts=PTS-STARTPTS,scale={W}:{H},setsar=1,fps={FPS}[v0];"
    fc += f"[v0][{telop_idx}:v]overlay=0:0:shortest=1[v];" if telop else "[v0]null[v];"
    fc += (f"[{audio_idx}:a]aresample=44100,aformat=channel_layouts=stereo,apad,"
           f"atrim=duration={duration:.3f},asetpts=PTS-STARTPTS[a]")
    run_ffmpeg([*args, "-filter_complex", fc, "-map", "[v]", "-map", "[a]",
                "-t", f"{duration:.3f}", "-r", str(FPS), *ENC, str(out)])


def finalize(concat: Path, out: Path, total: float) -> None:
    """BGM ミックスと、上限尺を超えたときの速度調整を行う。"""
    max_total = float(env("MAX_TOTAL_SEC", "60"))
    speed = total / max_total if total > max_total else 1.0
    if speed > 1.25:
        raise RuntimeError(f"合計 {total:.1f}s は {max_total:.0f}s に収まりません（シナリオを短くしてください）")
    bgm = env("BGM_PATH")
    if speed == 1.0 and not bgm:
        concat.replace(out)
        return

    args = ["-i", str(concat)]
    vf = f"[0:v]setpts=PTS/{speed:.4f}[v]"
    af = f"[0:a]atempo={speed:.4f}[na]"
    if bgm:
        args += ["-stream_loop", "-1", "-i", bgm]
        af += (f";[1:a]volume={env('BGM_VOLUME', '0.12')},aresample=44100[bg];"
               f"[na][bg]amix=inputs=2:duration=first:dropout_transition=0[a]")
    else:
        af += ";[na]anull[a]"
    if speed > 1.0:
        log(f"合計 {total:.1f}s → {speed:.2f} 倍速で {max_total:.0f}s に収めます")
    run_ffmpeg([*args, "-filter_complex", f"{vf};{af}", "-map", "[v]", "-map", "[a]",
                "-t", f"{total / speed:.3f}", *ENC, "-movflags", "+faststart", str(out)])
    concat.unlink(missing_ok=True)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    args = p.parse_args()

    scenario = load_scenario(args.run)
    rd = run_dir(args.run)
    work = rd / "edit"
    work.mkdir(parents=True, exist_ok=True)
    (rd / "output").mkdir(exist_ok=True)
    st.set_status(args.run, AGENT, "in_progress")

    try:
        segments: list[Path] = []
        total = 0.0
        for scene in scenario["scenes"]:
            sid = int(scene["id"])
            clip = rd / "clips" / f"scene_{sid:02d}.mp4"
            if not clip.exists():
                raise RuntimeError(f"{clip} がありません（video-generator 未完了）")
            audio = rd / "audio" / f"scene_{sid:02d}.mp3"
            duration = float(scene["duration"])
            if audio.exists():  # 音声がはみ出す場合はシーンを延ばして同期を保つ
                duration = max(duration, media_duration(audio) + 0.2)
            telop = None
            if scene.get("subtitle"):
                telop = work / f"telop_{sid:02d}.png"
                render_telop(scene["subtitle"], telop)
            seg = work / f"seg_{sid:02d}.mp4"
            build_segment(clip, audio, telop, duration, seg)
            segments.append(seg)
            total += duration
            log(f"[{AGENT}] scene {sid}: {duration:.1f}s")

        listing = work / "concat.txt"
        listing.write_text("".join(f"file '{s.resolve()}'\n" for s in segments), encoding="utf-8")
        concat = work / "concat.mp4"
        run_ffmpeg(["-f", "concat", "-safe", "0", "-i", str(listing), "-c", "copy", "-movflags", "+faststart", str(concat)])
        final = rd / "output" / "final.mp4"
        finalize(concat, final, total)
    except Exception as e:
        st.set_status(args.run, AGENT, "failed", str(e)[:1000])
        raise SystemExit(f"[{AGENT}] 失敗: {e}")

    st.set_status(args.run, AGENT, "completed")
    print(f"{final} ({media_duration(final):.1f}s)")


if __name__ == "__main__":
    main()
