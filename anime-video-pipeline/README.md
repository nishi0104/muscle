# anime-video-pipeline

ニュース記事の URL を渡すだけで、アニメ風の YouTube Shorts 動画を作る Claude Code マルチエージェント構成。

| Agent | 役割 | 使うもの |
|---|---|---|
| 1 scene-planner | 10 シーン構成の JSON を作成 | Claude のみ |
| 2 image-generator | シーン画像 1080x1920 | Gemini (`GEMINI_IMAGE_MODEL`) |
| 3 video-generator | 画像 → 動画クリップ | fal.ai (`FAL_VIDEO_MODEL`、既定 Kling) / 失敗時 ffmpeg スライドショー |
| 4 audio-generator | ナレーション MP3 | ElevenLabs（尺超過時に最大 1.2 倍速） |
| 5 video-editor | 合成・テロップ・60 秒調整 | ffmpeg + Pillow |
| 6 youtube-uploader | Shorts アップロード | YouTube Data API v3 |

Agent2（→3）と Agent4 は並列実行されます。進捗は `runs/<run_id>/state.json` にシーン単位で記録され、失敗しても途中から再開できます。

## セットアップ
```bash
cd anime-video-pipeline
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # API キーを記入
```
- ffmpeg: `brew install ffmpeg` など（無ければ imageio-ffmpeg 同梱版を使用）
- 日本語フォント: macOS / Windows は自動検出。Linux は `fonts-noto-cjk` を入れるか `FONT_PATH` を指定
- YouTube: Google Cloud Console で YouTube Data API v3 を有効化 → OAuth クライアント（デスクトップアプリ）を作成 → `client_secret.json` をこのディレクトリに置く

## 使い方
```bash
cd anime-video-pipeline
claude
> /make-video https://example.com/news/article
> /make-video https://example.com/news/article --upload   # private でアップロードまで
> /make-video 20260924-120000                              # 既存 run を途中から再開
```

API キー無しで動作確認:
```
> /make-video 今日のニュース本文… --mock
```

Claude を使わず残りの工程だけ回す（scenario.json 作成済みの run）:
```bash
python tools/pipeline.py --run <run_id> [--upload]
python tools/state.py show <run_id>
```

## サンプル: 鹿の一人称で歩く森（実写風）
```bash
python tools/pipeline.py --run deer-forest --scenario examples/deer-forest/scenario.json
# → runs/deer-forest/output/final.mp4
```
シナリオの `style` / `motion` を変えると画風（アニメ⇔実写）や動きを切り替えられます。

## トラブルシュート
- モデル廃止・名称変更 → `.env` のモデル ID を変更（コード修正不要）
- 動画 API が正方形出力 → 自動でぼかし背景付き 1080x1920 に変換
- コンテンツポリシーで動画生成失敗 → そのシーンだけ自動でスライドショーに代替
- 音声が長すぎる → 自動で速度調整。無理ならエージェントがナレーションを短縮して再生成
