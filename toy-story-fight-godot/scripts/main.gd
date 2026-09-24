# ゲーム全体: 部屋のステージ・ライティング・カメラ・ラウンド進行・当たり処理。
extends Node3D

const Models := preload("res://scripts/models.gd")
const FighterScript := preload("res://scripts/fighter.gd")
const HUDScript := preload("res://scripts/hud.gd")
const FLOOR_SHADER := preload("res://shaders/wood_floor.gdshader")
const WALL_SHADER := preload("res://shaders/wallpaper.gdshader")

const LOTSO := {"name": "ロッツォ", "model": "lotso", "hp": 100.0, "speed": 3.2, "power": 1.0}
const ENEMIES := [
	{"name": "ブリキロボ", "model": "robot", "hp": 100.0, "speed": 2.8, "power": 1.0, "aggro": 0.5, "color": Color(1.0, 0.2, 0.1)},
	{"name": "ゼンマイザウルス", "model": "dino", "hp": 110.0, "speed": 3.2, "power": 1.15, "aggro": 0.65, "color": Color(1.0, 0.6, 0.1)},
	{"name": "ダーク・ジャック", "model": "jack", "hp": 120.0, "speed": 3.6, "power": 1.3, "aggro": 0.8, "color": Color(0.7, 0.2, 1.0)},
]

var scene_state := "title"
var stage := 0
var round_no := 1
var wins := [0, 0]
var timer := 99.0
var state_t := 0.0
var hitstop := 0.0
var shake := 0.0
var title_t := 0.0
var p = null
var e = null
var projs := []
var cam: Camera3D
var env: Environment
var sun: DirectionalLight3D
var lamp: OmniLight3D
var window_mat: StandardMaterial3D
var hud


func _ready() -> void:
	_setup_input()
	_build_world()
	hud = HUDScript.new()
	add_child(hud)
	show_title()


# ---------- 入力 ----------
func _setup_input() -> void:
	_action("p_left", [KEY_A, KEY_LEFT], [JOY_BUTTON_DPAD_LEFT], JOY_AXIS_LEFT_X, -1.0)
	_action("p_right", [KEY_D, KEY_RIGHT], [JOY_BUTTON_DPAD_RIGHT], JOY_AXIS_LEFT_X, 1.0)
	_action("p_up", [KEY_W, KEY_UP], [JOY_BUTTON_DPAD_UP, JOY_BUTTON_A])
	_action("p_down", [KEY_S, KEY_DOWN], [JOY_BUTTON_DPAD_DOWN, JOY_BUTTON_LEFT_SHOULDER], JOY_AXIS_LEFT_Y, 1.0)
	_action("p_punch", [KEY_J], [JOY_BUTTON_X])
	_action("p_kick", [KEY_K], [JOY_BUTTON_B])
	_action("p_special", [KEY_L], [JOY_BUTTON_Y, JOY_BUTTON_RIGHT_SHOULDER])
	_action("start", [KEY_ENTER, KEY_SPACE], [JOY_BUTTON_START])


func _action(action: String, keys: Array, buttons: Array, axis := -1, axis_value := 0.0) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	for k in keys:
		var ev := InputEventKey.new()
		ev.physical_keycode = k
		InputMap.action_add_event(action, ev)
	for b in buttons:
		var jb := InputEventJoypadButton.new()
		jb.button_index = b
		InputMap.action_add_event(action, jb)
	if axis >= 0:
		var ja := InputEventJoypadMotion.new()
		ja.axis = axis
		ja.axis_value = axis_value
		InputMap.action_add_event(action, ja)


func _player_input() -> Dictionary:
	return {
		"left": Input.is_action_pressed("p_left"),
		"right": Input.is_action_pressed("p_right"),
		"up": Input.is_action_pressed("p_up"),
		"down": Input.is_action_pressed("p_down"),
		"punch": Input.is_action_just_pressed("p_punch"),
		"kick": Input.is_action_just_pressed("p_kick"),
		"special": Input.is_action_just_pressed("p_special"),
	}


