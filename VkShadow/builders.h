#pragma once
//Descriptors
struct DescriptorBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	VkDescriptorPool pool;

	void add_binding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shader_stages) {
		VkDescriptorSetLayoutBinding new_binding = {};
		new_binding.binding = binding;
		new_binding.descriptorCount = 1;
		new_binding.descriptorType = type;
		new_binding.stageFlags = shader_stages;

		bindings.push_back(new_binding);
	}

	void clear_bindings() {
		bindings.clear();
	}

	VkDescriptorSetLayout create_layout(VkDevice device, VkDescriptorSetLayout* out_layout, VkDescriptorSetLayoutCreateFlags layout_flags = 0) {
		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.pNext = nullptr;
		layout_info.pBindings = bindings.data();
		layout_info.bindingCount = (uint32_t)bindings.size();
		layout_info.flags = layout_flags;

		VkDescriptorSetLayout set_layout = {};
		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, out_layout));
		return set_layout;
	}

	void init_pool(VkDevice device, std::vector<VkDescriptorPoolSize> pool_sizes, VkDescriptorPoolCreateFlags pool_flags = 0) {
		uint32_t max_sets = 0;
		for (VkDescriptorPoolSize p : pool_sizes) {
			max_sets += p.descriptorCount;
		}

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = pool_flags;
		pool_info.maxSets = max_sets;
		pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
		pool_info.pPoolSizes = pool_sizes.data();

		VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
	}

	void reset_pool(VkDevice device, VkDescriptorPoolResetFlags pool_flags = 0) {
		VK_CHECK(vkResetDescriptorPool(device, pool, pool_flags));
	}

	VkDescriptorSet allocate_set(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorSet* out_set) {
		VkDescriptorSetAllocateInfo set_info = {};
		set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		set_info.pNext = nullptr;
		set_info.descriptorPool = pool;
		set_info.descriptorSetCount = 1;
		set_info.pSetLayouts = &layout;

		VkDescriptorSet descriptor_set = {};
		VK_CHECK(vkAllocateDescriptorSets(device, &set_info, out_set));
		return descriptor_set;
	}

};

//Pipelines
struct PipelineBuilder {
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState color_blending;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkPipelineRenderingCreateInfo rendering_info;
	VkFormat color_attachment_format;
	VkPipelineLayout pipeline_layout;

	PipelineBuilder() { clear(); };

	void clear(){
		shader_stages.clear();

		input_assembly = {};
		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

		rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

		color_blending = {};

		multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

		depth_stencil = {};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

		rendering_info = {};
		rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

		color_attachment_format = VK_FORMAT_UNDEFINED;

		pipeline_layout = {};
	}
	void set_shaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
		shader_stages.clear();
		VkPipelineShaderStageCreateInfo vertex_info = {};
		vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertex_info.pNext = nullptr;
		vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertex_info.module = vertex_shader;
		vertex_info.pName = "main";

		VkPipelineShaderStageCreateInfo fragment_info = {};
		fragment_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragment_info.pNext = nullptr;
		fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragment_info.module = fragment_shader;
		fragment_info.pName = "main";

		shader_stages = {vertex_info, fragment_info};
	}
	void set_topology(VkPrimitiveTopology topology) {
		input_assembly.topology = topology;
		input_assembly.primitiveRestartEnable = VK_FALSE;
	}
	void set_polygon_mode(VkPolygonMode mode) {
		rasterizer.polygonMode = mode;
		rasterizer.lineWidth = 1.0f;
	}
	void set_culling_mode(VkCullModeFlags mode, VkFrontFace face) {
		rasterizer.cullMode = mode;
		rasterizer.frontFace = face;
	}
	void set_multisampling_none() {
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
	}
	void disable_blending() {
		color_blending.blendEnable = VK_FALSE;
		color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	}
	/*
	VK_BLEND_FACTOR_ONE = additive
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = alphablend
	*/
	void enable_blending(VkBlendFactor dst_blend_factor) {
		color_blending.blendEnable = VK_TRUE;
		color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blending.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blending.dstColorBlendFactor = dst_blend_factor;
		color_blending.colorBlendOp = VK_BLEND_OP_ADD;

		color_blending.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blending.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blending.alphaBlendOp = VK_BLEND_OP_ADD;

	}
	void disable_depthtest() {
		depth_stencil.depthTestEnable = VK_FALSE;
		depth_stencil.depthWriteEnable = VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable = VK_FALSE;
		depth_stencil.front = {};
		depth_stencil.back = {};
		depth_stencil.minDepthBounds = 0.0f;
		depth_stencil.maxDepthBounds = 1.0f;
	}
	void enable_depthtest(VkBool32 depth_write_enable, VkCompareOp op) {
		depth_stencil.depthTestEnable = VK_FALSE;
		depth_stencil.depthWriteEnable = depth_write_enable;
		depth_stencil.depthCompareOp = op;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable = VK_FALSE;
		depth_stencil.front = {};
		depth_stencil.back = {};
		depth_stencil.minDepthBounds = 0.0f;
		depth_stencil.maxDepthBounds = 1.0f;
	}
	void set_color_attachment_format(VkFormat format) {
		color_attachment_format = format;
		rendering_info.pColorAttachmentFormats = &color_attachment_format;
		rendering_info.colorAttachmentCount = 1;
	}
	void set_depth_attachment_format(VkFormat format) {
		rendering_info.depthAttachmentFormat = format;
	}
	VkPipeline build_pipeline(VkDevice device) {
		VkPipelineViewportStateCreateInfo viewport_state = {};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.pNext = nullptr;
		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		//Only rendering to one color attachment, no blending
		VkPipelineColorBlendStateCreateInfo colorblending_info = {};
		colorblending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorblending_info.pNext = nullptr;
		colorblending_info.logicOpEnable = VK_FALSE;
		colorblending_info.logicOp = VK_LOGIC_OP_COPY;
		colorblending_info.attachmentCount = 1;
		colorblending_info.pAttachments = &color_blending;

		VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		//Define dynamic state
		VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.pDynamicStates = &state[0];
		dynamic_state_info.dynamicStateCount = 2;

		VkGraphicsPipelineCreateInfo graphics_pipeline_info = {};
		graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		//set pnext to render info for dynamic rendering
		graphics_pipeline_info.pNext = &rendering_info;

		graphics_pipeline_info.stageCount = (uint32_t)shader_stages.size();
		graphics_pipeline_info.pStages = shader_stages.data();
		graphics_pipeline_info.pVertexInputState = &vertex_input_info;
		graphics_pipeline_info.pInputAssemblyState = &input_assembly;
		graphics_pipeline_info.pViewportState = &viewport_state;
		graphics_pipeline_info.pRasterizationState = &rasterizer;
		graphics_pipeline_info.pMultisampleState = &multisampling;
		graphics_pipeline_info.pColorBlendState = &colorblending_info;
		graphics_pipeline_info.pDepthStencilState = &depth_stencil;
		graphics_pipeline_info.layout = pipeline_layout;
		graphics_pipeline_info.pDynamicState = &dynamic_state_info;

		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
			return VK_NULL_HANDLE; // failed to create graphics pipeline
		}
		else {
			return pipeline;
		}
	}
};