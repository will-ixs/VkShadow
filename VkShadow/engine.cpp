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
	MeshResource quit = { 
		.file_path = "QUIT",
		.model_mat = glm::mat4(0),
		.type = MESHTYPE::UNDEFINED
	};
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

	mesh_thread.join();
}

void Engine::init() {
	init_glfw();
	init_vulkan();
	init_commands();

	mesh_queue.push(model_res.bunny);
	mesh_queue.push(model_res.teapot);
	mesh_queue.push(model_res.square);
	mesh_thread = std::thread(mesh_uploader, this);

	init_swapchain();
	init_draw_resources();
	init_sync_structures();
	init_ubo_data();
	init_descriptors();
	init_pipelines();
}


void Engine::run() {
	while (!glfwWindowShouldClose(window)) {
		auto start_time = std::chrono::high_resolution_clock::now();

		glfwPollEvents();

		if (minimized) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		draw();

		auto stop_time = std::chrono::high_resolution_clock::now();;
		std::ostringstream frame_time;
		frame_time << std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time);;
		std::string title = "Vulkan: " + frame_time.str();
		glfwSetWindowTitle(window, title.c_str());
	}

	vkDeviceWaitIdle(device);
}


static void mesh_uploader(Engine* engine) {
	while (true) {
		if (engine->mesh_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		
		MeshResource res = engine->mesh_queue.front();
		engine->mesh_queue.pop();
		if (res.file_path == "QUIT") {
			break;
		}

		switch (res.type)
		{
		case MESHTYPE::OBJ:
			engine->load_obj(res.file_path, res.model_mat);
			break;
		case MESHTYPE::GLTF:
			engine->load_gltf(res.file_path, res.model_mat);
			break;
		default:
			break;
		}
	}
	if (engine->logging_enabled) {
		engine->logger.log(0, "Mesh uploader exiting...");
	}
}