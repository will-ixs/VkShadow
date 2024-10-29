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
	init_swapchain();
	init_draw_resources();
	init_commands();
	init_sync_structures();
	init_ubo_data();
	init_descriptors();
	init_pipelines();
	
	init_mesh(file_paths.bunny_model);
}
/*
* init glfw, create window
*/
void Engine::init_glfw() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(window_width, window_height, "Shadow", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	//TODO Renable
	//glfwSetWindowIconifyCallback(window, iconify_callback);
	//glfwSetKeyCallback(window, key_callback);
	//glfwSetCursorPosCallback(window, cursor_position_callback);
}

/*
* create instance, surface
* select phys device
* create device
* get queues (graphcs & present)
*/
void Engine::init_vulkan() {

	vkb::InstanceBuilder instance_builder;

	vkb::Result<vkb::SystemInfo> sys_info_query = vkb::SystemInfo::get_system_info();
	if (!sys_info_query) {
		logger.err("Failed to get sysinfo");
	}

	vkb::SystemInfo sys_info = sys_info_query.value();
	if (sys_info.validation_layers_available && use_validation_layers) {
		instance_builder.request_validation_layers();
	}
	if (sys_info.debug_utils_available && use_debug_messenger) {
		instance_builder.use_default_debug_messenger();
	}

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	instance_builder.enable_extensions(extensions);
	instance_builder.require_api_version(1, 3, 0);


	vkb::Result<vkb::Instance> instance_builder_return = instance_builder.build();
	if (!instance_builder_return) {
		logger.err("Failed to create Vulkan instance. Error : " + instance_builder_return.error().message() + "\n");
	}
	vkb::Instance vkb_instance = instance_builder_return.value();
	instance = vkb_instance;


	if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
	{
		logger.err("Failed to create surface");
	}

	VkPhysicalDeviceVulkan13Features features13{
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
	};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
	};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector phys_device_selector{ vkb_instance };
	vkb::Result<vkb::PhysicalDevice> physical_device_selector_return = phys_device_selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
		.set_required_features_12(features12)
		.set_surface(surface)
		.select();
	if (!physical_device_selector_return) {
		logger.err("Failed to select device" + physical_device_selector_return.error().message() + "\n");
	}
	vkb::PhysicalDevice vkb_phys_device = physical_device_selector_return.value();
	phys_device = vkb_phys_device;

	vkb::DeviceBuilder device_builder{ vkb_phys_device };
	vkb::Result<vkb::Device> device_builder_return = device_builder.build();
	if (!device_builder_return) {
		logger.err("Failed to build device" + device_builder_return.error().message() + "\n");
	}
	vkb::Device vkb_device = device_builder_return.value();
	device = vkb_device;

	vkb::Result<VkQueue> graphics_queue_return = vkb_device.get_queue(vkb::QueueType::graphics);
	if (!graphics_queue_return) {
		logger.err("Failed to acquire graphics queue" + graphics_queue_return.error().message() + "\n");
	}
	graphics_queue = graphics_queue_return.value();
	graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
	transfer_queue_family = vkb_device.get_queue_index(vkb::QueueType::transfer).value();


	vkb::Result<VkQueue> present_queue_return = vkb_device.get_queue(vkb::QueueType::present);
	if (!present_queue_return) {
		logger.err("Failed to acquire present queue" + present_queue_return.error().message() + "\n");
	}
	present_queue = present_queue_return.value();

	vkb::Result<VkQueue> transfer_queue_return = vkb_device.get_queue(vkb::QueueType::transfer);
	if (!transfer_queue_return) {
		logger.err("Failed to acquire transfer queue" + transfer_queue_return.error().message() + "\n");
	}
	transfer_queue = transfer_queue_return.value();
	
	VmaAllocatorCreateInfo vma_info = {};
	vma_info.physicalDevice = phys_device;
	vma_info.device = device;
	vma_info.instance = instance;
	vma_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&vma_info, &vma_allocator);
}

/*
create swapchain
*/
void Engine::init_swapchain() {
	create_swapchain(window_width, window_height);
}

