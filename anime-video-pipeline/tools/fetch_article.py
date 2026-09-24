"""ニュース記事 URL（またはテキスト）を runs/<run_id>/source.txt に保存する。

python tools/fetch_article.py --run <run_id> --input "<URL or テキスト>"
"""
from __future__ import annotations

import argparse
import html
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import log, run_dir  # noqa: E402


class _TextExtractor(HTMLParser):
    SKIP = {"script", "style", "noscript", "nav", "footer", "header", "aside", "form", "svg"}
    BLOCK = {"p", "h1", "h2", "h3", "li", "article", "br", "div"}

    def __init__(self) -> None:
        super().__init__()
        self.title = ""
        self.parts: list[str] = []
        self._skip = 0
        self._in_title = False

    def handle_starttag(self, tag, attrs):
        if tag in self.SKIP:
            self._skip += 1
        elif tag == "title":
            self._in_title = True
        elif tag in self.BLOCK:
            self.parts.append("\n")

    def handle_endtag(self, tag):
        if tag in self.SKIP and self._skip:
            self._skip -= 1
        elif tag == "title":
            self._in_title = False

    def handle_data(self, data):
        if self._in_title:
            self.title += data
        elif not self._skip:
            self.parts.append(data)


def fetch(url: str) -> str:
    import requests

    resp = requests.get(url, timeout=30, headers={"User-Agent": "Mozilla/5.0 (anime-video-pipeline)"})
    resp.raise_for_status()
    resp.encoding = resp.apparent_encoding or resp.encoding
    parser = _TextExtractor()
    parser.feed(resp.text)
    body = html.unescape("".join(parser.parts))
    lines = [re.sub(r"\s+", " ", ln).strip() for ln in body.splitlines()]
    # 短すぎる行（メニュー等）を除外
    lines = [ln for ln in lines if len(ln) >= 15]
    return f"タイトル: {parser.title.strip()}\nURL: {url}\n\n" + "\n".join(lines)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--input", required=True)
    args = p.parse_args()

    out = run_dir(args.run) / "source.txt"
    out.parent.mkdir(parents=True, exist_ok=True)
    if re.match(r"https?://", args.input.strip()):
        text = fetch(args.input.strip())
    else:
        text = args.input
    out.write_text(text[:20000], encoding="utf-8")
    log(f"保存しました: {out} ({len(text)} 文字)")


if __name__ == "__main__":
    main()
