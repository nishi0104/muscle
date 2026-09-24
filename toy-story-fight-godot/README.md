# トイストーリーファイト（Godot版）

ロッツォが主人公の3D格闘ゲーム。Godot 4.4 以降で `project.godot` を開いて F5 で実行。

## 操作
| キー | ゲームパッド | 動作 |
|---|---|---|
| A / D | 十字キー・左スティック | 移動 |
| W | 上 / A | ジャンプ |
| S | 下 / LB | ガード |
| J | X | パンチ |
| K | B | キック |
| L | Y / RB | 必殺技（ゲージMAX時） |
| Enter | Start | 開始・次へ |

## 構成
- `scripts/main.gd` ステージ・ライティング・カメラ・ラウンド進行
- `scripts/fighter.gd` 移動・攻撃判定・プロシージャルアニメーション・CPU思考
- `scripts/models.gd` キャラクターモデル（ロッツォ / ブリキロボ / ゼンマイザウルス / ダーク・ジャック）
- `scripts/hud.gd` 体力・必殺ゲージ・タイトル画面
- `shaders/fur.gdshader` シェル法による毛並み
- `shaders/wood_floor.gdshader`, `shaders/wallpaper.gdshader` 部屋の床と雲の壁紙
