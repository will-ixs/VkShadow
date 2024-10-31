#include "engine.h"

/*
* init glfw, create window
*/
void Engine::init_glfw() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(window_width, window_height, "Shadow", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	glfwSetWindowIconifyCallback(window, minimize_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
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
	ubo_data.view = glm::lookAt(glm::vec3(0.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo_data.proj = glm::perspective(glm::radians(70.0f), (16.0f / 9.0f), 0.01f, 100.0f);
	ubo_data.proj[1][1] *= -1;
	ubo_data.Q = glm::transpose(glm::inverse(ubo_data.model));

	sun.pos = glm::vec3(0.0f, 5.0f, 1.0f);
	sun.col = glm::vec3(0.5f, 0.5f, 0.5f);

	//light info
	ubo_data.lightpos = sun.pos;
	ubo_data.lightcol = sun.col;
	ubo_data.light_view = glm::lookAt(sun.pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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

	VkWriteDescriptorSet write_sets[] = { ubo_write, sampler_write, sampled_img_write };
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
	pipeline_builder.set_culling_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipeline_builder.set_multisampling_none();
	pipeline_builder.disable_blending();
	pipeline_builder.enable_depthtest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	//pipeline_builder.disable_depthtest();
	pipeline_builder.set_color_attachment_format(draw_image.format);
	pipeline_builder.set_depth_attachment_format(depth_image.format);
	mesh_pipeline = pipeline_builder.build_pipeline(device);

	vkDestroyShaderModule(device, vert_shader, nullptr);
	vkDestroyShaderModule(device, frag_shader, nullptr);
}

/*

*/
void Engine::init_shadow_pipeline() {
	//TODO
}


static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	engine->framebuffer_resized = true;
}
static void minimize_callback(GLFWwindow* window, int iconified) {
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	if (iconified)
	{
		//minimized
		engine->handle_minimize();
	}
	else
	{
		//restored
		engine->handle_restore();
	}
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	engine->handle_keypress(key, scancode, action, mods);
}
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	engine->handle_cursor_pos(xpos, ypos);
}