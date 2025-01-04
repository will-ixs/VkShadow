#version 450
#extension GL_EXT_buffer_reference : require

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	mat4 Q;
	mat4 lightview;
	mat4 lightproj;
	vec3 lightpos;	
	vec3 lightcol;
	vec3 ka;
	vec3 kd;
	vec4 kss;
} ubo;

layout (location = 0) out vec3 worldNorm;
layout (location = 1) out vec4 worldPos;
layout (location = 2) out vec4 lightPos;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 col;
	float uv_y;
	vec3 normal;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout( push_constant ) uniform constants{
	mat4 model;
	VertexBuffer vertex_buffer;
} pc;

void main() 
{	
	Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
	gl_Position =  ubo.proj * ubo.view * pc.model * vec4(v.position, 1.0f);
	
	worldNorm = normalize(vec3(ubo.Q * vec4(v.normal, 1.0f)));
	//worldNorm = v.normal;
	worldPos = pc.model * vec4(v.position, 1.0f);
	lightPos = ubo.lightproj * ubo.lightview * pc.model * vec4(v.position, 1.0);
}
