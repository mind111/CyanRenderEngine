#version 450 core

in VSOut {
	vec2 texCoord;
} psIn;

out vec3 outColor;

uniform sampler2D microColorBuffer;
uniform int microBufferRes;

void main() {
	// overlay debug grid on top of the micro buffer texture
	float microBufferPixelSize = (1.f / microBufferRes);
	float tx = mod(psIn.texCoord.x, microBufferPixelSize) / microBufferPixelSize;
	tx = abs(tx * 2.f - 1.f);
	tx = smoothstep(0.7, 1.f, tx);
	float ty = mod(psIn.texCoord.y, microBufferPixelSize) / microBufferPixelSize;
	ty = abs(ty * 2.f - 1.f);
	ty = smoothstep(.7, 1.f, ty);
	vec3 white = vec3(.8f);
	vec3 color = texture(microColorBuffer, psIn.texCoord).rbg;
	// outColor = mix(vec3(psIn.texCoord, 1.f), white, max(tx, ty));
	outColor = mix(vec3(color), white, max(tx, ty));
}
