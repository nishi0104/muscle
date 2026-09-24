# 体力ゲージ・タイマー・必殺ゲージ・メッセージ・タイトル画面。
extends CanvasLayer

const BAR_W := 520.0
const GOLD := Color(1.0, 0.82, 0.25)
const PINK := Color(1.0, 0.3, 0.55)

var font: SystemFont
var fight_ui: Control
var p_hp: ColorRect
var p_dmg: ColorRect
var e_hp: ColorRect
var e_dmg: ColorRect
var p_meter: ColorRect
var e_meter: ColorRect
var p_meter_l: Label
var e_meter_l: Label
var e_name: Label
var timer_l: Label
var msg_l: Label
var sub_l: Label
var pips := [[], []]
var title_box: Control
var prompt_l: Label
var overlay: ColorRect
var ov_title: Label
var ov_sub: Label
var msg_time := 0.0
var p_dmg_pct := 1.0
var e_dmg_pct := 1.0
var blink := 0.0


func _ready() -> void:
	font = SystemFont.new()
	font.font_names = PackedStringArray(["Hiragino Sans", "Hiragino Kaku Gothic ProN", "Yu Gothic", "Meiryo",
		"Noto Sans CJK JP", "Noto Sans JP", "WenQuanYi Zen Hei", "sans-serif"])
	font.font_weight = 800
	var root := Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var th := Theme.new()
	th.default_font = font
	root.theme = th
	add_child(root)

	fight_ui = Control.new()
	fight_ui.set_anchors_preset(Control.PRESET_FULL_RECT)
	fight_ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(fight_ui)
	var pb := _bar(40.0)
	p_dmg = pb[0]
	p_hp = pb[1]
	var eb := _bar(720.0)
	e_dmg = eb[0]
	e_hp = eb[1]
	_label(fight_ui, "ロッツォ", 26, Color.WHITE, Vector2(40, 72), Vector2(400, 40), HORIZONTAL_ALIGNMENT_LEFT)
	e_name = _label(fight_ui, "", 26, Color.WHITE, Vector2(840, 72), Vector2(400, 40), HORIZONTAL_ALIGNMENT_RIGHT)
	var tb := ColorRect.new()
	tb.color = Color(0, 0, 0, 0.55)
	tb.position = Vector2(588, 22)
	tb.size = Vector2(104, 70)
	fight_ui.add_child(tb)
	timer_l = _label(fight_ui, "99", 54, Color.WHITE, Vector2(588, 18), Vector2(104, 70), HORIZONTAL_ALIGNMENT_CENTER)
	for side in 2:
		for i in 2:
			var c := ColorRect.new()
			c.size = Vector2(18, 18)
			c.position = Vector2(540 - i * 26, 80) if side == 0 else Vector2(722 + i * 26, 80)
			c.rotation = PI / 4
			c.pivot_offset = Vector2(9, 9)
			fight_ui.add_child(c)
			pips[side].append(c)
	var pm := _meter(40.0)
	p_meter = pm[0]
	p_meter_l = pm[1]
	var em := _meter(1000.0)
	e_meter = em[0]
	e_meter_l = em[1]
	e_meter_l.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT

	msg_l = _label(root, "", 110, GOLD, Vector2(0, 250), Vector2(1280, 150), HORIZONTAL_ALIGNMENT_CENTER)
	msg_l.pivot_offset = Vector2(640, 75)
	sub_l = _label(root, "", 36, Color.WHITE, Vector2(0, 390), Vector2(1280, 60), HORIZONTAL_ALIGNMENT_CENTER)

	overlay = ColorRect.new()
	overlay.color = Color(0, 0, 0, 0.6)
	overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(overlay)
	ov_title = _label(overlay, "", 110, GOLD, Vector2(0, 200), Vector2(1280, 150), HORIZONTAL_ALIGNMENT_CENTER)
	ov_sub = _label(overlay, "", 36, Color.WHITE, Vector2(0, 360), Vector2(1280, 60), HORIZONTAL_ALIGNMENT_CENTER)
	_label(overlay, "Enter / P で続ける", 28, Color.WHITE, Vector2(0, 470), Vector2(1280, 50), HORIZONTAL_ALIGNMENT_CENTER)
	overlay.visible = false

	title_box = Control.new()
	title_box.set_anchors_preset(Control.PRESET_FULL_RECT)
	title_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(title_box)
	_label(title_box, "トイストーリー", 88, GOLD, Vector2(40, 150), Vector2(700, 130), HORIZONTAL_ALIGNMENT_CENTER)
	_label(title_box, "ファイト", 140, PINK, Vector2(40, 260), Vector2(700, 180), HORIZONTAL_ALIGNMENT_CENTER)
	prompt_l = _label(title_box, "PRESS ENTER / P", 34, Color.WHITE, Vector2(40, 470), Vector2(700, 50), HORIZONTAL_ALIGNMENT_CENTER)
	_label(title_box, "移動 A/D　ジャンプ W　ガード S　パンチ J　キック K　必殺技 L（ゲージMAX）", 20,
		Color(1, 1, 1, 0.85), Vector2(0, 640), Vector2(1280, 40), HORIZONTAL_ALIGNMENT_CENTER)


