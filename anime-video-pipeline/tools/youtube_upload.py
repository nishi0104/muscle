"""Agent6: 完成動画を YouTube Shorts にアップロードする。

python tools/youtube_upload.py --run <run_id> [--privacy private|unlisted|public] [--dry-run]

初回は OAuth 認証のためブラウザが開く（トークンは YOUTUBE_TOKEN_PATH に保存）。
既定の公開範囲は private（.env の YOUTUBE_PRIVACY で変更）。
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import state as st  # noqa: E402
from common import ROOT, env, is_mock, load_scenario, log, run_dir, save_json  # noqa: E402

AGENT = "youtube-uploader"
SCOPES = ["https://www.googleapis.com/auth/youtube.upload"]


def build_metadata(scenario: dict, privacy: str) -> dict:
    yt = scenario["youtube"]
    title = yt["title"]
    if "#shorts" not in title.lower():
        title = f"{title} #Shorts"
    description = yt["description"]
    if scenario.get("source"):
        description += f"\n\n出典: {scenario['source']}"
    return {
        "snippet": {
            "title": title[:100],
            "description": description[:5000],
            "tags": yt.get("tags", [])[:30],
            "categoryId": str(yt.get("category_id", "25")),  # 25 = News & Politics
            "defaultLanguage": "ja",
        },
        "status": {"privacyStatus": privacy, "selfDeclaredMadeForKids": False},
    }


def youtube_client():
    from google.auth.transport.requests import Request
    from google.oauth2.credentials import Credentials
    from google_auth_oauthlib.flow import InstalledAppFlow
    from googleapiclient.discovery import build

    token_path = ROOT / env("YOUTUBE_TOKEN_PATH", "youtube_token.json")
    creds = Credentials.from_authorized_user_file(str(token_path), SCOPES) if token_path.exists() else None
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            secrets = ROOT / env("YOUTUBE_CLIENT_SECRETS", "client_secret.json")
            creds = InstalledAppFlow.from_client_secrets_file(str(secrets), SCOPES).run_local_server(port=0)
        token_path.write_text(creds.to_json(), encoding="utf-8")
    return build("youtube", "v3", credentials=creds)


def upload(video: Path, body: dict) -> str:
    from googleapiclient.http import MediaFileUpload

    request = youtube_client().videos().insert(
        part="snippet,status", body=body,
        media_body=MediaFileUpload(str(video), mimetype="video/mp4", chunksize=8 * 1024 * 1024, resumable=True),
    )
    response = None
    while response is None:
        status, response = request.next_chunk()
        if status:
            log(f"  アップロード {int(status.progress() * 100)}%")
    return response["id"]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--run", required=True)
    p.add_argument("--privacy", choices=["private", "unlisted", "public"], default=env("YOUTUBE_PRIVACY", "private"))
    p.add_argument("--dry-run", action="store_true", help="メタデータを表示するだけでアップロードしない")
    args = p.parse_args()

    rd = run_dir(args.run)
    video = rd / "output" / "final.mp4"
    if not video.exists():
        raise SystemExit(f"{video} がありません（video-editor 未完了）")
    body = build_metadata(load_scenario(args.run), args.privacy)

    if args.dry_run or is_mock():
        print(json.dumps(body, ensure_ascii=False, indent=2))
        log("dry-run のためアップロードしていません")
        if is_mock() and not args.dry_run:
            st.set_status(args.run, AGENT, "skipped", "mock モード")
        return

    st.set_status(args.run, AGENT, "in_progress")
    try:
        video_id = upload(video, body)
    except Exception as e:
        st.set_status(args.run, AGENT, "failed", str(e)[:1000])
        raise SystemExit(f"[{AGENT}] 失敗: {e}")
    url = f"https://youtube.com/shorts/{video_id}"
    save_json(rd / "output" / "youtube.json", {"video_id": video_id, "url": url, "privacy": args.privacy})
    st.set_status(args.run, AGENT, "completed")
    print(url)


if __name__ == "__main__":
    main()
