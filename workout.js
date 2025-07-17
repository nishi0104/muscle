// Description:
//   筋トレメニュー提案機能
//   ユーザーが鍛えたい部位を入力すると、対応するトレーニングメニューを提案します
//
// Commands:
//   腕トレ - 腕のトレーニングメニューを提案
//   腹筋 - 腹筋のトレーニングメニューを提案
//   脚 - 脚のトレーニングメニューを提案
//   胸 - 胸のトレーニングメニューを提案
//   背中 - 背中のトレーニングメニューを提案
//   肩 - 肩のトレーニングメニューを提案
//   全身 - 全身のトレーニングメニューを提案
//   筋トレメニュー - 利用可能な部位一覧を表示

'use strict';

module.exports = (robot) => {
  // 筋トレメニューデータ
  const workoutMenus = {
    腕: {
      name: '腕トレーニング',
      exercises: [
        { name: 'ダンベルカール', reps: '12-15回', sets: '3セット', description: '上腕二頭筋を鍛える基本種目' },
        { name: 'トライセプスディップス', reps: '10-12回', sets: '3セット', description: '上腕三頭筋を効果的に鍛える' },
        { name: 'ハンマーカール', reps: '12-15回', sets: '3セット', description: '前腕筋群も同時に鍛える' },
        { name: 'プッシュアップ（ナロー）', reps: '8-12回', sets: '3セット', description: '腕立て伏せの狭い手幅バージョン' },
        { name: 'アームカール（チューブ）', reps: '15-20回', sets: '2セット', description: '自宅でも手軽にできる' }
      ]
    },
    腹筋: {
      name: '腹筋トレーニング',
      exercises: [
        { name: 'クランチ', reps: '15-20回', sets: '3セット', description: '腹直筋上部を集中的に鍛える' },
        { name: 'プランク', reps: '30-60秒', sets: '3セット', description: '体幹全体を強化する' },
        { name: 'レッグレイズ', reps: '12-15回', sets: '3セット', description: '腹直筋下部に効果的' },
        { name: 'バイシクルクランチ', reps: '左右10回ずつ', sets: '3セット', description: '腹斜筋も同時に鍛える' },
        { name: 'マウンテンクライマー', reps: '20-30回', sets: '3セット', description: '有酸素効果も期待できる' }
      ]
    },
    脚: {
      name: '脚トレーニング',
      exercises: [
        { name: 'スクワット', reps: '15-20回', sets: '3セット', description: '太ももとお尻を総合的に鍛える' },
        { name: 'ランジ', reps: '片足10-12回', sets: '3セット', description: 'バランス力も向上させる' },
        { name: 'カーフレイズ', reps: '20-25回', sets: '3セット', description: 'ふくらはぎを集中的に鍛える' },
        { name: 'ウォールシット', reps: '30-45秒', sets: '3セット', description: '壁を使った効果的な太もも強化' },
        { name: 'ステップアップ', reps: '片足8-10回', sets: '3セット', description: '階段や台を使った機能的運動' }
      ]
    },
    胸: {
      name: '胸トレーニング',
      exercises: [
        { name: 'プッシュアップ', reps: '10-15回', sets: '3セット', description: '胸筋を鍛える基本種目' },
        { name: 'ダンベルフライ', reps: '12-15回', sets: '3セット', description: '胸筋の幅を広げる' },
        { name: 'インクラインプッシュアップ', reps: '12-15回', sets: '3セット', description: '胸筋上部を重点的に' },
        { name: 'ダンベルプレス', reps: '10-12回', sets: '3セット', description: '胸筋全体を効果的に鍛える' },
        { name: 'チェストディップス', reps: '8-12回', sets: '3セット', description: '胸筋下部に効果的' }
      ]
    },
    背中: {
      name: '背中トレーニング',
      exercises: [
        { name: 'プルアップ', reps: '5-10回', sets: '3セット', description: '背中全体を鍛える王道種目' },
        { name: 'バックエクステンション', reps: '12-15回', sets: '3セット', description: '脊柱起立筋を強化' },
        { name: 'ベントオーバーロウ', reps: '10-12回', sets: '3セット', description: '広背筋と僧帽筋を鍛える' },
        { name: 'スーパーマン', reps: '15-20回', sets: '3セット', description: '自重で背中を鍛える' },
        { name: 'ラットプルダウン', reps: '12-15回', sets: '3セット', description: '機器を使った背中強化' }
      ]
    },
    肩: {
      name: '肩トレーニング',
      exercises: [
        { name: 'ショルダープレス', reps: '10-12回', sets: '3セット', description: '肩の前部と中部を鍛える' },
        { name: 'サイドレイズ', reps: '12-15回', sets: '3セット', description: '肩の幅を広げる' },
        { name: 'フロントレイズ', reps: '12-15回', sets: '3セット', description: '肩の前部を集中的に' },
        { name: 'リアレイズ', reps: '15-20回', sets: '3セット', description: '肩の後部を鍛える' },
        { name: 'アップライトロウ', reps: '12-15回', sets: '3セット', description: '肩と僧帽筋を同時に鍛える' }
      ]
    },
    全身: {
      name: '全身トレーニング',
      exercises: [
        { name: 'バーピー', reps: '8-12回', sets: '3セット', description: '全身を使った高強度運動' },
        { name: 'デッドリフト', reps: '8-10回', sets: '3セット', description: '背中・脚・体幹を総合的に鍛える' },
        { name: 'クリーン&プレス', reps: '6-8回', sets: '3セット', description: '全身の連動性を高める' },
        { name: 'スラスター', reps: '10-12回', sets: '3セット', description: 'スクワットからプレスへの複合運動' },
        { name: 'ケトルベルスイング', reps: '15-20回', sets: '3セット', description: '爆発的パワーを養う' }
      ]
    }
  };

  // 部位別メニュー提案
  Object.keys(workoutMenus).forEach(part => {
    const patterns = [
      new RegExp(`^${part}トレ$`, 'i'),
      new RegExp(`^${part}$`, 'i'),
      new RegExp(`^${part}.*トレーニング$`, 'i')
    ];
    
    patterns.forEach(pattern => {
      robot.respond(pattern, (res) => {
        const menu = workoutMenus[part];
        let response = `💪 ${menu.name}メニュー 💪\n\n`;
        
        menu.exercises.forEach((exercise, index) => {
          response += `${index + 1}. ${exercise.name}\n`;
          response += `   ${exercise.reps} × ${exercise.sets}\n`;
          response += `   ${exercise.description}\n\n`;
        });
        
        response += `頑張って${part}を鍛えましょう！🔥`;
        res.send(response);
      });
    });
  });

  // メニュー一覧表示
  robot.respond(/筋トレメニュー$/i, (res) => {
    let response = '🏋️‍♂️ 利用可能な筋トレメニュー 🏋️‍♂️\n\n';
    response += '以下の部位名を入力してください：\n';
    response += '• 腕トレ - 腕のトレーニング\n';
    response += '• 腹筋 - 腹筋のトレーニング\n';
    response += '• 脚 - 脚のトレーニング\n';
    response += '• 胸 - 胸のトレーニング\n';
    response += '• 背中 - 背中のトレーニング\n';
    response += '• 肩 - 肩のトレーニング\n';
    response += '• 全身 - 全身のトレーニング\n\n';
    response += '例：「腕トレ」と入力すると腕のメニューが表示されます！';
    
    res.send(response);
  });

  // ランダムメニュー提案
  robot.respond(/おすすめ$/i, (res) => {
    const parts = Object.keys(workoutMenus);
    const randomPart = parts[Math.floor(Math.random() * parts.length)];
    const menu = workoutMenus[randomPart];
    const randomExercise = menu.exercises[Math.floor(Math.random() * menu.exercises.length)];
    
    let response = `🎯 今日のおすすめトレーニング 🎯\n\n`;
    response += `${randomExercise.name}\n`;
    response += `${randomExercise.reps} × ${randomExercise.sets}\n`;
    response += `${randomExercise.description}\n\n`;
    response += `${randomPart}を鍛えてみませんか？💪`;
    
    res.send(response);
  });

  // モチベーション応援メッセージ
  robot.respond(/疲れた$/i, (res) => {
    const motivationMessages = [
      '💪 その疲れは成長の証拠です！',
      '🔥 継続は力なり！明日も頑張りましょう！',
      '⭐ 疲れた時こそ成長のチャンス！',
      '💯 今日もよく頑張りました！休息も大切です！',
      '🏆 あなたの努力は必ず結果に繋がります！'
    ];
    
    const randomMessage = motivationMessages[Math.floor(Math.random() * motivationMessages.length)];
    res.send(randomMessage);
  });

  // トレーニングのコツ
  robot.respond(/コツ$/i, (res) => {
    const tips = [
      '📝 正しいフォームを心がけましょう！',
      '💧 水分補給を忘れずに！',
      '😴 十分な睡眠と栄養も大切です！',
      '📅 無理せず継続することが最重要！',
      '🎯 目標を設定してモチベーションを保ちましょう！'
    ];
    
    const randomTip = tips[Math.floor(Math.random() * tips.length)];
    res.send(randomTip);
  });
};