# ---------- ワールド ----------
func _build_world() -> void:
	env = Environment.new()
	var sky := Sky.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky.sky_material = sky_mat
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 0.85
	env.ssao_enabled = true
	env.ssao_radius = 0.6
	env.ssao_intensity = 1.6
	env.ssil_enabled = true
	env.ssr_enabled = true
	env.glow_enabled = true
	env.glow_intensity = 0.6
	env.glow_bloom = 0.05
	env.adjustment_enabled = true
	env.adjustment_saturation = 1.1
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)

	sun = DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.shadow_blur = 1.5
	sun.directional_shadow_max_distance = 20.0
	add_child(sun)

	lamp = OmniLight3D.new()
	lamp.position = Vector3(-3.5, 2.6, -1.8)
	lamp.omni_range = 9.0
	lamp.shadow_enabled = true
	lamp.light_color = Color(1.0, 0.75, 0.45)
	add_child(lamp)

	var fill := OmniLight3D.new()
	fill.position = Vector3(0, 3.0, 4.0)
	fill.omni_range = 12.0
	fill.light_energy = 0.35
	add_child(fill)

	# 床
	var fm := ShaderMaterial.new()
	fm.shader = FLOOR_SHADER
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(30, 16)
	Models.add(self, floor_mesh, fm, Vector3(0, 0, 2))
	# ラグ
	Models.add(self, Models.cyl(3.0, 0.015), Models.std(Color(0.15, 0.25, 0.5), 1.0), Vector3(0, 0.008, 0.3), Vector3(1.6, 1, 0.6))
	Models.add(self, Models.cyl(2.4, 0.016), Models.std(Color(0.6, 0.45, 0.2), 1.0), Vector3(0, 0.009, 0.3), Vector3(1.6, 1, 0.6))
	# 壁
	var wm := ShaderMaterial.new()
	wm.shader = WALL_SHADER
	var wall := PlaneMesh.new()
	wall.size = Vector2(30, 9)
	wall.orientation = PlaneMesh.FACE_Z
	Models.add(self, wall, wm, Vector3(0, 4.5, -3.5))
	Models.add(self, Models.box(Vector3(30, 0.18, 0.05)), Models.std(Color(0.95, 0.95, 0.92), 0.4), Vector3(0, 0.09, -3.47))
	# 窓
	window_mat = Models.glow(Color(0.7, 0.85, 1.0), 0.5)
	Models.add(self, Models.box(Vector3(2.4, 1.6, 0.02)), window_mat, Vector3(3.2, 2.8, -3.49))
	var frame_m := Models.std(Color(0.95, 0.95, 0.92), 0.4)
	for y in [2.0, 2.8, 3.6]:
		Models.add(self, Models.box(Vector3(2.6, 0.08, 0.1)), frame_m, Vector3(3.2, y, -3.45))
	for x in [2.0, 3.2, 4.4]:
		Models.add(self, Models.box(Vector3(0.08, 1.7, 0.1)), frame_m, Vector3(x, 2.8, -3.45))

	# 積み木
	var letters := ["A", "B", "C", "D", "E"]
	var colors := [Color(0.85, 0.15, 0.12), Color(0.15, 0.4, 0.85), Color(0.95, 0.75, 0.1), Color(0.15, 0.65, 0.3), Color(0.9, 0.4, 0.1)]
	var spots := [Vector3(-4.2, 0.2, -2.2), Vector3(-3.75, 0.2, -2.3), Vector3(-3.98, 0.6, -2.25), Vector3(4.6, 0.2, -1.2), Vector3(-2.2, 0.2, -2.8)]
	for i in 5:
		var b := Models.add(self, Models.box(Vector3(0.4, 0.4, 0.4)), Models.std(colors[i], 0.45, 0.0, 0.5), spots[i], Vector3.ONE, Vector3(0, randf_range(-0.3, 0.3), 0))
		var l := Label3D.new()
		l.text = letters[i]
		l.font_size = 180
		l.pixel_size = 0.0018
		l.outline_size = 0
		l.shaded = true
		l.position = Vector3(0, 0, 0.205)
		b.add_child(l)
	# ボール
	var ball := Models.add(self, Models.sphere(0.4), Models.std(Color(0.9, 0.1, 0.1), 0.3, 0.0, 1.0), Vector3(5.2, 0.4, -2.0))
	Models.add(ball, Models.torus(0.39, 0.42), Models.std(Color(0.95, 0.95, 0.95), 0.3, 0.0, 1.0), Vector3.ZERO, Vector3(1, 3, 1), Vector3(0, 0, 0.4))
	Models.add(ball, Models.sphere(0.12), Models.std(Color(0.1, 0.3, 0.9), 0.3, 0.0, 1.0), Vector3(0, 0, 0.33), Vector3(1, 1, 0.5))
	# おもちゃ箱
	var chest_m := Models.std(Color(0.55, 0.32, 0.15), 0.5, 0.0, 0.4)
	Models.add(self, Models.box(Vector3(1.8, 0.9, 0.8)), chest_m, Vector3(-5.8, 0.45, -2.8))
	Models.add(self, Models.box(Vector3(1.9, 0.08, 0.9)), Models.std(Color(0.8, 0.2, 0.2), 0.4, 0.0, 0.4), Vector3(-5.8, 0.94, -2.8))

	cam = Camera3D.new()
	cam.fov = 40.0
	cam.position = Vector3(0, 1.5, 5.0)
	add_child(cam)
	cam.make_current()