func _label(parent: Control, text: String, size: int, color: Color, pos: Vector2, sz: Vector2, align: HorizontalAlignment) -> Label:
	var l := Label.new()
	l.text = text
	l.position = pos
	l.size = sz
	l.horizontal_alignment = align
	l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_outline_color", Color(0.05, 0.02, 0.08))
	l.add_theme_constant_override("outline_size", maxi(6, size / 7))
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.5))
	l.add_theme_constant_override("shadow_offset_y", 4)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(l)
	return l


func _bar(x: float) -> Array:
	var frame := ColorRect.new()
	frame.color = Color(0.05, 0.03, 0.08, 0.85)
	frame.position = Vector2(x - 4, 28)
	frame.size = Vector2(BAR_W + 8, 40)
	fight_ui.add_child(frame)
	var back := ColorRect.new()
	back.color = Color(0.35, 0.02, 0.05)
	back.position = Vector2(x, 32)
	back.size = Vector2(BAR_W, 32)
	fight_ui.add_child(back)
	var dmg := ColorRect.new()
	dmg.color = Color(1, 0.25, 0.2)
	dmg.position = back.position
	dmg.size = back.size
	fight_ui.add_child(dmg)
	var hp := ColorRect.new()
	hp.color = GOLD
	hp.position = back.position
	hp.size = back.size
	fight_ui.add_child(hp)
	var shine := ColorRect.new()
	shine.color = Color(1, 1, 1, 0.22)
	shine.position = Vector2(x, 34)
	shine.size = Vector2(BAR_W, 10)
	fight_ui.add_child(shine)
	return [dmg, hp]


func _meter(x: float) -> Array:
	var back := ColorRect.new()
	back.color = Color(0, 0, 0, 0.6)
	back.position = Vector2(x, 670)
	back.size = Vector2(240, 18)
	fight_ui.add_child(back)
	var fill := ColorRect.new()
	fill.position = back.position
	fill.size = Vector2(0, 18)
	fight_ui.add_child(fill)
	var l := _label(fight_ui, "SPECIAL", 18, Color.WHITE, Vector2(x, 638), Vector2(240, 30), HORIZONTAL_ALIGNMENT_LEFT)
	return [fill, l]


func _process(delta: float) -> void:
	blink += delta
	if msg_time > 0.0:
		msg_time -= delta
		if msg_time <= 0.0:
			msg_l.text = ""
			sub_l.text = ""
	prompt_l.modulate.a = 0.35 + 0.65 * absf(sin(blink * 3.0))


func show_title(on: bool) -> void:
	title_box.visible = on
	fight_ui.visible = not on
	overlay.visible = false
	msg_l.text = ""
	sub_l.text = ""


func show_msg(text: String, dur := 0.0, sub := "", color := GOLD) -> void:
	msg_l.text = text
	sub_l.text = sub
	msg_l.add_theme_color_override("font_color", color)
	msg_time = dur
	msg_l.scale = Vector2(1.8, 1.8)
	msg_l.modulate.a = 0.0
	var tw := create_tween().set_parallel(true).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.tween_property(msg_l, "scale", Vector2.ONE, 0.35)
	tw.tween_property(msg_l, "modulate:a", 1.0, 0.2)


func show_overlay(title: String, sub: String, color := GOLD) -> void:
	msg_l.text = ""
	sub_l.text = ""
	overlay.visible = true
	ov_title.text = title
	ov_title.add_theme_color_override("font_color", color)
	ov_sub.text = sub


func set_enemy_name(n: String) -> void:
	e_name.text = n


func update_fight(p, e, timer: float, wins: Array, delta: float) -> void:
	var pp: float = p.hp / p.max_hp
	var ep: float = e.hp / e.max_hp
	p_dmg_pct = maxf(pp, move_toward(p_dmg_pct, pp, delta * 0.5))
	e_dmg_pct = maxf(ep, move_toward(e_dmg_pct, ep, delta * 0.5))
	p_hp.size.x = BAR_W * pp
	p_dmg.size.x = BAR_W * p_dmg_pct
	e_hp.size.x = BAR_W * ep
	e_hp.position.x = 720.0 + BAR_W * (1.0 - ep)
	e_dmg.size.x = BAR_W * e_dmg_pct
	e_dmg.position.x = 720.0 + BAR_W * (1.0 - e_dmg_pct)
	timer_l.text = "%02d" % int(ceil(timer))
	for side in 2:
		for i in 2:
			pips[side][i].color = PINK if i < wins[side] else Color(0, 0, 0, 0.45)
	var flash := Color(1, 0.35, 0.6) if fmod(blink, 0.3) < 0.15 else Color.WHITE
	p_meter.size.x = 2.4 * p.meter
	p_meter.color = flash if p.meter >= 100.0 else Color(0.3, 0.75, 1.0)
	p_meter_l.text = "SPECIAL OK!  [L]" if p.meter >= 100.0 else "SPECIAL"
	e_meter.size.x = 2.4 * e.meter
	e_meter.position.x = 1000.0 + 240.0 - 2.4 * e.meter
	e_meter.color = flash if e.meter >= 100.0 else Color(0.3, 0.75, 1.0)
	e_meter_l.text = "SPECIAL OK!" if e.meter >= 100.0 else "SPECIAL"


func reset_bars() -> void:
	p_dmg_pct = 1.0
	e_dmg_pct = 1.0