/*
allocate draw & depth image
create their image views
allocate shadowmap & image view
create & map UBO
create sampler
*/
void Engine::init_draw_resources() {
	//Initialize draw image
	VkExtent3D draw_img_extent = {};
	draw_img_extent.width = window_width;
	draw_img_extent.height = window_height;
	draw_img_extent.depth = 1;
	
	draw_extent.width = window_width;
	draw_extent.height = window_height;

	draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image.extent = draw_img_extent;

	VkImageUsageFlags draw_img_uses = {};
	draw_img_uses |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_img_uses |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_img_uses |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo draw_img_info = {};
	draw_img_info.imageType = VK_IMAGE_TYPE_2D;
	draw_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	draw_img_info.pNext = nullptr;
	draw_img_info.format = draw_image.format;
	draw_img_info.extent = draw_image.extent;
	draw_img_info.usage = draw_img_uses;
	draw_img_info.mipLevels = 1;
	draw_img_info.arrayLayers = 1;
	draw_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
	draw_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;

	VmaAllocationCreateInfo draw_img_alloc_info = {};
	draw_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	draw_img_alloc_info.flags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vmaCreateImage(vma_allocator, &draw_img_info, &draw_img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);

	VkImageViewCreateInfo draw_view_info = {};
	draw_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	draw_view_info.pNext = nullptr;
	draw_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	draw_view_info.image = draw_image.image;
	draw_view_info.format = draw_image.format;
	draw_view_info.subresourceRange.baseMipLevel = 0;
	draw_view_info.subresourceRange.levelCount = 1;
	draw_view_info.subresourceRange.baseArrayLayer = 0;
	draw_view_info.subresourceRange.layerCount = 1;
	draw_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkCreateImageView(device, &draw_view_info, nullptr, &draw_image.view);


	//Initialize depth image for draw image
	depth_image.format = VK_FORMAT_D16_UNORM;
	depth_image.extent = draw_img_extent;

	VkImageUsageFlags depth_img_uses = {};
	depth_img_uses |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_img_info = {};
	depth_img_info.imageType = VK_IMAGE_TYPE_2D;
	depth_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depth_img_info.pNext = nullptr;
	depth_img_info.format = depth_image.format;
	depth_img_info.extent = depth_image.extent;
	depth_img_info.usage = depth_img_uses;
	depth_img_info.mipLevels = 1;
	depth_img_info.arrayLayers = 1;
	depth_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	vmaCreateImage(vma_allocator, &depth_img_info, &draw_img_alloc_info, &depth_image.image, &depth_image.allocation, nullptr);

	VkImageViewCreateInfo depth_view_info = {};
	depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depth_view_info.pNext = nullptr;
	depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depth_view_info.image = depth_image.image;
	depth_view_info.format = depth_image.format;
	depth_view_info.subresourceRange.baseMipLevel = 0;
	depth_view_info.subresourceRange.levelCount = 1;
	depth_view_info.subresourceRange.baseArrayLayer = 0;
	depth_view_info.subresourceRange.layerCount = 1;
	depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	vkCreateImageView(device, &depth_view_info, nullptr, &depth_image.view);

	//Initialize depth image for shadowmap
	VkExtent3D shadowmap_extent = {};
	shadowmap_extent.width = 1024;
	shadowmap_extent.height = 1024;
	shadowmap_extent.depth = 1;
	
	shadowmap_image.format = VK_FORMAT_D16_UNORM;
	shadowmap_image.extent = shadowmap_extent;

	VkImageUsageFlags shadowmap_usage = {};
	shadowmap_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	shadowmap_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	VkImageCreateInfo shadowmap_img_info = {};
	shadowmap_img_info.imageType = VK_IMAGE_TYPE_2D;
	shadowmap_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	shadowmap_img_info.pNext = nullptr;
	shadowmap_img_info.format = depth_image.format;
	shadowmap_img_info.extent = depth_image.extent;
	shadowmap_img_info.usage = shadowmap_usage;
	shadowmap_img_info.mipLevels = 1;
	shadowmap_img_info.arrayLayers = 1;
	shadowmap_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
	shadowmap_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	vmaCreateImage(vma_allocator, &shadowmap_img_info, &draw_img_alloc_info, &shadowmap_image.image, &shadowmap_image.allocation, nullptr);

	VkImageViewCreateInfo shadowmap_view_info = {};
	shadowmap_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	shadowmap_view_info.pNext = nullptr;
	shadowmap_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	shadowmap_view_info.image = shadowmap_image.image;
	shadowmap_view_info.format = shadowmap_image.format;
	shadowmap_view_info.subresourceRange.baseMipLevel = 0;
	shadowmap_view_info.subresourceRange.levelCount = 1;
	shadowmap_view_info.subresourceRange.baseArrayLayer = 0;
	shadowmap_view_info.subresourceRange.layerCount = 1;
	shadowmap_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	vkCreateImageView(device, &shadowmap_view_info, nullptr, &shadowmap_image.view);

	//Sampler
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.pNext = nullptr;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	vkCreateSampler(device, &sampler_info, nullptr, &shadowmap_sampler);
	
	//UBO
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.size = bufferSize;
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VmaAllocationCreateInfo allocation_info = {};
	allocation_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
	vmaCreateBuffer(vma_allocator, &buffer_info, &allocation_info, &ubo.buffer, &ubo.allocation, &ubo.info);
}	