func _apply_lighting(s: int) -> void:
	match s:
		0:
			sun.light_color = Color(1.0, 0.95, 0.88)
			sun.light_energy = 1.3
			sun.rotation_degrees = Vector3(-50, -35, 0)
			lamp.light_energy = 0.0
			env.ambient_light_energy = 0.35
			window_mat.emission = Color(0.7, 0.85, 1.0)
		1:
			sun.light_color = Color(1.0, 0.55, 0.3)
			sun.light_energy = 1.5
			sun.rotation_degrees = Vector3(-18, -60, 0)
			lamp.light_energy = 0.6
			env.ambient_light_energy = 0.4
			window_mat.emission = Color(1.0, 0.55, 0.25)
		_:
			sun.light_color = Color(0.45, 0.55, 1.0)
			sun.light_energy = 0.35
			sun.rotation_degrees = Vector3(-40, 30, 0)
			lamp.light_energy = 3.0
			env.ambient_light_energy = 0.12
			window_mat.emission = Color(0.1, 0.12, 0.3)


# ---------- 進行 ----------
func _clear_fighters() -> void:
	for f in [p, e]:
		if f != null:
			f.queue_free()
	p = null
	e = null
	for pr in projs:
		pr["node"].queue_free()
	projs.clear()


func _make_fighter(d: Dictionary, player: bool, x: float):
	var f = FighterScript.new()
	f.setup(d, player, Models.build(d["model"]))
	f.position = Vector3(x, 0, 0)
	f.facing = 1 if x < 0 else -1
	add_child(f)
	return f


func show_title() -> void:
	Engine.time_scale = 1.0
	_clear_fighters()
	scene_state = "title"
	_apply_lighting(0)
	p = _make_fighter(LOTSO, true, 1.3)
	p.face_camera = true
	hud.show_title(true)


func start_game(s: int) -> void:
	stage = s
	round_no = 1
	wins = [0, 0]
	_apply_lighting(s)
	hud.show_title(false)
	hud.set_enemy_name(ENEMIES[s]["name"])
	new_round()


func new_round() -> void:
	_clear_fighters()
	p = _make_fighter(LOTSO, true, -1.8)
	e = _make_fighter(ENEMIES[stage], false, 1.8)
	timer = 99.0
	scene_state = "intro"
	state_t = 1.8
	hud.reset_bars()
	var sub: String = "STAGE %d　VS %s" % [stage + 1, ENEMIES[stage]["name"]] if round_no == 1 else ""
	hud.show_msg("ROUND %d" % round_no, 1.8, sub)


