// フローリング（板目・木目・継ぎ目）をプロシージャル生成する床シェーダー。
shader_type spatial;

uniform vec3 c1 : source_color = vec3(0.56, 0.34, 0.17);
uniform vec3 c2 : source_color = vec3(0.36, 0.2, 0.09);

varying vec3 wpos;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = wpos.xz;
	float plank_w = 0.22;
	float plank_l = 1.8;
	float row = floor(p.y / plank_w);
	float off = hash(vec2(row, 3.0)) * 3.0;
	float col = floor((p.x + off) / plank_l);
	float id = hash(vec2(row, col));
	vec2 lp = vec2(fract((p.x + off) / plank_l), fract(p.y / plank_w));
	float grain = noise(vec2(p.x * 3.0, p.y * 60.0 + id * 10.0)) * 0.6
		+ noise(vec2(p.x * 0.8, p.y * 18.0 + id * 5.0)) * 0.4;
	float rings = sin(p.y * 80.0 + grain * 6.0 + id * 20.0) * 0.5 + 0.5;
	vec3 base = mix(c1, c2, grain * 0.7 + rings * 0.2) * mix(0.8, 1.15, id);
	float edge = smoothstep(0.0, 0.03, lp.y) * smoothstep(1.0, 0.97, lp.y)
		* smoothstep(0.0, 0.004, lp.x) * smoothstep(1.0, 0.996, lp.x);
	base *= mix(0.35, 1.0, edge);
	ALBEDO = base;
	ROUGHNESS = mix(0.22, 0.5, grain);
	SPECULAR = 0.5;
}
