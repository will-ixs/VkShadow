#include "engine.h"

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

/*

*/
void Engine::draw_geo(VkCommandBuffer cmd) {
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
	depth_attachment.clearValue.depthStencil.depth = 1.0f;

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
	for (const MeshData& mesh : meshes) {
		pcs.vb_addr = mesh.vertex_buffer_address;
		vkCmdPushConstants(cmd, mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pcs);
		vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, 0);
	}
	vkCmdEndRendering(cmd);
}