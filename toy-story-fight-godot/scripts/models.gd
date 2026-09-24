# キャラクターモデルをプリミティブメッシュから組み立てる。
# モデルは足元が原点、正面が +Z。腕・脚のピボットは付け根にあり、-Y 方向に垂れている。
extends RefCounted

const FUR := preload("res://shaders/fur.gdshader")

static var _fur_cache := {}


static func fur_mat(base: Color, tip: Color, length := 0.03, density := 200.0, shells := 18) -> ShaderMaterial:
	var key := "%s_%s_%s_%s" % [base.to_html(), tip.to_html(), length, density]
	if _fur_cache.has(key):
		return _fur_cache[key]
	var first: ShaderMaterial = null
	var prev: ShaderMaterial = null
	for i in range(shells + 1):
		var m := ShaderMaterial.new()
		m.shader = FUR
		m.set_shader_parameter("shell", float(i) / shells)
		m.set_shader_parameter("fur_color", base)
		m.set_shader_parameter("tip_color", tip)
		m.set_shader_parameter("fur_length", length)
		m.set_shader_parameter("density", density)
		if prev == null:
			first = m
		else:
			prev.next_pass = m
		prev = m
	_fur_cache[key] = first
	return first


static func std(color: Color, rough := 0.5, metal := 0.0, clearcoat := 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = rough
	m.metallic = metal
	if clearcoat > 0.0:
		m.clearcoat_enabled = true
		m.clearcoat = clearcoat
		m.clearcoat_roughness = 0.1
	return m


static func glow(color: Color, energy := 3.0) -> StandardMaterial3D:
	var m := std(color, 0.3)
	m.emission_enabled = true
	m.emission = color
	m.emission_energy_multiplier = energy
	return m


static func sphere(r: float, seg := 48) -> SphereMesh:
	var s := SphereMesh.new()
	s.radius = r
	s.height = r * 2.0
	s.radial_segments = seg
	s.rings = seg >> 1
	return s


static func capsule(r: float, h: float) -> CapsuleMesh:
	var c := CapsuleMesh.new()
	c.radius = r
	c.height = h
	c.radial_segments = 32
	c.rings = 12
	return c


static func cyl(r: float, h: float, top := -1.0) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.bottom_radius = r
	c.top_radius = r if top < 0.0 else top
	c.height = h
	return c


static func box(size: Vector3) -> BoxMesh:
	var b := BoxMesh.new()
	b.size = size
	return b


static func torus(inner: float, outer: float) -> TorusMesh:
	var t := TorusMesh.new()
	t.inner_radius = inner
	t.outer_radius = outer
	return t


static func add(parent: Node3D, m: Mesh, mat: Material, pos := Vector3.ZERO, scl := Vector3.ONE, rot := Vector3.ZERO) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = m
	mi.material_override = mat
	mi.position = pos
	mi.scale = scl
	mi.rotation = rot
	parent.add_child(mi)
	return mi


static func pivot(parent: Node3D, pos: Vector3) -> Node3D:
	var n := Node3D.new()
	n.position = pos
	parent.add_child(n)
	return n


static func build(kind: String) -> Dictionary:
	match kind:
		"robot":
			return build_robot()
		"dino":
			return build_dino()
		"jack":
			return build_jack()
	return build_lotso()


# ---- ロッツォ ----
static func build_lotso() -> Dictionary:
	var root := Node3D.new()
	var pink := fur_mat(Color(0.66, 0.2, 0.42), Color(0.88, 0.44, 0.66), 0.032, 210.0)
	var beige := fur_mat(Color(0.78, 0.66, 0.48), Color(0.97, 0.9, 0.75), 0.028, 230.0)
	var nose := std(Color(0.3, 0.05, 0.18), 0.22, 0.0, 0.8)
	var dark := std(Color(0.22, 0.03, 0.1), 0.6)
	var brow := std(Color(0.45, 0.1, 0.28), 0.9)
	var wood := std(Color(0.5, 0.28, 0.12), 0.45, 0.0, 0.5)

	var legs := []
	for sx in [-1.0, 1.0]:
		var leg := pivot(root, Vector3(sx * 0.17, 0.5, 0.0))
		add(leg, capsule(0.15, 0.55), pink, Vector3(0, -0.24, 0))
		add(leg, sphere(0.15), pink, Vector3(0, -0.43, 0.06), Vector3(1.0, 0.6, 1.3))
		legs.append(leg)

	var torso := pivot(root, Vector3(0, 0.5, 0))
	add(torso, sphere(0.42, 64), pink, Vector3(0, 0.3, 0), Vector3(1.0, 1.05, 0.9))
	add(torso, sphere(0.3), beige, Vector3(0, 0.28, 0.2), Vector3(0.95, 1.1, 0.55))

	var arms := []
	for sx in [-1.0, 1.0]:
		var arm := pivot(torso, Vector3(sx * 0.38, 0.52, 0))
		add(arm, capsule(0.12, 0.55), pink, Vector3(sx * 0.02, -0.22, 0))
		arms.append(arm)

	# 右手の杖
	var cane := pivot(arms[1], Vector3(0.02, -0.44, 0.1))
	add(cane, cyl(0.022, 0.6), wood, Vector3(0, -0.25, 0))
	add(cane, cyl(0.028, 0.24), wood, Vector3(0, 0.05, 0.02), Vector3.ONE, Vector3(PI / 2, 0, 0))

	var head := pivot(torso, Vector3(0, 0.72, 0))
	add(head, sphere(0.36, 64), pink, Vector3(0, 0.28, 0), Vector3(1.05, 0.95, 0.95))
	for sx in [-1.0, 1.0]:
		add(head, sphere(0.13), pink, Vector3(sx * 0.27, 0.58, -0.02), Vector3(1, 1, 0.55))
		add(head, sphere(0.08), beige, Vector3(sx * 0.27, 0.57, 0.05), Vector3(1, 1, 0.4))
		# 目
		add(head, sphere(0.05, 24), std(Color(0.95, 0.95, 0.93), 0.15, 0.0, 1.0), Vector3(sx * 0.11, 0.42, 0.32))
		add(head, sphere(0.034, 24), std(Color(0.45, 0.22, 0.08), 0.1, 0.0, 1.0), Vector3(sx * 0.11, 0.42, 0.355))
		add(head, sphere(0.018, 16), std(Color(0.02, 0.01, 0.01), 0.05, 0.0, 1.0), Vector3(sx * 0.11, 0.42, 0.382))
		add(head, capsule(0.022, 0.14), brow, Vector3(sx * 0.12, 0.505, 0.325), Vector3.ONE, Vector3(0, 0, PI / 2 + sx * 0.18))
	add(head, sphere(0.2), beige, Vector3(0, 0.18, 0.28), Vector3(1.15, 0.85, 0.8))
	add(head, sphere(0.1), nose, Vector3(0, 0.26, 0.45), Vector3(1.35, 0.85, 0.8))
	add(head, sphere(0.08), dark, Vector3(0, 0.085, 0.43), Vector3(1.3, 0.55, 0.5))
	add(head, sphere(0.045), std(Color(0.85, 0.35, 0.45), 0.4), Vector3(0, 0.07, 0.45), Vector3(1.2, 0.5, 0.5))

	return {"root": root, "torso": torso, "head": head, "arm_l": arms[0], "arm_r": arms[1],
		"leg_l": legs[0], "leg_r": legs[1]}


# ---- ブリキロボ ----
static func build_robot() -> Dictionary:
	var root := Node3D.new()
	var metal := std(Color(0.62, 0.66, 0.72), 0.28, 0.95)
	var red := std(Color(0.72, 0.07, 0.05), 0.3, 0.2, 1.0)
	var dark := std(Color(0.08, 0.08, 0.1), 0.4, 0.5)

	var legs := []
	for sx in [-1.0, 1.0]:
		var leg := pivot(root, Vector3(sx * 0.16, 0.5, 0))
		add(leg, box(Vector3(0.18, 0.46, 0.2)), metal, Vector3(0, -0.23, 0))
		add(leg, box(Vector3(0.24, 0.08, 0.34)), red, Vector3(0, -0.46, 0.05))
		legs.append(leg)

	var torso := pivot(root, Vector3(0, 0.5, 0))
	add(torso, box(Vector3(0.7, 0.7, 0.45)), metal, Vector3(0, 0.35, 0))
	add(torso, box(Vector3(0.42, 0.32, 0.04)), red, Vector3(0, 0.4, 0.23))
	for i in 3:
		add(torso, sphere(0.035, 16), glow([Color(1, 0.9, 0.2), Color(0.2, 1, 0.4), Color(0.2, 0.6, 1)][i], 4.0), Vector3(-0.1 + i * 0.1, 0.4, 0.255))
	for sx in [-1.0, 1.0]:
		for sy in [0.08, 0.62]:
			add(torso, sphere(0.02, 12), metal, Vector3(sx * 0.3, sy, 0.225))
	# ゼンマイ
	var key := pivot(torso, Vector3(0, 0.4, -0.25))
	add(key, cyl(0.03, 0.2), metal, Vector3(0, 0, -0.08), Vector3.ONE, Vector3(PI / 2, 0, 0))
	add(key, box(Vector3(0.36, 0.14, 0.03)), metal, Vector3(0, 0, -0.18))

	var arms := []
	for sx in [-1.0, 1.0]:
		var arm := pivot(torso, Vector3(sx * 0.43, 0.6, 0))
		add(arm, sphere(0.09, 24), metal, Vector3.ZERO)
		add(arm, cyl(0.065, 0.46), metal, Vector3(0, -0.24, 0))
		add(arm, box(Vector3(0.16, 0.14, 0.16)), red, Vector3(0, -0.5, 0))
		arms.append(arm)

	var head := pivot(torso, Vector3(0, 0.72, 0))
	add(head, cyl(0.08, 0.08), dark, Vector3(0, 0.02, 0))
	add(head, box(Vector3(0.46, 0.4, 0.4)), metal, Vector3(0, 0.24, 0))
	add(head, box(Vector3(0.38, 0.1, 0.03)), dark, Vector3(0, 0.29, 0.2))
	var eye := add(head, box(Vector3(0.08, 0.06, 0.02)), glow(Color(1, 0.1, 0.05), 6.0), Vector3(0, 0.29, 0.22))
	add(head, box(Vector3(0.24, 0.05, 0.02)), dark, Vector3(0, 0.13, 0.205))
	add(head, cyl(0.012, 0.18), metal, Vector3(0, 0.52, 0))
	add(head, sphere(0.04, 16), glow(Color(1, 0.2, 0.1), 5.0), Vector3(0, 0.62, 0))

	return {"root": root, "torso": torso, "head": head, "arm_l": arms[0], "arm_r": arms[1],
		"leg_l": legs[0], "leg_r": legs[1], "key": key, "eye": eye}


# ---- ゼンマイザウルス ----
static func build_dino() -> Dictionary:
	var root := Node3D.new()
	var plastic := std(Color(0.18, 0.5, 0.16), 0.32, 0.0, 1.0)
	var belly := std(Color(0.85, 0.78, 0.3), 0.4, 0.0, 0.6)
	var white := std(Color(0.96, 0.95, 0.9), 0.2)
	var metal := std(Color(0.7, 0.7, 0.72), 0.25, 0.95)

	var legs := []
	for sx in [-1.0, 1.0]:
		var leg := pivot(root, Vector3(sx * 0.22, 0.55, -0.05))
		add(leg, capsule(0.15, 0.6), plastic, Vector3(0, -0.26, 0))
		add(leg, sphere(0.13), plastic, Vector3(0, -0.5, 0.1), Vector3(1.0, 0.5, 1.6))
		legs.append(leg)

	var torso := pivot(root, Vector3(0, 0.55, 0))
	add(torso, sphere(0.42, 64), plastic, Vector3(0, 0.3, 0), Vector3(0.95, 1.15, 1.0))
	add(torso, sphere(0.3), belly, Vector3(0, 0.26, 0.26), Vector3(0.9, 1.1, 0.5))
	var tail := pivot(torso, Vector3(0, 0.15, -0.3))
	add(tail, capsule(0.16, 0.7), plastic, Vector3(0, -0.08, -0.28), Vector3.ONE, Vector3(-PI / 2 - 0.3, 0, 0))
	add(tail, capsule(0.09, 0.5), plastic, Vector3(0, -0.2, -0.7), Vector3.ONE, Vector3(-PI / 2 - 0.2, 0, 0))
	var yellow := std(Color(0.95, 0.75, 0.1), 0.35, 0.0, 0.8)
	for i in 5:
		add(torso, cyl(0.08, 0.14, 0.0), yellow, Vector3(0, 0.75 - i * 0.14, -0.25 - i * 0.07), Vector3(0.4, 1, 1), Vector3(-0.5 - i * 0.2, 0, 0))
	var key := pivot(torso, Vector3(0.42, 0.3, 0))
	add(key, cyl(0.025, 0.14), metal, Vector3(0.06, 0, 0), Vector3.ONE, Vector3(0, 0, PI / 2))
	add(key, box(Vector3(0.03, 0.12, 0.3)), metal, Vector3(0.14, 0, 0))

	var arms := []
	for sx in [-1.0, 1.0]:
		var arm := pivot(torso, Vector3(sx * 0.3, 0.52, 0.2))
		add(arm, capsule(0.06, 0.34), plastic, Vector3(0, -0.13, 0))
		arms.append(arm)

	var head := pivot(torso, Vector3(0, 0.75, 0.1))
	add(head, sphere(0.26), plastic, Vector3(0, 0.26, 0.18), Vector3(1.0, 0.85, 1.5))
	add(head, sphere(0.2), plastic, Vector3(0, 0.12, 0.28), Vector3(0.95, 0.5, 1.4))
	for i in 6:
		var sx := -1.0 if i < 3 else 1.0
		add(head, cyl(0.022, 0.07, 0.0), white, Vector3(sx * 0.14, 0.16, 0.3 + (i % 3) * 0.1), Vector3.ONE, Vector3(PI, 0, 0))
	for sx in [-1.0, 1.0]:
		add(head, sphere(0.05, 24), std(Color(1, 0.9, 0.2), 0.1, 0.0, 1.0), Vector3(sx * 0.17, 0.36, 0.28))
		add(head, sphere(0.025, 16), std(Color(0.02, 0.02, 0.02), 0.05, 0.0, 1.0), Vector3(sx * 0.2, 0.37, 0.3), Vector3(0.6, 1.4, 1))

	return {"root": root, "torso": torso, "head": head, "arm_l": arms[0], "arm_r": arms[1],
		"leg_l": legs[0], "leg_r": legs[1], "key": key, "tail": tail}


# ---- ダーク・ジャック（びっくり箱ピエロ） ----
static func build_jack() -> Dictionary:
	var root := Node3D.new()
	var wood := std(Color(0.25, 0.1, 0.35), 0.35, 0.0, 0.7)
	var trim := std(Color(0.95, 0.75, 0.15), 0.25, 0.9)
	var satin := std(Color(0.35, 0.08, 0.5), 0.35)
	var satin2 := std(Color(0.9, 0.15, 0.35), 0.35)
	var porcelain := std(Color(0.96, 0.94, 0.9), 0.15, 0.0, 1.0)
	var spring := std(Color(0.75, 0.75, 0.78), 0.2, 1.0)

	var base := pivot(root, Vector3.ZERO)
	add(base, box(Vector3(0.8, 0.7, 0.8)), wood, Vector3(0, 0.35, 0))
	for y in [0.02, 0.68]:
		add(base, box(Vector3(0.84, 0.05, 0.84)), trim, Vector3(0, y, 0))
	add(base, box(Vector3(0.8, 0.05, 0.8)), wood, Vector3(0, 0.72, -0.42), Vector3.ONE, Vector3(-1.9, 0, 0))
	var key := pivot(base, Vector3(0.42, 0.35, 0))
	add(key, cyl(0.025, 0.12), trim, Vector3(0.05, 0, 0), Vector3.ONE, Vector3(0, 0, PI / 2))
	add(key, box(Vector3(0.03, 0.26, 0.05)), trim, Vector3(0.11, -0.1, 0))

	var torso := pivot(root, Vector3(0, 0.7, 0))
	for i in 7:
		add(torso, torus(0.08, 0.14), spring, Vector3(0, 0.03 + i * 0.06, 0))
	add(torso, sphere(0.25), satin, Vector3(0, 0.62, 0), Vector3(1.0, 1.2, 0.85))
	add(torso, sphere(0.13), satin2, Vector3(0, 0.62, 0.17), Vector3(1, 1, 0.4))
	add(torso, torus(0.1, 0.24), std(Color(1, 1, 1), 0.8), Vector3(0, 0.88, 0), Vector3(1, 1.8, 1))

	var arms := []
	for sx in [-1.0, 1.0]:
		var arm := pivot(torso, Vector3(sx * 0.27, 0.78, 0))
		add(arm, capsule(0.07, 0.42), satin if sx < 0 else satin2, Vector3(0, -0.18, 0))
		add(arm, sphere(0.08), std(Color(1, 1, 1), 0.6), Vector3(0, -0.42, 0))
		arms.append(arm)

	var head := pivot(torso, Vector3(0, 0.92, 0))
	add(head, sphere(0.26, 64), porcelain, Vector3(0, 0.25, 0))
	add(head, sphere(0.06, 24), std(Color(0.85, 0.02, 0.05), 0.1, 0.0, 1.0), Vector3(0, 0.23, 0.26))
	for sx in [-1.0, 1.0]:
		add(head, sphere(0.04, 16), std(Color(0.02, 0.02, 0.02), 0.1), Vector3(sx * 0.1, 0.32, 0.22), Vector3(1, 1.3, 0.6))
		add(head, capsule(0.012, 0.12), std(Color(0.05, 0.02, 0.05), 0.5), Vector3(sx * 0.1, 0.4, 0.22), Vector3.ONE, Vector3(0, 0, PI / 2 - sx * 0.35))
		add(head, cyl(0.1, 0.36, 0.0), satin if sx < 0 else satin2, Vector3(sx * 0.13, 0.55, -0.02), Vector3.ONE, Vector3(0, 0, -sx * 0.55))
		add(head, sphere(0.045, 16), trim, Vector3(sx * 0.25, 0.7, -0.02))
	add(head, torus(0.08, 0.1), std(Color(0.7, 0.02, 0.1), 0.3), Vector3(0, 0.13, 0.2), Vector3(1.2, 1, 0.6), Vector3(PI / 2 - 0.3, 0, 0))

	return {"root": root, "torso": torso, "head": head, "arm_l": arms[0], "arm_r": arms[1],
		"key": key, "no_legs": true}


static func build_strawberry() -> Node3D:
	var n := Node3D.new()
	var berry := std(Color(0.85, 0.05, 0.12), 0.25, 0.0, 1.0)
	berry.emission_enabled = true
	berry.emission = Color(1, 0.1, 0.3)
	berry.emission_energy_multiplier = 0.6
	add(n, sphere(0.15), berry, Vector3.ZERO, Vector3(1, 1.2, 1))
	var seed_m := std(Color(1, 0.85, 0.3), 0.3)
	for i in 14:
		var a := i * 2.4
		var y := -0.12 + (i % 5) * 0.05
		var r := sqrt(max(0.0, 0.15 * 0.15 - (y / 1.2) * (y / 1.2)))
		add(n, sphere(0.012, 8), seed_m, Vector3(cos(a) * r, y, sin(a) * r))
	add(n, cyl(0.1, 0.04, 0.02), std(Color(0.15, 0.55, 0.1), 0.6), Vector3(0, 0.18, 0))
	return n
