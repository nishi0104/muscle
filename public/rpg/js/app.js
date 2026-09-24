// 画面描画とイベント処理
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  let data = MQStore.load();
  let state = Game.compute(data);
  let chart;

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  }

  function exName(id) {
    const ex = data.exercises.find((e) => e.id === id);
    return ex ? ex.name : '(削除された種目)';
  }

  function fmt(n) {
    return n.toLocaleString('ja-JP');
  }

  function persist() {
    if (!MQStore.save(data)) toast('⚠️ 保存に失敗しました（ブラウザの保存領域を確認してください）');
    state = Game.compute(data);
    render();
  }

  // ---------- 描画 ----------
  function render() {
    renderHome();
    renderExerciseSelects();
    renderHistory();
    renderBosses();
    renderChart();
    updatePreview();
  }

  function renderHome() {
    const st = Game.stageFor(state.level);
    $('avatar').textContent = st.icon;
    $('heroTitle').textContent = st.title;
    $('level').textContent = state.level;
    $('headerLv').textContent = `Lv ${state.level}`;
    const pct = Math.min(100, (state.expInLevel / state.expToNext) * 100);
    $('expFill').style.width = pct + '%';
    $('expBarWrap').setAttribute('aria-valuenow', Math.round(pct));
    $('expText').textContent = `${fmt(state.expInLevel)} / ${fmt(state.expToNext)} EXP`;
    $('totalExp').textContent = fmt(state.totalExp);

    const goal = data.settings.weeklyGoal;
    $('weekCount').textContent = `${state.weekCount}/${goal}`;
    $('streakWeeks').textContent = state.streak;
    $('bossKills').textContent = state.totalKills;
    $('weeklyGoal').value = String(goal);

    const labels = ['月', '火', '水', '木', '金', '土', '日'];
    $('weekDots').innerHTML = state.weekDots
      .map((d, i) => `<div class="dot${d.done ? ' done' : ''}${d.today ? ' today' : ''}" title="${d.date}">${d.done ? '✔' : labels[i]}</div>`)
      .join('');
    const left = goal - state.weekCount;
    $('weekHint').textContent = state.currentWeekAchieved
      ? `今週の目標達成！ボーナス獲得済み 🎉`
      : `あと${left}日で週ボーナス +${fmt(Game.WEEK_BONUS_BASE * Math.min(state.streak + 1, Game.WEEK_BONUS_MAX_MULT))} EXP`;
    document.querySelectorAll('.bossBonus').forEach((el) => (el.textContent = Game.BOSS_BONUS));
    $('weekBonusBase').textContent = Game.WEEK_BONUS_BASE;
  }

  function renderExerciseSelects() {
    const opts = data.exercises.map((e) => `<option value="${e.id}">${escapeHtml(e.name)}</option>`).join('');
    for (const id of ['fExercise', 'graphExercise']) {
      const sel = $(id);
      const prev = sel.value;
      sel.innerHTML = opts;
      if (data.exercises.some((e) => e.id === prev)) sel.value = prev;
    }
  }

  function renderHistory() {
    const recs = Game.sortRecords(data.records).reverse().slice(0, 30);
    $('history').innerHTML = recs.length
      ? recs.map((r) => `
        <li>
          <div>
            <div class="h-main">${escapeHtml(exName(r.exerciseId))} <strong>${r.weight}kg</strong> × ${r.reps}回 × ${r.sets}セット</div>
            <div class="h-sub">${r.date} ・ +${fmt(Game.volume(r))} EXP</div>
          </div>
          <button class="icon-btn" data-del="${r.id}" aria-label="削除">🗑</button>
        </li>`).join('')
      : '<li class="empty">まだ記録がありません。最初の一歩を踏み出そう！</li>';
  }

  function renderBosses() {
    $('bossList').innerHTML = data.exercises.map((ex, i) => {
      const pr = state.prs[ex.id];
      const kills = state.kills[ex.id] || 0;
      const boss = Game.bossFor(ex, i, pr, kills);
      const body = pr === undefined
        ? `<p class="boss-wait">まずは記録して挑戦者登録しよう</p>`
        : `<div class="boss-hp"><div class="boss-hp-fill" style="width:${Math.min(100, (pr / boss.target) * 100)}%"></div></div>
           <div class="boss-meta"><span>現在のPR <strong>${pr}kg</strong></span><span>撃破条件 <strong>${boss.target}kg</strong>以上</span></div>`;
      return `
        <div class="card boss">
          <div class="boss-head">
            <div class="boss-icon">${pr === undefined ? '❔' : boss.icon}</div>
            <div>
              <div class="boss-ex">${escapeHtml(ex.name)}</div>
              <div class="boss-name">${pr === undefined ? '？？？' : escapeHtml(boss.name)}</div>
            </div>
            <div class="boss-kills">🏆 ${kills}</div>
          </div>
          ${body}
          <div class="boss-actions">
            <button class="btn small" data-challenge="${ex.id}">挑戦する</button>
            ${i >= 3 ? `<button class="btn small ghost" data-del-ex="${ex.id}">種目を削除</button>` : ''}
          </div>
        </div>`;
    }).join('');
  }

  function renderChart() {
    const exId = $('graphExercise').value;
    const series = Game.maxWeightSeries(data.records, exId);
    $('chartEmpty').hidden = series.length > 0;
    $('chartWrap').hidden = series.length === 0;
    chart.setData(series, 'kg');
  }

  function updatePreview() {
    const w = parseFloat($('fWeight').value) || 0;
    const r = parseInt($('fReps').value, 10) || 0;
    const s = parseInt($('fSets').value, 10) || 0;
    const exId = $('fExercise').value;
    const pr = state.prs[exId];
    let text = `獲得予定 ${fmt(Math.round(w * r * s))} EXP`;
    if (pr !== undefined && w > pr) text += ` ＋ ボス撃破 ${Game.BOSS_BONUS} EXP ⚔️`;
    else if (pr !== undefined) text += `（PR ${pr}kg / ボス ${Math.round((pr + Game.BOSS_STEP) * 10) / 10}kg）`;
    $('expPreview').textContent = text;
  }

  // ---------- 演出 ----------
  const queue = [];
  function showModal(html) {
    queue.push(html);
    if (queue.length === 1) openNext();
  }
  function openNext() {
    if (!queue.length) return;
    $('modal').innerHTML = queue[0] + '<button class="btn primary" id="modalOk">OK</button>';
    $('overlay').hidden = false;
    $('modalOk').focus();
  }
  function closeModal() {
    queue.shift();
    $('overlay').hidden = true;
    if (queue.length) setTimeout(openNext, 150);
  }

  let toastTimer;
  function toast(msg) {
    const t = $('toast');
    t.textContent = msg;
    t.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => (t.hidden = true), 2500);
  }

  // ---------- イベント ----------
  function switchView(name) {
    document.querySelectorAll('.view').forEach((v) => v.classList.toggle('active', v.id === 'view-' + name));
    document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('active', t.dataset.view === name));
    window.scrollTo(0, 0);
    if (name === 'graph') renderChart();
  }

  function onSubmitLog(e) {
    e.preventDefault();
    const rec = {
      id: MQStore.uid(),
      date: $('fDate').value,
      exerciseId: $('fExercise').value,
      weight: Math.round(parseFloat($('fWeight').value) * 100) / 100,
      reps: parseInt($('fReps').value, 10),
      sets: parseInt($('fSets').value, 10),
      createdAt: Date.now(),
    };
    if (!rec.date || !rec.exerciseId || !(rec.weight > 0) || !(rec.reps > 0) || !(rec.sets > 0)) {
      toast('入力内容を確認してください');
      return;
    }
    const before = state;
    data.records.push(rec);
    persist();
    const after = state;

    const gained = after.totalExp - before.totalExp;
    toast(`+${fmt(gained)} EXP 獲得！`);

    const pr = before.prs[rec.exerciseId];
    if (pr !== undefined && rec.weight > pr) {
      const idx = data.exercises.findIndex((x) => x.id === rec.exerciseId);
      const boss = Game.bossFor(null, idx, pr, (before.kills[rec.exerciseId] || 0));
      showModal(`<div class="fx fx-boss">${boss.icon}</div><h3>ボス撃破！</h3>
        <p>${escapeHtml(boss.name)}を倒した！<br>${escapeHtml(exName(rec.exerciseId))} PR ${pr}kg → <strong>${rec.weight}kg</strong></p>
        <p class="bonus">+${Game.BOSS_BONUS} EXP</p><p class="hint">次のボス：${Math.round((rec.weight + Game.BOSS_STEP) * 10) / 10}kg</p>`);
    }
    const newWeeks = Object.keys(after.achievedWeeks).filter((w) => !before.achievedWeeks[w]);
    if (newWeeks.length) {
      const n = after.achievedWeeks[newWeeks[newWeeks.length - 1]];
      showModal(`<div class="fx">🔥</div><h3>週目標達成！</h3><p>${n}週連続達成中</p>
        <p class="bonus">+${fmt(Game.WEEK_BONUS_BASE * Math.min(n, Game.WEEK_BONUS_MAX_MULT))} EXP</p>`);
    }
    if (after.level > before.level) {
      const st = Game.stageFor(after.level);
      const evolved = st !== Game.stageFor(before.level);
      showModal(`<div class="fx fx-level">${st.icon}</div><h3 class="lvup">LEVEL UP!</h3>
        <p class="lv-change">Lv ${before.level} → <strong>Lv ${after.level}</strong></p>
        ${evolved ? `<p>称号「${st.title}」に進化した！</p>` : '<p>さらに強くなった！</p>'}`);
    }

    $('fWeight').value = '';
    $('fReps').value = '';
    updatePreview();
  }

  function onSubmitExercise(e) {
    e.preventDefault();
    const name = $('exName').value.trim();
    if (!name) return;
    if (data.exercises.some((x) => x.name === name)) { toast('同じ名前の種目があります'); return; }
    const ex = { id: MQStore.uid(), name };
    data.exercises.push(ex);
    persist();
    $('fExercise').value = ex.id;
    $('exName').value = '';
    updatePreview();
    toast(`「${name}」を追加しました`);
  }

  function init() {
    chart = new LineChart($('chart'), $('tooltip'));
    $('fDate').value = Game.todayStr();

    document.querySelectorAll('.tab').forEach((t) => t.addEventListener('click', () => switchView(t.dataset.view)));
    $('logForm').addEventListener('submit', onSubmitLog);
    $('exForm').addEventListener('submit', onSubmitExercise);
    ['fWeight', 'fReps', 'fSets', 'fExercise'].forEach((id) => $(id).addEventListener('input', updatePreview));
    $('graphExercise').addEventListener('change', renderChart);
    $('weeklyGoal').addEventListener('change', (e) => {
      data.settings.weeklyGoal = parseInt(e.target.value, 10);
      persist();
    });

    $('history').addEventListener('click', (e) => {
      const id = e.target.closest('[data-del]')?.dataset.del;
      if (!id || !confirm('この記録を削除しますか？（獲得EXPも取り消されます）')) return;
      data.records = data.records.filter((r) => r.id !== id);
      persist();
    });

    $('bossList').addEventListener('click', (e) => {
      const ch = e.target.closest('[data-challenge]')?.dataset.challenge;
      if (ch) {
        switchView('log');
        $('fExercise').value = ch;
        const pr = state.prs[ch];
        if (pr !== undefined) $('fWeight').value = Math.round((pr + Game.BOSS_STEP) * 10) / 10;
        updatePreview();
        $('fReps').focus();
        return;
      }
      const del = e.target.closest('[data-del-ex]')?.dataset.delEx;
      if (del && confirm(`「${exName(del)}」とその記録をすべて削除しますか？`)) {
        data.exercises = data.exercises.filter((x) => x.id !== del);
        data.records = data.records.filter((r) => r.exerciseId !== del);
        persist();
      }
    });

    $('resetBtn').addEventListener('click', () => {
      if (!confirm('全データを削除して最初からやり直しますか？')) return;
      MQStore.clear();
      data = MQStore.load();
      persist();
      toast('データをリセットしました');
    });

    $('overlay').addEventListener('click', (e) => {
      if (e.target.id === 'overlay' || e.target.id === 'modalOk') closeModal();
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && !$('overlay').hidden) closeModal();
    });

    render();
  }

  init();
})();
