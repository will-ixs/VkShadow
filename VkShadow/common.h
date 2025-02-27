#pragma once

#include <glm/mat4x4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vk_enum_string_helper.h>

//Macros
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
	const char* mesh_vert = "../../shaders/spirv/mesh.vert.spv";
	const char* mesh_frag = "../../shaders/spirv/mesh.frag.spv";
	const char* shadow_vert = "../../shaders/spirv/shadow.vert.spv";
	const char* shadow_frag = "../../shaders/spirv/shadow.frag.spv";
} shader_paths;

struct {
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

struct MeshData {
	BufferData index_buffer;
	BufferData vertex_buffer;
	VkDeviceAddress vertex_buffer_address;
	glm::mat4 model_mat;
	uint32_t index_count;
};

struct PerFrameData {
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	
	VkFence render_fence;
	VkSemaphore render_semaphore;
	VkSemaphore swapcahin_semaphore;
};

struct TransitionData {
	VkPipelineStageFlags2 src_mask;
	VkPipelineStageFlags2 dst_mask;
	VkAccessFlags2 src_acc;
	VkAccessFlags2 dst_acc;
	VkImageAspectFlags barrier_aspect;

	VkImageLayout src_layout;
	VkImageLayout dst_layout;
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
/* TODO
struct GeoUniformBufferObject {
	alignas(16)glm::mat4 view_proj;
	alignas(16)glm::mat4 Q;

	//temp change to device address for light buffer, lightsize, device address for material buffer
	alignas(16)glm::mat4 light_view;
	alignas(16)glm::mat4 light_proj;
	alignas(16)glm::vec3 lightpos;
	alignas(16)glm::vec3 lightcol;
	alignas(16)glm::vec3 ka;
	alignas(16)glm::vec3 kd;
	alignas(16)glm::vec4 kss;

};
struct ShadowmapUniformBufferObject {
	alignas(16)glm::mat4 light_view_proj;
};
*/
struct PushConstants {
	alignas(16)glm::mat4 model;
	alignas(16)VkDeviceAddress vb_addr;
	//alignas(8) uint32_t material_index
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
