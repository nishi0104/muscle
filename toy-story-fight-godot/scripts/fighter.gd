# ファイター1体分の状態・移動・攻撃判定・プロシージャルアニメーション。
extends Node3D

const GRAVITY := 22.0
const JUMP_V := 8.5
const ARENA := 3.8
const ATTACKS := {
	"punch": {"dur": 0.32, "a0": 0.08, "a1": 0.16, "range": 0.8, "h0": 0.8, "h1": 1.5, "dmg": 6.0, "kb": 2.5, "meter": 9.0},
	"kick": {"dur": 0.48, "a0": 0.16, "a1": 0.26, "range": 1.0, "h0": 0.1, "h1": 0.9, "dmg": 10.0, "kb": 4.0, "meter": 13.0},
	"special": {"dur": 0.7, "a0": 0.25, "a1": 0.3, "range": 0.0, "h0": 0.0, "h1": 0.0, "dmg": 0.0, "kb": 0.0, "meter": 0.0},
}

var def := {}
var is_player := false
var hp := 100.0
var max_hp := 100.0
var meter := 0.0
var vx := 0.0
var vy := 0.0
var facing := 1
var state := "idle"
var atk := {}
var hitstun := 0.0
var guarding := false
var squash := 0.0
var anim_t := 0.0
var face_camera := false
var parts := {}
var yaw: Node3D
var pose: Node3D
var torso_y := 0.0
var ai_t := 0.0
var ai_act := "wait"


func setup(d: Dictionary, player: bool, model: Dictionary) -> void:
	def = d
	is_player = player
	hp = d.get("hp", 100.0)
	max_hp = hp
	yaw = Node3D.new()
	add_child(yaw)
	pose = Node3D.new()
	yaw.add_child(pose)
	pose.add_child(model["root"])
	parts = model
	torso_y = parts["torso"].position.y


func on_ground() -> bool:
	return position.y <= 0.001


func start_attack(kind: String) -> void:
	atk = ATTACKS[kind].duplicate()
	atk["kind"] = kind
	atk["t"] = 0.0
	atk["hit"] = false
	state = kind
	if kind == "special":
		meter = 0.0


func tick(delta: float, input: Dictionary, foe, game) -> void:
	anim_t += delta
	if state != "ko" and state != "win":
		facing = 1 if foe.position.x > position.x else -1
	if state == "ko" or state == "win":
		guarding = false
		if on_ground():
			vx = move_toward(vx, 0.0, 8.0 * delta)
	elif hitstun > 0.0:
		guarding = false
		hitstun -= delta
		state = "hurt"
		if on_ground():
			vx = move_toward(vx, 0.0, 10.0 * delta)
		if hitstun <= 0.0:
			state = "idle"
	elif not atk.is_empty():
		guarding = false
		atk["t"] += delta
		if not atk["hit"] and atk["t"] >= atk["a0"]:
			if atk["kind"] == "special":
				atk["hit"] = true
				game.spawn_projectile(self)
			elif atk["t"] <= atk["a1"]:
				var hx: float = position.x + facing * atk["range"]
				var top: float = position.y + atk["h1"]
				var bot: float = position.y + atk["h0"]
				if absf(hx - foe.position.x) < 0.55 and top > foe.position.y and bot < foe.position.y + 1.7 and foe.state != "ko":
					atk["hit"] = true
					var pos := Vector3(hx - facing * 0.25, position.y + (atk["h0"] + atk["h1"]) * 0.5, 0.25)
					game.do_hit(self, foe, atk["dmg"] * def.get("power", 1.0), atk["kb"], atk["meter"], pos)
		if atk["t"] >= atk["dur"]:
			atk = {}
			state = "idle"
		if on_ground():
			vx = move_toward(vx, 0.0, 20.0 * delta)
	else:
		guarding = input.get("down", false) and on_ground()
		var sp: float = def.get("speed", 3.0)
		if guarding:
			vx = 0.0
			state = "guard"
		else:
			vx = (sp if input.get("right", false) else 0.0) - (sp if input.get("left", false) else 0.0)
			if input.get("up", false) and on_ground():
				vy = JUMP_V
			if not on_ground() or vy > 0.0:
				state = "jump"
			elif absf(vx) > 0.01:
				state = "walk"
			else:
				state = "idle"
			if input.get("punch", false):
				start_attack("punch")
			elif input.get("kick", false):
				start_attack("kick")
			elif input.get("special", false) and meter >= 100.0:
				start_attack("special")

	vy -= GRAVITY * delta
	position.x += vx * delta
	position.y += vy * delta
	if position.y < 0.0:
		position.y = 0.0
		vy = 0.0
	position.x = clampf(position.x, -ARENA, ARENA)
	var d: float = foe.position.x - position.x
	if absf(d) < 0.75 and absf(foe.position.y - position.y) < 1.2 and state != "ko" and foe.state != "ko":
		position.x -= (0.75 - absf(d)) * 0.5 * (signf(d) if d != 0.0 else 1.0)
	animate(delta)


