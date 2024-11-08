#pragma once

#include <glm/mat4x4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)																	\
	do{																				\
		VkResult err = x;															\
		if (err) {																	\
			std::cout << "Vulkan Error: " << string_VkResult(err) << std::endl;		\
			abort();																\
		}																			\
	} while (0)																	

#define LOG(x,y)																	\
    do {																			\
        if (logging_enabled) {														\
            logger.log(x,y);														\
        }																			\
    } while (0)																	

#include "builders.h"

enum MESHTYPE
{
	OBJ,
	GLTF,
	UNDEFINED
};

struct MeshResource {
	std::string file_path;
	glm::mat4 model_mat;
	MESHTYPE type;
};

struct {

	//const char* mesh_vert = "E:/Proj/Vs/VkShadow/shaders/spirv/mesh.vert.spv";
	//const char* mesh_frag = "E:/Proj/Vs/VkShadow/shaders/spirv/mesh.frag.spv";
	//const char* shadow_vert = "E:/Proj/Vs/VkShadow/shaders/spirv/shadow.vert.spv";
	//const char* shadow_frag = "E:/Proj/Vs/VkShadow/shaders/spirv/shadow.frag.spv";

	const char* mesh_vert = "../../shaders/spirv/mesh.vert.spv";
	const char* mesh_frag = "../../shaders/spirv/mesh.frag.spv";
	const char* shadow_vert = "../../shaders/spirv/shadow.vert.spv";
	const char* shadow_frag = "../../shaders/spirv/shadow.frag.spv";
} shader_paths;

struct {
	//const MeshResource bunny_model = {
	//	.file_path = "E:/Proj/Vs/VkShadow/models/bunny.obj",
	//	.model_mat = glm::mat4(1),
	//	.type = MESHTYPE::OBJ
	//};

	const MeshResource bunny = {
		.file_path = "../../models/bunny.obj",
		.model_mat = glm::mat4(1),
		.type = MESHTYPE::OBJ
	};
	const MeshResource teapot = {
	.file_path = "../../models/teapot.obj",
	.model_mat = glm::translate(glm::mat4(1), glm::vec3(2.0f, 0.0f, 0.0f)),
	.type = MESHTYPE::OBJ
	};
	const MeshResource square = {
	.file_path = "../../models/square.obj",
	.model_mat = glm::translate(glm::mat4(1), glm::vec3(-2.0f, 0.0f, 0.0f)),
	.type = MESHTYPE::OBJ
	};
} model_res;


struct ImageData {
	VkImage image;
	VkImageView view;
	VkExtent3D extent;
	VkFormat format;
	VmaAllocation allocation;
};

struct BufferData {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};


struct PerFrameData {
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	
	VkFence render_fence;
	VkSemaphore render_semaphore;
	VkSemaphore swapcahin_semaphore;
};

//GPU DATA
struct UniformBufferObject {
	alignas(16)glm::mat4 view;
	alignas(16)glm::mat4 proj;
	alignas(16)glm::mat4 Q;

	alignas(16)glm::mat4 light_view;
	alignas(16)glm::mat4 light_proj;
	alignas(16)glm::vec3 lightpos;
	alignas(16)glm::vec3 lightcol;
	alignas(16)glm::vec3 ka;
	alignas(16)glm::vec3 kd;
	alignas(16)glm::vec4 kss;

};

struct PushConstants {
	alignas(16)VkDeviceAddress vb_addr;
	alignas(16)glm::mat4 model;
};

struct Light {
	glm::vec3 pos;
	glm::vec3 col;
};

struct Vertex {
	glm::vec3 pos;
	float uv_x;
	glm::vec3 col;
	float uv_y;
	alignas(16)glm::vec3 normal;

	bool operator==(const Vertex& other) const {
		return pos == other.pos && col == other.col && uv_x == other.uv_x && uv_y == other.uv_y;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.col) << 1)) >> 1) ^
				(hash<glm::vec2>()(glm::vec2(vertex.uv_x, vertex.uv_y)) << 1);
		}
	};
}

struct MeshData{
	BufferData index_buffer;
	BufferData vertex_buffer;
	VkDeviceAddress vertex_buffer_address;
	glm::mat4 model_mat;
	uint32_t index_count;
};
