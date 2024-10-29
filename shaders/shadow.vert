#version 450
#extension GL_EXT_buffer_reference : require

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	mat4 Q;
	mat4 lightview;
	mat4 lightproj;
	vec3 lightpos;	
	vec3 lightcol;
	vec3 ka;
	vec3 ks;
	vec3 kd;
	float n;
} ubo;

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

layout(push_constant) uniform constants{	
	VertexBuffer vertex_buffer;
} pc;

void main() 
{	
	Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
	gl_Position =  ubo.lightproj * ubo.lightview * ubo.model * vec4(v.position, 1.0f);
}