/*
pools and buffers
*/
void Engine::init_commands() {
	frames.resize(FRAMES_IN_FLIGHT);

	VkCommandPoolCreateInfo command_pool_info = {};
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.pNext = nullptr;
	command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	command_pool_info.queueFamilyIndex = graphics_queue_family;

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkCreateCommandPool(device, &command_pool_info, nullptr, &frames[i].command_pool);

		VkCommandBufferAllocateInfo cmd_alloc_info = {};
		cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_alloc_info.pNext = nullptr;
		cmd_alloc_info.commandPool = frames[i].command_pool;
		cmd_alloc_info.commandBufferCount = 1;
		cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(device, &cmd_alloc_info, &frames[i].command_buffer);
	}
	VkCommandPoolCreateInfo transfer_pool_info = {};
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.pNext = nullptr;
	command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	command_pool_info.queueFamilyIndex = transfer_queue_family;
	vkCreateCommandPool(device, &command_pool_info, nullptr, &single_time_pool);
}

/*
sync structures
*/
void Engine::init_sync_structures() {
	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphore_info.pNext = nullptr;
	semaphore_info.flags = 0;

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkCreateFence(device, &fence_info, nullptr, &frames[i].render_fence);

		vkCreateSemaphore(device, &semaphore_info, nullptr, &frames[i].render_semaphore);
		vkCreateSemaphore(device, &semaphore_info, nullptr, &frames[i].swapcahin_semaphore);
	}
}