func _physics_process(delta: float) -> void:
	var confirm := Input.is_action_just_pressed("start") or Input.is_action_just_pressed("p_punch")
	if scene_state == "title":
		title_t += delta
		p.anim_t += delta
		p.animate(delta)
		cam.position = Vector3(0.2 + sin(title_t * 0.35) * 0.6, 1.4, 4.2)
		cam.look_at(Vector3(0.2, 1.1, 0))
		if confirm:
			start_game(0)
		return
	if hitstop > 0.0:
		hitstop -= delta
		_update_camera(delta)
		return
	match scene_state:
		"intro":
			state_t -= delta
			if state_t <= 0.0:
				scene_state = "fight"
				hud.show_msg("FIGHT!", 0.8)
		"fight":
			timer -= delta
			if timer <= 0.0:
				timer = 0.0
				_time_up()
		"ko":
			state_t -= delta
			if state_t < 2.0:
				Engine.time_scale = 1.0
			if state_t <= 0.0:
				_after_ko()
				return
		"stageclear":
			if confirm:
				start_game(stage + 1)
				return
		"gameover", "clear":
			if confirm:
				show_title()
				return
	var fighting := scene_state == "fight"
	var pin := _player_input() if fighting else {}
	var ein: Dictionary = e.cpu_input(delta, p) if fighting else {}
	p.tick(delta, pin, e, self)
	e.tick(delta, ein, p, self)
	_update_projectiles(delta)
	_update_camera(delta)
	hud.update_fight(p, e, timer, wins, delta)


func _update_camera(delta: float) -> void:
	var mid: float = (p.position.x + e.position.x) * 0.5
	var dist: float = absf(p.position.x - e.position.x)
	var target := Vector3(clampf(mid, -2.0, 2.0), 1.45, 4.4 + maxf(0.0, dist - 2.5) * 0.6)
	if scene_state == "ko" or scene_state == "stageclear" or scene_state == "clear" or scene_state == "gameover":
		var winner = p if p.state == "win" else e
		target = Vector3(winner.position.x * 0.8, 1.3, 3.3)
	cam.position = cam.position.lerp(target, 1.0 - exp(-4.0 * delta))
	cam.look_at(Vector3(cam.position.x, 1.0, 0))
	shake = move_toward(shake, 0.0, delta)
	cam.h_offset = randf_range(-1, 1) * shake * 0.5
	cam.v_offset = randf_range(-1, 1) * shake * 0.5


func spawn_projectile(f) -> void:
	var node: Node3D
	var color: Color
	if f.is_player:
		node = Models.build_strawberry()
		color = Color(1, 0.3, 0.5)
	else:
		color = f.def["color"]
		node = Node3D.new()
		Models.add(node, Models.sphere(0.18), Models.glow(color, 6.0))
		Models.add(node, Models.sphere(0.1), Models.glow(Color.WHITE, 8.0))
	var light := OmniLight3D.new()
	light.light_color = color
	light.light_energy = 2.5
	light.omni_range = 2.5
	node.add_child(light)
	node.position = Vector3(f.position.x + f.facing * 0.7, f.position.y + 1.0, 0.1)
	add_child(node)
	projs.append({"node": node, "vx": f.facing * 7.5, "owner": f, "color": color})


func _update_projectiles(delta: float) -> void:
	for pr in projs:
		var n: Node3D = pr["node"]
		n.position.x += pr["vx"] * delta
		n.rotation.z -= signf(pr["vx"]) * delta * 12.0
		var tgt = e if pr["owner"] == p else p
		if not pr.get("dead", false) and absf(n.position.x - tgt.position.x) < 0.45 \
				and tgt.position.y < n.position.y + 0.2 and tgt.position.y + 1.7 > n.position.y and tgt.state != "ko":
			pr["dead"] = true
			do_hit(pr["owner"], tgt, 22.0 * pr["owner"].def.get("power", 1.0), 5.0, 0.0, n.position)
			_burst(n.position, pr["color"], 60, 8.0, 0.05)
		if absf(n.position.x) > 8.0:
			pr["dead"] = true
	# 飛び道具同士の相殺
	for a in projs:
		for b in projs:
			if a != b and a["owner"] != b["owner"] and not a.get("dead", false) and not b.get("dead", false) \
					and a["node"].position.distance_to(b["node"].position) < 0.4:
				a["dead"] = true
				b["dead"] = true
				_burst(a["node"].position, Color.WHITE, 50, 6.0, 0.04)
	for pr in projs:
		if pr.get("dead", false):
			pr["node"].queue_free()
	projs = projs.filter(func(x): return not x.get("dead", false))


