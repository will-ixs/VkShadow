#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "engine.h"

Engine::Engine() {
	init();
}
Engine::Engine(int w, int h) 
	: window_width(w),  window_height(h)
{
	init();
}

Engine::~Engine() {
	std::pair quit("QUIT", MESHTYPE::UNDEFINED);
	mesh_queue.push(quit);

	for (MeshData mesh : meshes) {
		vmaDestroyBuffer(vma_allocator, mesh.vertex_buffer.buffer, mesh.vertex_buffer.allocation);
		vmaDestroyBuffer(vma_allocator, mesh.index_buffer.buffer, mesh.index_buffer.allocation);
	}

	vkDestroyPipelineLayout(device, mesh_pipeline_layout, nullptr);
	vkDestroyPipeline(device, mesh_pipeline, nullptr);

	vkDestroyDescriptorPool(device, descriptor_builder.pool, nullptr);
	vkDestroyDescriptorSetLayout(device, global_layout, nullptr);

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkDestroyFence(device, frames[i].render_fence, nullptr);
		vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
		vkDestroySemaphore(device, frames[i].swapcahin_semaphore, nullptr);

		vkDestroyCommandPool(device, frames[i].command_pool, nullptr);
	}
	vkDestroyCommandPool(device, single_time_pool, nullptr);

	
	destroy_swapchain();
	vkDestroyImageView(device, shadowmap_image.view, nullptr);
	vkDestroyImageView(device, draw_image.view, nullptr);
	vkDestroyImageView(device, depth_image.view, nullptr);
	vmaDestroyImage(vma_allocator, shadowmap_image.image, shadowmap_image.allocation);
	vmaDestroyImage(vma_allocator, draw_image.image, draw_image.allocation);
	vmaDestroyImage(vma_allocator, depth_image.image, depth_image.allocation);

	vmaDestroyBuffer(vma_allocator, ubo.buffer, ubo.allocation);

	vkDestroySampler(device, shadowmap_sampler, nullptr);

	vmaDestroyAllocator(vma_allocator);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void Engine::init() {
	init_glfw();
	init_vulkan();
	init_commands();

	mesh_queue.push(std::pair(file_paths.bunny_model, MESHTYPE::OBJ));
	std::thread t(mesh_uploader, this);
	t.detach();
		
	init_swapchain();
	init_draw_resources();
	init_sync_structures();
	init_ubo_data();
	init_descriptors();
	init_pipelines();
}


void Engine::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if (minimized) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		//TODO fps in window title
		//glfwSetWindowTitle(window, "さよなら絶望先生");

		draw();
	}

	vkDeviceWaitIdle(device);
}


static void mesh_uploader(Engine* engine) {
	while (true) {
		if (engine->mesh_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		
		std::pair<std::string, MESHTYPE> res = engine->mesh_queue.front();
		engine->mesh_queue.pop();
		if (res.first == "QUIT") {
			break;
		}

		switch (res.second)
		{
		case MESHTYPE::OBJ:
			engine->load_obj(res.first.c_str());
			break;
		case MESHTYPE::GLTF:
			engine->load_gltf(res.first.c_str());
			break;
		default:
			break;
		}
	}
	std::cout << "Mesh uploader exiting..." << std::endl;
}