/*
sets default values for ubo
*/
void Engine::init_ubo_data() {
	ubo_data = {};
	ubo_data.model = glm::mat4(1.0f);
	ubo_data.view = glm::lookAt(glm::vec3(0.0f, 2.0f, 2.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo_data.proj = glm::perspective(glm::radians(70.0f), (16.0f / 9.0f), 1000.0f, 0.1f);
	ubo_data.proj[1][1] *= -1;
	ubo_data.Q = glm::transpose(glm::inverse(ubo_data.model));

	sun.pos = glm::vec3(0.0f, 5.0f, 1.0f);
	sun.col = glm::vec3(0.5f, 0.5f, 0.5f);

	//light info
	ubo_data.lightpos = sun.pos;
	ubo_data.lightcol = sun.col;
	ubo_data.light_view = glm::lookAt(sun.pos, glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo_data.light_proj = glm::perspective(glm::radians(60.0f), 1.0f, 10000.0f, 0.1f);
	ubo_data.light_proj[1][1] *= -1;

	//temp hardcoded material 
	ubo_data.ka = glm::vec3(0.2f, 0.2f, 0.2f);
	ubo_data.ks = glm::vec3(0.8f, 0.7f, 0.7f);
	ubo_data.kd = glm::vec3(1.0f, 1.0f, 1.0f);
	ubo_data.s = 10.0f;

	memcpy(ubo.info.pMappedData, &ubo_data, sizeof(UniformBufferObject));
}

/*
descriptor writes
*/
void Engine::init_descriptors() {

	std::vector<VkDescriptorPoolSize> pool_sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1}
	};
	descriptor_builder.init_pool(device, pool_sizes);

	descriptor_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
	descriptor_builder.add_binding(1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	descriptor_builder.add_binding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);

	descriptor_builder.create_layout(device, &global_layout);
	descriptor_builder.allocate_set(device, global_layout, &global_set);

	//Ubo
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = ubo.buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(UniformBufferObject);

	VkWriteDescriptorSet ubo_write = {};
	ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ubo_write.pNext = nullptr;
	ubo_write.dstBinding = 0;
	ubo_write.dstSet = global_set;
	ubo_write.descriptorCount = 1;
	ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_write.pBufferInfo = &buffer_info;

	//Sampler
	VkDescriptorImageInfo sampler_info = {};
	sampler_info.sampler = shadowmap_sampler;

	VkWriteDescriptorSet sampler_write = {};
	sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	sampler_write.pNext = nullptr;
	sampler_write.dstBinding = 1;
	sampler_write.dstSet = global_set;
	sampler_write.descriptorCount = 1;
	sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampler_write.pImageInfo = &sampler_info;

	//SampledImage
	VkDescriptorImageInfo sampled_img_info = {};
	sampled_img_info.imageView = shadowmap_image.view;
	sampled_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet sampled_img_write = {};
	sampled_img_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	sampled_img_write.pNext = nullptr;
	sampled_img_write.dstBinding = 2;
	sampled_img_write.dstSet = global_set;
	sampled_img_write.descriptorCount = 1;
	sampled_img_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampled_img_write.pImageInfo = &sampled_img_info;

	VkWriteDescriptorSet write_sets[] = {ubo_write, sampler_write, sampled_img_write};
	vkUpdateDescriptorSets(device, 3, write_sets, 0, nullptr);
	
}

//Pipelines
void Engine::init_pipelines() {
	init_mesh_pipeline();
	//init_shadow_pipeline();
}
/*
creates mesh pipeline & layout
*/
void Engine::init_mesh_pipeline() {
	VkShaderModule vert_shader;
	if (!load_shader(device, &vert_shader, file_paths.mesh_vert)) {
		logger.err("Failed to create mesh vertex shader.");
	}
	LOG(3, "Loaded mesh vertex shader.");

	VkShaderModule frag_shader;
	if (!load_shader(device, &frag_shader, file_paths.mesh_frag)) {
		logger.err("Failed to create mesh fragment shader.");
	}
	LOG(3, "Loaded mesh fragment shader.");

	VkPushConstantRange pc_range = {};
	pc_range.offset = 0;
	pc_range.size = sizeof(PushConstants);
	pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pNext = nullptr;
	layout_info.flags = 0;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &global_layout;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &pc_range;

	VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &mesh_pipeline_layout));

	pipeline_builder = {};
	pipeline_builder.pipeline_layout = mesh_pipeline_layout;
	pipeline_builder.set_shaders(vert_shader, frag_shader);
	pipeline_builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipeline_builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipeline_builder.set_culling_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipeline_builder.set_multisampling_none();
	pipeline_builder.disable_blending();
	//pipeline_builder.enable_depthtest(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipeline_builder.disable_depthtest();
	pipeline_builder.set_color_attachment_format(draw_image.format);
	pipeline_builder.set_depth_attachment_format(depth_image.format);
	mesh_pipeline = pipeline_builder.build_pipeline(device);

	vkDestroyShaderModule(device, vert_shader, nullptr);
	vkDestroyShaderModule(device, frag_shader, nullptr);
}

/*
nothing yet
*/
void Engine::init_shadow_pipeline() {

}

