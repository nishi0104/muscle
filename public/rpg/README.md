# マッスルクエスト（筋トレ育成RPG）

フレームワークなしの HTML/CSS/JS 製。データは localStorage（キー: `muscleQuest.v1`）に保存。

## 起動
- `public/rpg/index.html` をブラウザで直接開く
- またはサーバー経由: `npx serve public/rpg` / 既存の Express 起動時は `/public/rpg/`

## ルール
- EXP = 重量 × 回数 × セット数
- レベルLから次までの必要EXP = 5000 × L^1.3（100単位で丸め）
- ボス = 種目ごとの現在PR + 2.5kg。PRを更新すると撃破（+300 EXP）
- 週目標（初期値3日/週・月曜始まり）達成で +500 EXP × 連続週数（最大5倍）
- 状態はすべて記録から再計算するため、記録を削除するとEXPも取り消される
