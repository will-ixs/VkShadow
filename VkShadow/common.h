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

struct {

	const char* mesh_vert = "E:/Proj/Vs/VkShadow/shaders/spirv/mesh.vert.spv";
	const char* mesh_frag = "E:/Proj/Vs/VkShadow/shaders/spirv/mesh.frag.spv";
	const char* shadow_vert = "E:/Proj/Vs/VkShadow/shaders/spirv/shadow.vert.spv";
	const char* shadow_frag = "E:/Proj/Vs/VkShadow/shaders/spirv/shadow.frag.spv";
	const char* bunny_model = "E:/Proj/Vs/VkShadow/models/bunny.obj";

	//const char* mesh_vert = "../../shaders/spirv/mesh.vert.spv";
	//const char* mesh_frag = "../../shaders/spirv/mesh.frag.spv";
	//const char* shadow_vert = "../../shaders/spirv/shadow.vert.spv";
	//const char* shadow_frag = "../../shaders/spirv/shadow.frag.spv";
	//const char* bunny_model = "../../models/bunny.obj";
} file_paths;

enum MESHTYPE
{
	OBJ,
	GLTF,
	UNDEFINED
};

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
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 Q;

	glm::mat4 light_view;
	glm::mat4 light_proj;
	alignas(16)glm::vec3 lightpos;
	alignas(16)glm::vec3 lightcol;
	alignas(16)glm::vec3 ka;
	alignas(16)glm::vec3 ks;
	glm::vec3 kd;
	float s;
};

struct PushConstants {
	VkDeviceAddress vb_addr;
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
	uint32_t index_count;
};
