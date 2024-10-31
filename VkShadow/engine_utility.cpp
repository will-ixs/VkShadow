#include "engine.h"
/*
create vert & ind vectors from OBJ
*/
void Engine::load_obj(const char* file_name) {
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
	LOG(1, "Loaded " + std::string(file_name));
}

void Engine::load_gltf(const char* file_name) {
	return;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	//TODO TinyGltf/Fastgltf Loader

	MeshData mesh = upload_mesh(vertices, indices);
	meshes.push_back(mesh);
	LOG(1, "Loaded " + std::string(file_name));
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
	mesh.index_count = (uint32_t)i.size();


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
		
	memcpy(staging_buffer.info.pMappedData, v.data(), vbuf_size);
	memcpy((char*)staging_buffer.info.pMappedData + vbuf_size, i.data(), ibuf_size);

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