// シェルテクスチャリングによる毛並みシェーダー。
// 同じメッシュを法線方向に少しずつ膨らませた層 (shell) を重ね、毛先以外を discard する。
shader_type spatial;
render_mode cull_back, diffuse_burley, specular_schlick_ggx;

uniform vec3 fur_color : source_color = vec3(0.7, 0.22, 0.45);
uniform vec3 tip_color : source_color = vec3(0.86, 0.42, 0.63);
uniform float shell = 0.0;
uniform float fur_length = 0.035;
uniform float density = 120.0;
uniform float thickness = 0.9;
uniform float gravity_strength = 0.35;

varying vec3 base_pos;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void vertex() {
	base_pos = VERTEX;
	vec3 down = normalize((inverse(MODEL_MATRIX) * vec4(0.0, -1.0, 0.0, 0.0)).xyz);
	VERTEX += NORMAL * fur_length * shell + down * fur_length * gravity_strength * shell * shell;
}

void fragment() {
	vec3 p = base_pos * density;
	vec3 cell = floor(p);
	vec3 jitter = vec3(hash(cell + 1.3), hash(cell + 2.7), hash(cell + 5.1)) - 0.5;
	vec3 f = fract(p) - 0.5 - jitter * 0.6;
	float h = hash(cell);
	float len = mix(0.45, 1.0, h);
	if (shell > 0.0) {
		float d = length(f) * 2.0;
		if (shell > len || d > thickness * (1.0 - shell / len)) {
			discard;
		}
	}
	float ao = mix(0.4, 1.0, shell);
	vec3 col = mix(fur_color, tip_color, shell) * mix(0.85, 1.12, hash(cell + 7.0));
	ALBEDO = col * ao;
	ROUGHNESS = 0.95;
	SPECULAR = 0.25;
	RIM = 0.35;
	RIM_TINT = 0.6;
}
