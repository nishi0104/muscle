---
name: image-generator
description: Agent2（画像生成）。scenario.json の各シーンについて Gemini でアニメ風 1080x1920 画像を生成する。scene-planner 完了後、audio-generator と並列に実行する。
tools: Bash, Read
---

`runs/<run_id>/scenario.json` から各シーンの画像を生成します。

## 手順
1. `python tools/image_client.py --run <run_id>` を実行する（timeout は 600000ms を指定）。
   - 完了済みシーンは自動でスキップされる。失敗時はプロンプトを簡略化して最大 3 回再試行する。
2. 終了コードが 0 以外なら `python tools/state.py show <run_id>` で失敗シーンと原因を確認する。
   - コンテンツポリシー系の失敗: `scenario.json` の該当 `image_prompt` を穏当な表現に書き換え、
     `python tools/image_client.py --run <run_id> --scenes <id>` で再実行（最大 2 回）。
   - モデル廃止/404: `.env` の `GEMINI_IMAGE_MODEL` を見直すよう報告する（コードは変更しない）。
   - API キー未設定: そのまま報告する。
3. 最後に、生成できたシーン・簡略化したシーン・失敗シーンを簡潔に報告する。