func cpu_input(delta: float, foe) -> Dictionary:
	var inp := {}
	var dist := absf(foe.position.x - position.x)
	var ag: float = def.get("aggro", 0.5)
	ai_t -= delta
	if ai_t <= 0.0:
		ai_t = randf_range(0.15, 0.45)
		var r := randf()
		if not foe.atk.is_empty() and dist < 1.6 and r < ag * 0.6:
			ai_act = "guard"
		elif meter >= 100.0 and r < 0.4:
			ai_act = "special"
		elif dist > 1.3:
			ai_act = "approach" if r < ag else ("jump" if r < ag + 0.12 else "wait")
		elif r < ag * 0.55:
			ai_act = "punch"
		elif r < ag:
			ai_act = "kick"
		elif r < ag + 0.15:
			ai_act = "back"
		else:
			ai_act = "guard"
	var toward := "right" if foe.position.x > position.x else "left"
	var away := "left" if toward == "right" else "right"
	match ai_act:
		"approach":
			inp[toward] = true
		"back":
			inp[away] = true
		"jump":
			inp["up"] = true
			inp[toward] = true
			ai_act = "approach"
		"guard":
			inp["down"] = true
		"punch", "kick", "special":
			inp[ai_act] = true
			ai_act = "wait"
	return inp


func _curve(p: float, a: float, b: float) -> float:
	return minf(smoothstep(0.0, a, p), 1.0 - smoothstep(b, 1.0, p))