func do_hit(att, dfd, dmg: float, kb: float, meter_gain: float, pos: Vector3) -> void:
	if scene_state != "fight":
		return
	if dfd.guarding:
		dmg *= 0.15
		kb *= 0.6
		dfd.meter = minf(100.0, dfd.meter + 4.0)
		hitstop = 0.04
		_burst(pos, Color(0.5, 0.8, 1.0), 16, 3.0, 0.03)
		_popup(pos, "GUARD", Color(0.6, 0.85, 1.0))
	else:
		dfd.hitstun = 0.28 + dmg * 0.012
		dfd.atk = {}
		dfd.state = "hurt"
		dfd.squash = 1.0
		hitstop = 0.06 + dmg * 0.004
		shake = 0.12 + dmg * 0.012
		_burst(pos, Color(1, 0.85, 0.3), 30, 6.0, 0.035)
		if dfd.is_player:
			_burst(pos, Color(0.95, 0.5, 0.7), 18, 2.0, 0.07, false)
		_popup(pos, "BAM!" if dmg >= 10.0 else "POW!", Color(1, 0.9, 0.2))
	dfd.hp = maxf(0.0, dfd.hp - dmg)
	dfd.vx = kb * att.facing
	att.meter = minf(100.0, att.meter + meter_gain)
	if dfd.hp <= 0.0:
		_ko(att)


func _burst(pos: Vector3, color: Color, amount: int, speed: float, size: float, emissive := true) -> void:
	var ps := CPUParticles3D.new()
	var m := Models.sphere(size, 8)
	m.material = Models.glow(color, 5.0) if emissive else Models.std(color, 1.0)
	ps.mesh = m
	ps.amount = amount
	ps.one_shot = true
	ps.explosiveness = 1.0
	ps.lifetime = 0.5 if emissive else 1.0
	ps.spread = 180.0
	ps.initial_velocity_min = speed * 0.4
	ps.initial_velocity_max = speed
	ps.gravity = Vector3(0, -9.0 if emissive else -1.5, 0)
	ps.damping_min = 2.0
	ps.damping_max = 4.0
	var curve := Curve.new()
	curve.add_point(Vector2(0, 1))
	curve.add_point(Vector2(1, 0))
	ps.scale_amount_curve = curve
	add_child(ps)
	ps.position = pos
	ps.emitting = true
	get_tree().create_timer(1.5, true, false, true).timeout.connect(ps.queue_free)


func _popup(pos: Vector3, text: String, color: Color) -> void:
	var l := Label3D.new()
	l.text = text
	l.font_size = 110
	l.pixel_size = 0.004
	l.outline_size = 28
	l.modulate = color
	l.outline_modulate = Color(0.1, 0.02, 0.05)
	l.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	l.no_depth_test = true
	l.position = pos + Vector3(0, 0.3, 0.3)
	add_child(l)
	var tw := l.create_tween().set_parallel(true)
	tw.tween_property(l, "position:y", l.position.y + 0.6, 0.6)
	tw.tween_property(l, "modulate:a", 0.0, 0.6).set_delay(0.2)
	tw.tween_property(l, "outline_modulate:a", 0.0, 0.6).set_delay(0.2)
	tw.chain().tween_callback(l.queue_free)


func _ko(winner) -> void:
	var loser = e if winner == p else p
	loser.state = "ko"
	loser.atk = {}
	loser.hitstun = 0.0
	loser.vy = 4.0
	loser.vx = -loser.facing * 3.0
	winner.state = "win"
	winner.atk = {}
	wins[0 if winner == p else 1] += 1
	scene_state = "ko"
	state_t = 3.0
	Engine.time_scale = 0.3
	hud.show_msg("K.O.!", 3.0, "", Color(1, 0.3, 0.3))


func _time_up() -> void:
	var winner = p if p.hp / p.max_hp >= e.hp / e.max_hp else e
	_ko(winner)
	hud.show_msg("TIME UP", 3.0)


func _after_ko() -> void:
	Engine.time_scale = 1.0
	if wins[0] >= 2:
		if stage >= ENEMIES.size() - 1:
			scene_state = "clear"
			hud.show_overlay("ALL CLEAR!", "ロッツォがおもちゃ界の王者だ！")
		else:
			scene_state = "stageclear"
			hud.show_overlay("YOU WIN!", "%sを倒した！ 次の敵へ" % ENEMIES[stage]["name"])
	elif wins[1] >= 2:
		scene_state = "gameover"
		hud.show_overlay("GAME OVER", "ロッツォは負けてしまった…", Color(1, 0.35, 0.35))
	else:
		round_no += 1
		new_round()
