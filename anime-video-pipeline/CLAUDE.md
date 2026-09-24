# アニメ風ショート動画 自動生成パイプライン

ニュース URL / テキストから、6 体のサブエージェント（`.claude/agents/`）が連携して
YouTube Shorts 用のアニメ風動画を作る。Claude Code はオーケストレーターとして振る舞う。

## 構成（二層アーキテクチャ）
- Claude Code レイヤー: `.claude/agents/*.md`（各エージェントの責務）と本ファイル（進行手順）
- Python ツールレイヤー: `tools/*.py`（外部 API 呼び出し・ffmpeg 処理）。API やモデルを変える時はここだけ直す
- 成果物: `runs/<run_id>/`（source.txt, scenario.json, state.json, images/, clips/, audio/, output/final.mp4）

## DAG
```
scene-planner ─┬─> image-generator ─> video-generator ─┐
               └─> audio-generator ────────────────────┴─> video-editor ─> youtube-uploader
```

## オーケストレーション手順
1. **準備**: run_id を `YYYYMMDD-HHMMSS` 形式で決め、以下を実行。
   - `python tools/state.py init <run_id> --input "<入力>"`
   - `python tools/fetch_article.py --run <run_id> --input "<入力>"`（URL なら本文取得、テキストならそのまま保存）
2. **Agent1**: Agent ツールで `scene-planner` を起動（run_id を渡す）。
3. **Agent2 + Agent4 を並列実行**: 1 つのメッセージで `image-generator` と `audio-generator` を同時に起動する。
4. **Agent3**: image-generator 完了後に `video-generator` を起動（audio-generator の完了は待たなくてよい）。
5. **Agent5**: video-generator と audio-generator の両方が completed になってから `video-editor` を起動。
6. **Agent6**: ユーザーがアップロードを依頼した場合のみ `youtube-uploader` を起動（既定 private）。
7. 最後に final.mp4 のパス、尺、フォールバックしたシーン、（あれば）YouTube URL を報告する。

各ステップの前に `python tools/state.py next <run_id>` で実行可能なエージェントを確認する。
completed のエージェントは再実行しない。

## 途中再開
- `python tools/state.py show <run_id>` で状態確認 → `next` に出たエージェントから再開する。
- 各ツールは完了済みシーンを自動でスキップするので、失敗シーンだけが再実行される。
- 完了済みをやり直す場合は `python tools/state.py reset <run_id> <agent>`（または各ツールに `--force`）。
- scenario.json 完成後は `python tools/pipeline.py --run <run_id>` で Claude 無しでも残りを一括実行できる。

## ルール
- モデル ID・API キーは `.env` で管理し、コードにハードコードしない。
- `PIPELINE_MOCK=1` を付けると外部 API を呼ばずにダミー素材で全工程を検証できる。
- YouTube への公開（public）はユーザーの明示的な指示がある場合のみ。
- Python 3.9 互換を保つ（各ファイル先頭の `from __future__ import annotations` を消さない）。