/*
create vert & ind vectors from OBJ
*/
void Engine::init_mesh(const char* file_name) {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file_name)) {
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.uv_x = attrib.texcoords[2 * index.texcoord_index + 0];
			vertex.uv_y = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];

			vertex.col = { 1.0f, 1.0f, 1.0f };

			vertex.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}

	MeshData mesh = upload_mesh(vertices, indices);
	meshes.push_back(mesh);
}

/*
true if vkShaderModule successfully created, assigned to out_shader
*/
bool Engine::load_shader(VkDevice device, VkShaderModule* out_shader, const char* file_path) {
	std::ifstream file(file_path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return false;
	}
	size_t file_size = (size_t)file.tellg();
	std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

	file.seekg(0);
	file.read((char*)buffer.data(), file_size);
	file.close();

	VkShaderModuleCreateInfo shader_mod_info = {};
	shader_mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_mod_info.pNext = nullptr;
	shader_mod_info.codeSize = buffer.size() * sizeof(uint32_t);
	shader_mod_info.pCode = buffer.data();

	VkShaderModule module = {};
	if (vkCreateShaderModule(device, &shader_mod_info, nullptr, &module) != VK_SUCCESS) {
		return false;
	}
	*out_shader = module;
	return true;
}

/*
create cpu staging buffer, copy data into it, copy to GPU buffers
*/
MeshData Engine::upload_mesh(std::span<Vertex> v, std::span<uint32_t> i) {
	size_t vbuf_size = v.size() * sizeof(Vertex);
	size_t ibuf_size = i.size() * sizeof(uint32_t);

	MeshData mesh = {};
	mesh.index_count = i.size();


	VkBufferCreateInfo vbuf_info = {};
	vbuf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vbuf_info.pNext = nullptr;
	vbuf_info.size = vbuf_size;
	vbuf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VkBufferCreateInfo ibuf_info = {};
	ibuf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ibuf_info.pNext = nullptr;
	ibuf_info.size = ibuf_size;
	ibuf_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBufferCreateInfo sbuf_info = {};
	sbuf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbuf_info.pNext = nullptr;
	sbuf_info.size = vbuf_size + ibuf_size;
	sbuf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


	VmaAllocationCreateInfo buf_alloc_gpu = {};
	buf_alloc_gpu.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	buf_alloc_gpu.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationCreateInfo buf_alloc_cpu = {};
	buf_alloc_cpu.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	buf_alloc_cpu.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	BufferData staging_buffer = {};

	vmaCreateBuffer(vma_allocator, &vbuf_info, &buf_alloc_gpu, &mesh.vertex_buffer.buffer, &mesh.vertex_buffer.allocation, &mesh.vertex_buffer.info);

	vmaCreateBuffer(vma_allocator, &ibuf_info, &buf_alloc_gpu, &mesh.index_buffer.buffer, &mesh.index_buffer.allocation, &mesh.index_buffer.info);
	
	vmaCreateBuffer(vma_allocator, &sbuf_info, &buf_alloc_cpu, &staging_buffer.buffer, &staging_buffer.allocation, &staging_buffer.info);


	VkBufferDeviceAddressInfo vdev_addr_info = {};
	vdev_addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	vdev_addr_info.buffer = mesh.vertex_buffer.buffer;
	mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &vdev_addr_info);

	void* staging_data = staging_buffer.allocation->GetMappedData();

	memcpy(staging_data, v.data(), vbuf_size);
	memcpy((char*)staging_data + vbuf_size, i.data(), ibuf_size);

	VkCommandBuffer cmd = begin_single_time_transfer();

	VkBufferCopy vert_copy = {};
	vert_copy.size = vbuf_size;
	vert_copy.srcOffset = 0;
	vert_copy.dstOffset = 0;

	VkBufferCopy ind_copy = {};
	ind_copy.size = ibuf_size;
	ind_copy.srcOffset = vbuf_size;
	ind_copy.dstOffset = 0;

	vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.vertex_buffer.buffer, 1, &vert_copy);
	vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.index_buffer.buffer, 1, &ind_copy);

	end_single_time_transfer(cmd);

	vmaDestroyBuffer(vma_allocator, staging_buffer.buffer, staging_buffer.allocation);

	return mesh;
}


