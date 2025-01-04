#include "engine.h"

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