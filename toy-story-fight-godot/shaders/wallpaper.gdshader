// 青空と雲の壁紙（アンディの部屋風）。
shader_type spatial;

uniform vec3 sky : source_color = vec3(0.36, 0.62, 0.86);
uniform vec3 cloud : source_color = vec3(0.97, 0.97, 1.0);

varying vec3 wpos;

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

float circle(vec2 q, vec2 c, float r) { return length(q - c) - r; }

void fragment() {
	vec2 size = vec2(1.6, 1.1);
	vec2 p = wpos.xy;
	float row = floor(p.y / size.y);
	p.x += mod(row, 2.0) * 0.8;
	vec2 q = mod(p, size) - size * 0.5;
	float d = min(min(circle(q, vec2(-0.2, -0.02), 0.15), circle(q, vec2(0.0, 0.06), 0.2)),
		min(circle(q, vec2(0.22, -0.03), 0.14), max(abs(q.y + 0.07) - 0.1, abs(q.x) - 0.3)));
	float c = smoothstep(0.008, -0.008, d);
	float paper = fract(sin(dot(floor(wpos.xy * 300.0), vec2(12.9898, 78.233))) * 43758.5453);
	ALBEDO = mix(sky, cloud, c) * mix(0.96, 1.0, paper);
	ROUGHNESS = 0.9;
}