VkCommandBuffer Engine::begin_single_time_transfer() {
	VkCommandBufferAllocateInfo cmd_alloc = {};
	cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_alloc.commandPool = single_time_pool;
	cmd_alloc.commandBufferCount = 1;

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(device, &cmd_alloc, &cmd);

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmd, &begin);

	return cmd;
}
void Engine::end_single_time_transfer(VkCommandBuffer cmd) {
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	vkQueueSubmit(transfer_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transfer_queue); //switch to fence?

	vkFreeCommandBuffers(device, single_time_pool, 1, &cmd);
}

void Engine::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout src_layout, VkImageLayout dst_layout) {
	VkImageMemoryBarrier2 image_barrier = {};
	image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_barrier.pNext = nullptr;

	image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	image_barrier.oldLayout = src_layout;
	image_barrier.newLayout = dst_layout;

	VkImageAspectFlags aspect_mask = (dst_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageSubresourceRange sub_image{};
	sub_image.aspectMask = aspect_mask;
	sub_image.baseMipLevel = 0;
	sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
	sub_image.baseArrayLayer = 0;
	sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

	image_barrier.subresourceRange = sub_image;
	image_barrier.image = image;

	VkDependencyInfo dep_info = {};
	dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dep_info.pNext = nullptr;

	dep_info.imageMemoryBarrierCount = 1;
	dep_info.pImageMemoryBarriers = &image_barrier;

	vkCmdPipelineBarrier2(cmd, &dep_info);

	return;
}
void Engine::copy_image(VkCommandBuffer cmd, VkImage src_image, VkImage dst_image, VkExtent2D src_extent, VkExtent2D dst_extent) {
	VkImageBlit2 blit = {};
	blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blit.pNext = nullptr;
	blit.srcOffsets[1].x = src_extent.width;
	blit.srcOffsets[1].y = src_extent.height;
	blit.srcOffsets[1].z = 1;
	blit.dstOffsets[1].x = dst_extent.width;
	blit.dstOffsets[1].y = dst_extent.height;
	blit.dstOffsets[1].z = 1;
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.srcSubresource.mipLevel = 0;
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;
	blit.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blit_info = {};
	blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blit_info.pNext = nullptr;
	blit_info.srcImage = src_image;
	blit_info.dstImage = dst_image;
	blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit_info.filter = VK_FILTER_LINEAR;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit;

	vkCmdBlitImage2(cmd, &blit_info);
}
//Swapchain
void Engine::create_swapchain(uint32_t width, uint32_t height) {
	vkb::SwapchainBuilder swapchain_builder{ phys_device, device, surface };
	vkb::Result<vkb::Swapchain> vkb_swapchain_res = swapchain_builder
		.set_desired_format(
			VkSurfaceFormatKHR{
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			})
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

	if (!vkb_swapchain_res) {
		logger.err("Failed to create swapchain.");
	}
	vkb::Swapchain vkb_swapchain = vkb_swapchain_res.value();

	swapchain_extent = vkb_swapchain.extent;
	swapchain = vkb_swapchain.swapchain;
	swapchain_images = vkb_swapchain.get_images().value();
	swapchain_image_views = vkb_swapchain.get_image_views().value();
	LOG(2, "Succesfully created swapchain");
}
void Engine::resize_swapchain() {
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);
	destroy_swapchain();
	create_swapchain(width, height);

}
void Engine::destroy_swapchain() {
	vkDestroySwapchainKHR(device, swapchain, nullptr);

	for (int i = 0; i < swapchain_image_views.size(); i++) {
		vkDestroyImageView(device, swapchain_image_views[i], nullptr);
	}
}

