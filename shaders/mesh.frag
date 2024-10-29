#version 450
layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	mat4 Q;
	mat4 lightview;
	mat4 lightproj;
	vec3 lightpos_world;
	vec3 lightcol;
	vec3 ka;
	vec3 ks;
	vec3 kd;
	float n;
} ubo;

layout (binding = 1) uniform texture2D _depth_texture;
layout (binding = 2) uniform sampler _sampler;

layout (location = 0) in vec3  worldNorm;
layout (location = 1) in vec4 worldPos;
layout (location = 2) in vec4 lightPos;

layout (location = 0) out vec4 outFragColor;

void main() 
{	
	vec3 projCoord = lightPos.xyz / lightPos.w;
	projCoord = projCoord * 0.5 + 0.5;

	//float depth = texture(sampler2D(_depth_texture, _sampler, projCoord.xy).x;
	//float coordZ = projCoord.z;

	//bool bypass = false;
	//if(projCoord.x < 0.01 || projCoord.x > 0.99 || projCoord.y < 0.01 || projCoord.y > 0.99){
	//	bypass = true;	
	//}
	
	vec3 currColor = ubo.ka;
	// if(coordZ - 0.001 < depth || bypass){
		vec3 eye = vec3(0.0f, 0.0f, 4.0f); 
		vec3 pos = vec3(worldPos);
		vec3 N = normalize(worldNorm);



		vec3 Li =  normalize(ubo.lightpos_world - pos);
		vec3 Ri = normalize(2 * N * dot(Li, N) - Li);
		vec3 kdS = ubo.kd * max(0, dot(Li, N));
		vec3 ksS = ubo.ks * pow(max(0, dot(Ri, normalize(eye))),ubo.n);
		currColor += ubo.lightcol * (kdS + ksS);
	// }
	outFragColor = vec4(worldNorm,1.0f);
}
