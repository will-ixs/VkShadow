#pragma once
#include <stdexcept>
#include <span>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <glm/gtx/transform.hpp>

#include "vk_mem_alloc.h"
#include "VkBootstrap.h"
#include "GLFW/glfw3.h"

#include <stb_image.h>

#include <tiny_obj_loader.h>

#include "logger.h"
#include "utils.h"

#define FRAMES_IN_FLIGHT 2


class Engine {
public:
	Engine();
	Engine(int w, int h);
	~Engine();
	bool framebuffer_resized = false;
	bool use_validation_layers = false;
	bool use_debug_messenger = false;
	bool logging_enabled = true;
	bool minimized = false;
	void run();


	std::queue<std::string> mesh_queue;
	void init_mesh(const char* file_name);

private:
	//Window
	int window_width = 1280;
	int window_height = 720;

	Logger logger;

	//Init
	GLFWwindow* window;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice phys_device;
	VkDevice device;
	VmaAllocator vma_allocator;

	VkQueue graphics_queue;
	VkQueue present_queue;
	VkQueue transfer_queue;

	uint32_t graphics_queue_family;
	uint32_t transfer_queue_family;

	VkCommandPool single_time_pool;
	VkFence single_time_fence;
	
	//Swapchain Data
	VkSwapchainKHR swapchain;
	VkFormat swapchain_image_format;
	VkExtent2D swapchain_extent;

	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;

	bool resize_requested = false;
	uint32_t frame_number = 0;
	uint32_t frame_counter = 0;
	//Rendering Data
	std::vector<PerFrameData> frames;
	ImageData draw_image;
	VkExtent2D draw_extent;
	ImageData depth_image;

	ImageData shadowmap_image;
	VkSampler shadowmap_sampler;
	
	UniformBufferObject ubo_data;
	BufferData ubo;

	Light sun;
	std::vector<MeshData> meshes;

	//Descriptors
	DescriptorBuilder descriptor_builder = {};
	VkDescriptorSetLayout global_layout;
	VkDescriptorSet global_set;

	//Pipelines
	PipelineBuilder pipeline_builder;
	VkPipeline mesh_pipeline;
	VkPipelineLayout mesh_pipeline_layout;
	VkPipeline shadow_pipeline;
	VkPipelineLayout shadow_pipeline_layout;


	//---------------------------------//
	//Initialization
	void init();
	void init_glfw();
	void init_vulkan();				//instance,surface,device,queues
	void init_swapchain();
	void init_draw_resources();
	void init_commands();			//command pools & bufferse
	void init_sync_structures();
	
	void init_descriptors();
	void init_ubo_data();

	void init_pipelines();
	void init_mesh_pipeline();
	void init_shadow_pipeline();


	//Drawing
	void draw();
	void draw_geo(VkCommandBuffer cmd);

	//Utility
	bool load_shader(VkDevice device, VkShaderModule* out_shader, const char* file_path);
	MeshData upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices);

	VkCommandBuffer begin_single_time_transfer();
	void end_single_time_transfer(VkCommandBuffer cmd);

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout src, VkImageLayout dst);
	void copy_image(VkCommandBuffer cmd, VkImage src_image, VkImage dst_image, VkExtent2D src_extent, VkExtent2D dst_extent);

	//---------------------------------//
	//Swapchain management
	void create_swapchain(uint32_t width, uint32_t height);
	void resize_swapchain();
	void destroy_swapchain();

};
static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
static void iconify_callback(GLFWwindow* window, int iconified);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
static void mesh_uploader(Engine* engine);