void Engine::draw() {
	VK_CHECK(vkWaitForFences(device, 1, &frames.at(frame_number).render_fence, VK_TRUE, 1000000000));

	uint32_t swapchain_index;
	VkResult acquire_res = vkAcquireNextImageKHR(device, swapchain, 1000000000, frames.at(frame_number).swapcahin_semaphore, nullptr, &swapchain_index);
	if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_swapchain();
		return;
	}
	else if (acquire_res != VK_SUCCESS && acquire_res != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acqurie swapchain image!");
	}

	VK_CHECK(vkResetFences(device, 1, &frames.at(frame_number).render_fence));
	VkCommandBuffer cmd = frames.at(frame_number).command_buffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.pNext = nullptr;
	cmd_begin_info.pInheritanceInfo = nullptr;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

	transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	transition_image(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	
	draw_geo(cmd);

	transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	transition_image(cmd, swapchain_images.at(swapchain_index), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copy_image(cmd, draw_image.image, swapchain_images.at(swapchain_index), draw_extent, swapchain_extent);

	transition_image(cmd, swapchain_images.at(swapchain_index), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmd_submit_info = {};
	cmd_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_submit_info.pNext = nullptr;
	cmd_submit_info.deviceMask = 0;
	cmd_submit_info.commandBuffer = cmd;

	VkSemaphoreSubmitInfo wait_semaphore_info = {};
	wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait_semaphore_info.pNext = nullptr;
	wait_semaphore_info.semaphore = frames.at(frame_number).swapcahin_semaphore;
	wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
	wait_semaphore_info.deviceIndex = 0;
	wait_semaphore_info.value = 1;

	VkSemaphoreSubmitInfo signal_semaphore_info = {};
	signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_semaphore_info.pNext = nullptr;
	signal_semaphore_info.semaphore = frames.at(frame_number).render_semaphore;
	signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	signal_semaphore_info.deviceIndex = 0;
	signal_semaphore_info.value = 1;

	VkSubmitInfo2 submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreInfoCount = 1;
	submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_submit_info;

	VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit_info, frames.at(frame_number).render_fence));

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.swapchainCount = 1;
	present_info.waitSemaphoreCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pWaitSemaphores = &frames.at(frame_number).render_semaphore;
	present_info.pImageIndices = &swapchain_index;

	VkResult present_res = vkQueuePresentKHR(graphics_queue, &present_info);
	if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR || resize_requested) {
		resize_requested = false;
		resize_swapchain();
	}
	else if (present_res != VK_SUCCESS) {
		logger.err("Failed to present to swapchain");
	};

	frame_number = (frame_number + 1) % FRAMES_IN_FLIGHT;
	frame_counter++;
}

void Engine::draw_geo(VkCommandBuffer cmd) {
	VkClearValue clear_value = {};
	clear_value.color = { {0.0f, 0.0f, 0.0f, 0.0f} };
	clear_value.depthStencil = { 0.0f, 0 };

	VkRenderingAttachmentInfo color_attachment = {};
	color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_attachment.pNext = nullptr;
	color_attachment.imageView = draw_image.view;
	color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depth_attachment = {};
	depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_attachment.pNext = nullptr;
	depth_attachment.imageView = depth_image.view;
	depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.clearValue = clear_value;

	VkRenderingInfo rendering_info = {};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.pNext = nullptr;
	rendering_info.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, draw_extent };
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_attachment;
	rendering_info.pDepthAttachment = &depth_attachment;
	rendering_info.pStencilAttachment = nullptr;

	vkCmdBeginRendering(cmd, &rendering_info);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 0, 1, &global_set, 0, nullptr);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = draw_extent.width;
	viewport.height = draw_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = draw_extent.width;
	scissor.extent.height = draw_extent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);


	PushConstants pcs;
	for (const MeshData &mesh : meshes) {
		pcs.vb_addr = mesh.vertex_buffer_address;
		vkCmdPushConstants(cmd, mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pcs);
		vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, 0);
	}
	vkCmdEndRendering(cmd);
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


static void framebuffer_resize_callback(GLFWwindow * window, int width, int height) {
		Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
		engine->framebuffer_resized = true;
}
//TODO callbacks
static void iconify_callback(GLFWwindow* window, int iconified)
{
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (iconified)
	{
		//engine->handle_minimize();
		//minimized
	}
	else
	{
		//engine->handle_restore();
		//restored
	}
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	//engine->handle_keypress(int key, int scancode, int action, int mods);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	//engine->handle_cursor_pos(double xpos, double ypos)
}