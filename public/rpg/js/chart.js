// Canvas による単一系列の折れ線グラフ（ライブラリなし）
(function () {
  'use strict';

  const PAD = { top: 16, right: 16, bottom: 32, left: 44 };

  function cssVar(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  // 目盛りをきりの良い値にする
  function niceStep(range, count) {
    const raw = range / count;
    const mag = Math.pow(10, Math.floor(Math.log10(raw)));
    const n = raw / mag;
    return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 2.5 ? 2.5 : n <= 5 ? 5 : 10) * mag;
  }

  function LineChart(canvas, tooltip) {
    this.canvas = canvas;
    this.tooltip = tooltip;
    this.points = [];
    this.hover = -1;
    const onMove = (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = (e.touches ? e.touches[0].clientX : e.clientX) - rect.left;
      this.setHover(this.nearest(x));
    };
    canvas.addEventListener('mousemove', onMove);
    canvas.addEventListener('touchstart', onMove, { passive: true });
    canvas.addEventListener('touchmove', onMove, { passive: true });
    canvas.addEventListener('mouseleave', () => this.setHover(-1));
    window.addEventListener('resize', () => this.draw());
  }

  LineChart.prototype.setData = function (series, unit) {
    this.series = series;
    this.unit = unit || 'kg';
    this.hover = -1;
    this.draw();
  };

  LineChart.prototype.nearest = function (x) {
    if (!this.points.length) return -1;
    let best = 0;
    for (let i = 1; i < this.points.length; i++) {
      if (Math.abs(this.points[i].x - x) < Math.abs(this.points[best].x - x)) best = i;
    }
    return best;
  };

  LineChart.prototype.setHover = function (i) {
    if (i === this.hover) return;
    this.hover = i;
    this.draw();
  };

  LineChart.prototype.draw = function () {
    const canvas = this.canvas;
    const series = this.series || [];
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    if (!w || !h) return;
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    this.points = [];
    if (!series.length) { this.tooltip.hidden = true; return; }

    const colLine = cssVar('--chart-line');
    const colGrid = cssVar('--chart-grid');
    const colText = cssVar('--text-muted');
    const colSurface = cssVar('--surface');

    // Y軸の範囲
    const vals = series.map((p) => p.value);
    let min = Math.min.apply(null, vals);
    let max = Math.max.apply(null, vals);
    if (min === max) { min -= 5; max += 5; }
    const step = niceStep(max - min, 4);
    min = Math.max(0, Math.floor(min / step) * step);
    max = Math.ceil(max / step) * step;

    const plotW = w - PAD.left - PAD.right;
    const plotH = h - PAD.top - PAD.bottom;
    const xAt = (i) => PAD.left + (series.length === 1 ? plotW / 2 : (plotW * i) / (series.length - 1));
    const yAt = (v) => PAD.top + plotH - ((v - min) / (max - min)) * plotH;

    // グリッドとY目盛り
    ctx.font = '11px system-ui, sans-serif';
    ctx.fillStyle = colText;
    ctx.strokeStyle = colGrid;
    ctx.lineWidth = 1;
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (let v = min; v <= max + 1e-9; v += step) {
      const y = Math.round(yAt(v)) + 0.5;
      ctx.beginPath(); ctx.moveTo(PAD.left, y); ctx.lineTo(w - PAD.right, y); ctx.stroke();
      ctx.fillText(String(Math.round(v * 10) / 10), PAD.left - 6, y);
    }

    // X目盛り（間引いて最大5つ）
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    const every = Math.max(1, Math.ceil(series.length / 5));
    series.forEach((p, i) => {
      if (i % every !== 0 && i !== series.length - 1) return;
      if (i !== series.length - 1 && series.length - 1 - i < every) return; // 最後のラベルとの重なり防止
      ctx.fillText(p.date.slice(5).replace('-', '/'), xAt(i), h - PAD.bottom + 8);
    });

    this.points = series.map((p, i) => ({ x: xAt(i), y: yAt(p.value), p }));

    // ホバー時の縦線
    if (this.hover >= 0) {
      const hx = Math.round(this.points[this.hover].x) + 0.5;
      ctx.strokeStyle = colText;
      ctx.setLineDash([3, 3]);
      ctx.beginPath(); ctx.moveTo(hx, PAD.top); ctx.lineTo(hx, PAD.top + plotH); ctx.stroke();
      ctx.setLineDash([]);
    }

    // 折れ線
    ctx.strokeStyle = colLine;
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.beginPath();
    this.points.forEach((pt, i) => (i ? ctx.lineTo(pt.x, pt.y) : ctx.moveTo(pt.x, pt.y)));
    ctx.stroke();

    // マーカー（面色のリングで線から浮かせる）
    this.points.forEach((pt, i) => {
      const r = i === this.hover ? 6 : 4;
      ctx.beginPath(); ctx.arc(pt.x, pt.y, r + 2, 0, Math.PI * 2); ctx.fillStyle = colSurface; ctx.fill();
      ctx.beginPath(); ctx.arc(pt.x, pt.y, r, 0, Math.PI * 2); ctx.fillStyle = colLine; ctx.fill();
    });

    // 最新値だけ直接ラベル
    const last = this.points[this.points.length - 1];
    ctx.fillStyle = cssVar('--text');
    ctx.font = 'bold 12px system-ui, sans-serif';
    ctx.textAlign = last.x > w - 60 ? 'right' : 'left';
    ctx.textBaseline = 'bottom';
    ctx.fillText(`${last.p.value}${this.unit}`, last.x + (ctx.textAlign === 'right' ? -8 : 8), last.y - 6);

    // ツールチップ
    const tip = this.tooltip;
    if (this.hover >= 0) {
      const pt = this.points[this.hover];
      tip.innerHTML = `<strong>${pt.p.value}${this.unit}</strong><span>${pt.p.date}</span>`;
      tip.hidden = false;
      const tw = tip.offsetWidth;
      tip.style.left = Math.min(Math.max(pt.x - tw / 2, 0), w - tw) + 'px';
      tip.style.top = Math.max(pt.y - tip.offsetHeight - 12, 0) + 'px';
    } else {
      tip.hidden = true;
    }
  };

  window.LineChart = LineChart;
})();
