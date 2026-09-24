---
description: ニュース URL またはテキストからアニメ風ショート動画を自動生成する
argument-hint: <URL またはテキスト> [--upload] [--mock]
---

入力: $ARGUMENTS

CLAUDE.md の「オーケストレーション手順」に従って、この入力から動画を生成してください。
- `--upload` があれば最後に youtube-uploader まで実行する（公開範囲は private）。
- `--mock` があれば全コマンドを `PIPELINE_MOCK=1` 付きで実行する（外部 API を呼ばない動作確認）。
- 入力が既存の run_id（`runs/` 配下のディレクトリ名）なら、新規作成せずその run を途中から再開する。
