// ゲームロジック：EXP・レベル・ボス・連続週ボーナス
// 状態はすべて記録(records)から導出するので、記録を削除しても整合性が保たれる
(function () {
  'use strict';

  const BOSS_STEP = 2.5;        // ボス = 現在のPR + 2.5kg
  const BOSS_BONUS = 300;       // ボス撃破ボーナスEXP
  const WEEK_BONUS_BASE = 500;  // 週目標達成ボーナス（× 連続週数）
  const WEEK_BONUS_MAX_MULT = 5;
  const DAY = 86400000;

  // キャラクターの成長段階
  const STAGES = [
    { lv: 1, icon: '🥚', title: 'たまご戦士' },
    { lv: 3, icon: '🐣', title: 'ひよっこトレーニー' },
    { lv: 6, icon: '🧒', title: '見習いリフター' },
    { lv: 10, icon: '💪', title: '鋼の戦士' },
    { lv: 15, icon: '🦍', title: 'ゴリラ騎士' },
    { lv: 22, icon: '🐉', title: '鉄の竜' },
    { lv: 30, icon: '👑', title: '筋肉王' },
  ];

  const BOSS_ICONS = ['👹', '👺', '🐲', '💀', '🤖', '👾', '🦂', '🐙'];
  const BOSS_NAMES = ['鉄塊オーガ', '重力の天狗', 'バーベルドラゴン', 'プレートスカル', 'メタルゴーレム', 'ダンベル星人', 'ラックスコーピオン', 'ケーブルクラーケン'];

  function volume(r) {
    return Math.round(r.weight * r.reps * r.sets);
  }

  // レベルLから次のレベルに必要なEXP
  function expToNext(level) {
    return Math.round((5000 * Math.pow(level, 1.3)) / 100) * 100;
  }

  function levelFromExp(total) {
    let level = 1;
    let rest = total;
    while (rest >= expToNext(level)) {
      rest -= expToNext(level);
      level++;
    }
    return { level, expInLevel: rest, expToNext: expToNext(level) };
  }

  function stageFor(level) {
    let s = STAGES[0];
    for (const st of STAGES) if (level >= st.lv) s = st;
    return s;
  }

  // 'YYYY-MM-DD' → UTCミリ秒（タイムゾーンのずれを避けるためUTCで扱う）
  function dateToMs(str) {
    const [y, m, d] = str.split('-').map(Number);
    return Date.UTC(y, m - 1, d);
  }

  function msToDate(ms) {
    return new Date(ms).toISOString().slice(0, 10);
  }

  // 月曜始まりの週キー（その週の月曜日の日付）
  function weekKey(dateStr) {
    const ms = dateToMs(dateStr);
    const dow = (new Date(ms).getUTCDay() + 6) % 7;
    return msToDate(ms - dow * DAY);
  }

  function todayStr() {
    const d = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
  }

  function sortRecords(records) {
    return records.slice().sort((a, b) => (a.date === b.date ? a.createdAt - b.createdAt : a.date < b.date ? -1 : 1));
  }

  function compute(data, today) {
    today = today || todayStr();
    const goal = data.settings.weeklyGoal;
    const records = sortRecords(data.records);

    // --- 基本EXP・PR・ボス撃破 ---
    let baseExp = 0;
    let bossExp = 0;
    const prs = {};
    const kills = {};
    let totalKills = 0;
    for (const r of records) {
      baseExp += volume(r);
      const pr = prs[r.exerciseId];
      if (pr === undefined) {
        prs[r.exerciseId] = r.weight; // 初記録はPR登録のみ
      } else if (r.weight > pr) {
        prs[r.exerciseId] = r.weight;
        kills[r.exerciseId] = (kills[r.exerciseId] || 0) + 1;
        totalKills++;
        bossExp += BOSS_BONUS;
      }
    }

    // --- 週ごとのトレーニング日数と連続達成 ---
    const weekDays = {}; // weekKey -> Set(date)
    for (const r of records) {
      const wk = weekKey(r.date);
      (weekDays[wk] = weekDays[wk] || new Set()).add(r.date);
    }
    const achieved = {}; // weekKey -> その週時点の連続週数
    let weekExp = 0;
    for (const wk of Object.keys(weekDays).sort()) {
      if (weekDays[wk].size < goal) continue;
      const prev = msToDate(dateToMs(wk) - 7 * DAY);
      achieved[wk] = (achieved[prev] || 0) + 1;
      weekExp += WEEK_BONUS_BASE * Math.min(achieved[wk], WEEK_BONUS_MAX_MULT);
    }

    const curWeek = weekKey(today);
    const lastWeek = msToDate(dateToMs(curWeek) - 7 * DAY);
    const streak = achieved[curWeek] || achieved[lastWeek] || 0;
    const curDays = weekDays[curWeek] || new Set();
    const weekDots = [];
    for (let i = 0; i < 7; i++) {
      const d = msToDate(dateToMs(curWeek) + i * DAY);
      weekDots.push({ date: d, done: curDays.has(d), today: d === today });
    }

    const totalExp = baseExp + bossExp + weekExp;
    return Object.assign(
      {
        totalExp, baseExp, bossExp, weekExp,
        prs, kills, totalKills,
        achievedWeeks: achieved,
        streak,
        weekCount: curDays.size,
        weekDots,
        currentWeekAchieved: !!achieved[curWeek],
      },
      levelFromExp(totalExp),
    );
  }

  // 種目ごとの日別最大重量（グラフ用）
  function maxWeightSeries(records, exerciseId) {
    const byDate = {};
    for (const r of records) {
      if (r.exerciseId !== exerciseId) continue;
      byDate[r.date] = Math.max(byDate[r.date] || 0, r.weight);
    }
    return Object.keys(byDate).sort().map((date) => ({ date, value: byDate[date] }));
  }

  function bossFor(exercise, index, pr, killCount) {
    const i = index % BOSS_ICONS.length;
    return {
      icon: BOSS_ICONS[i],
      name: `${BOSS_NAMES[i]}${killCount ? ' ' + toRoman(killCount + 1) : ''}`,
      target: pr === undefined ? null : Math.round((pr + BOSS_STEP) * 10) / 10,
    };
  }

  function toRoman(n) {
    const map = [[10, 'X'], [9, 'IX'], [5, 'V'], [4, 'IV'], [1, 'I']];
    if (n > 39) return String(n);
    let s = '';
    for (const [v, r] of map) while (n >= v) { s += r; n -= v; }
    return s;
  }

  window.Game = {
    BOSS_STEP, BOSS_BONUS, WEEK_BONUS_BASE, WEEK_BONUS_MAX_MULT,
    volume, expToNext, levelFromExp, stageFor, weekKey, todayStr,
    compute, maxWeightSeries, bossFor, sortRecords,
  };
})();
