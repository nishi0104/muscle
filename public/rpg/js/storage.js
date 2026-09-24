// localStorage への保存・読み込み
(function () {
  'use strict';

  const KEY = 'muscleQuest.v1';

  const DEFAULT_EXERCISES = ['ベンチプレス', 'スクワット', 'デッドリフト', 'ショルダープレス', 'ラットプルダウン'];

  function uid() {
    return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
  }

  function defaultData() {
    return {
      exercises: DEFAULT_EXERCISES.map((name) => ({ id: uid(), name })),
      records: [], // { id, date:'YYYY-MM-DD', exerciseId, weight, reps, sets, createdAt }
      settings: { weeklyGoal: 3 },
    };
  }

  function load() {
    try {
      const raw = localStorage.getItem(KEY);
      if (!raw) return defaultData();
      const data = JSON.parse(raw);
      const base = defaultData();
      return {
        exercises: Array.isArray(data.exercises) ? data.exercises : base.exercises,
        records: Array.isArray(data.records) ? data.records : [],
        settings: Object.assign(base.settings, data.settings),
      };
    } catch (e) {
      console.warn('データの読み込みに失敗しました', e);
      return defaultData();
    }
  }

  function save(data) {
    try {
      localStorage.setItem(KEY, JSON.stringify(data));
      return true;
    } catch (e) {
      console.warn('データの保存に失敗しました', e);
      return false;
    }
  }

  function clear() {
    try { localStorage.removeItem(KEY); } catch (e) { /* noop */ }
  }

  window.MQStore = { load, save, clear, uid };
})();