func animate(delta: float) -> void:
	var t := anim_t
	var target_yaw := 0.0 if face_camera else facing * deg_to_rad(62.0)
	yaw.rotation.y = lerp_angle(yaw.rotation.y, target_yaw, 1.0 - exp(-12.0 * delta))
	var no_legs: bool = parts.get("no_legs", false)

	var g := {"arm_l": 0.1, "arm_r": 0.1, "arm_lz": -0.15, "arm_rz": 0.15, "leg_l": 0.0, "leg_r": 0.0,
		"torso_x": 0.0, "torso_y": 0.0, "head_x": 0.0, "pose_x": 0.0, "bob": 0.0}
	var k := 1.0 - exp(-18.0 * delta)
	match state:
		"idle":
			var s := sin(t * 2.5)
			g.bob = s * 0.015
			g.arm_l = 0.1 + s * 0.05
			g.arm_r = 0.1 - s * 0.05
			g.head_x = s * 0.03
		"walk":
			var s := sin(t * 11.0)
			g.leg_l = s * 0.6
			g.leg_r = -s * 0.6
			g.arm_l = -s * 0.45
			g.arm_r = s * 0.45
			g.bob = absf(s) * 0.05 if not no_legs else absf(s) * 0.12
			g.torso_x = 0.08
		"jump":
			g.leg_l = -1.0
			g.leg_r = -0.3
			g.arm_l = -2.5
			g.arm_r = -2.2
			g.arm_lz = -0.4
			g.arm_rz = 0.4
		"guard":
			g.arm_l = -1.9
			g.arm_r = -1.9
			g.arm_lz = 0.55
			g.arm_rz = -0.55
			g.torso_x = 0.25
			g.head_x = 0.2
			g.bob = -0.06
		"hurt":
			g.torso_x = -0.45
			g.head_x = -0.35
			g.arm_l = -0.7
			g.arm_r = -0.7
			g.arm_lz = -0.6
			g.arm_rz = 0.6
			k = 1.0 - exp(-30.0 * delta)
		"ko":
			g.pose_x = -1.45
			g.arm_l = -2.8
			g.arm_r = -2.8
			g.head_x = -0.3
			k = 1.0 - exp(-6.0 * delta)
		"win":
			var s := sin(t * 6.0)
			g.arm_r = -PI + s * 0.15
			g.arm_rz = -0.2
			g.arm_l = 0.2
			g.bob = absf(s) * 0.08
			g.head_x = -0.15
		"punch", "kick", "special":
			var p: float = atk["t"] / atk["dur"]
			var e := _curve(p, atk["a0"] / atk["dur"], atk["a1"] / atk["dur"])
			k = 1.0 - exp(-40.0 * delta)
			if state == "punch":
				g.arm_r = lerpf(0.1, -1.6, e)
				g.arm_rz = lerpf(0.15, -0.25, e)
				g.arm_l = -1.3
				g.arm_lz = 0.4
				g.torso_y = 0.35 * e
				g.torso_x = 0.18 * e
			elif state == "kick":
				if no_legs:
					g.torso_x = 0.9 * e
					g.bob = 0.25 * e
					g.arm_l = -1.6 * e
					g.arm_r = -1.6 * e
				else:
					g.leg_r = -1.5 * e
					g.leg_l = 0.2 * e
					g.torso_x = -0.35 * e
					g.arm_l = -0.8
					g.arm_r = 0.4
					g.arm_lz = -0.6
					g.arm_rz = 0.6
			else:
				g.arm_l = -1.6 * e
				g.arm_r = -1.6 * e
				g.arm_lz = 0.3 * e
				g.arm_rz = -0.3 * e
				g.torso_x = 0.2 * e
				g.head_x = 0.15 * e

	_pose_part("arm_l", g.arm_l, g.arm_lz, k)
	_pose_part("arm_r", g.arm_r, g.arm_rz, k)
	_pose_part("leg_l", g.leg_l, 0.0, k)
	_pose_part("leg_r", g.leg_r, 0.0, k)
	_pose_part("head", g.head_x, 0.0, k)
	var torso: Node3D = parts["torso"]
	torso.rotation.x = lerpf(torso.rotation.x, g.torso_x, k)
	torso.rotation.y = lerpf(torso.rotation.y, g.torso_y, k)
	torso.position.y = lerpf(torso.position.y, torso_y + g.bob, k)
	pose.rotation.x = lerpf(pose.rotation.x, g.pose_x, k)

	if parts.has("key"):
		parts["key"].rotation.z += delta * (6.0 if state == "walk" else 2.0)
	if parts.has("tail"):
		parts["tail"].rotation.y = sin(t * 3.0) * 0.35
	if parts.has("eye"):
		parts["eye"].position.x = sin(t * 3.0) * 0.13

	squash = move_toward(squash, 0.0, delta * 6.0)
	pose.scale = Vector3(1.0 + squash * 0.12, 1.0 - squash * 0.12, 1.0 + squash * 0.12)


func _pose_part(name_: String, rx: float, rz: float, k: float) -> void:
	var n: Node3D = parts.get(name_)
	if n == null:
		return
	n.rotation.x = lerpf(n.rotation.x, rx, k)
	n.rotation.z = lerpf(n.rotation.z, rz, k)
