#pragma once

//#define DISPLAY_QUILT
#ifdef DISPLAY_QUILT
//!< キルトを自前でレンダリングしている場合、最左右のみを表示する [Display most left, right quilt, only in case self rendering quilt]
#define DISPLAY_MOST_LR_QUILT
#endif

#define TO_RADIAN(x) ((x) * std::numbers::pi_v<float> / 180.0f)
#define CHECKDIMENSION(_TileXY) if (_TileXY > TileDimensionMax) { BREAKPOINT(); }

class ViewsVK : public VK
{
private:
	using Super = VK;
public:
	virtual void Init() override {
		CreateProjectionMatrices();
		{
			const auto Pos = glm::vec3(0.0f, 0.0f, 1.0f);
			const auto Tag = glm::vec3(0.0f);
			const auto Up = glm::vec3(0.0f, 1.0f, 0.0f);
			View = glm::lookAt(Pos, Tag, Up);
			CreateViewMatrices();
		}

		Super::Init();
	}

	virtual void CreateViewports() override {
		//!< [Pass0]
		const auto TileX = GetTileX(), TileY = GetTileY();
		const auto W = QuiltX / TileX, H = QuiltY / TileY;
		const auto Ext2D = VkExtent2D({ .width = static_cast<uint32_t>(W), .height = static_cast<uint32_t>(H) });
		for (auto i = 0; i < TileY; ++i) {
			const auto Y = QuiltY - H * i;
			for (auto j = 0; j < TileX; ++j) {
				const auto X = j * W;
				QuiltViewports.emplace_back(VkViewport({ .x = static_cast<float>(X), .y = static_cast<float>(Y), .width = static_cast<float>(W), .height = -static_cast<float>(H), .minDepth = 0.0f, .maxDepth = 1.0f }));
				QuiltScissorRects.emplace_back(VkRect2D({ VkOffset2D({.x = static_cast<int32_t>(X), .y = static_cast<int32_t>(Y - H) }), Ext2D }));
			}
		}

		//!< [Pass1]
		Super::CreateViewports();
	}

	static constexpr float ToRadian(const float Degree) { return Degree * std::numbers::pi_v<float> / 180.0f; }

	float GetOffsetAngle(const int i) const { return static_cast<float>(i) * OffsetAngleCoef - HalfViewCone; }
	void CreateProjectionMatrix(const int i) {
		//!< 左右方向にずれている角度 (ラジアン)
		const auto OffsetAngle = GetOffsetAngle(i);
		//!< 左右方向にずれている距離
		const auto OffsetX = CameraDistance * std::tan(OffsetAngle);

		const auto DisplayAspect = GetDisplayAspect();
		auto Prj = glm::perspective(Fov, DisplayAspect, 0.1f, 100.0f);
		Prj[2][0] += OffsetX / (CameraSize * DisplayAspect);

		ProjectionMatrices.emplace_back(Prj);
	}
	void CreateProjectionMatrices() {
		for (auto i = 0; i < TileXY; ++i) {
			CreateProjectionMatrix(i);
		}
	}
	void CreateViewMatrix(const int i) {
		const auto OffsetAngle = GetOffsetAngle(i);
		const auto OffsetX = CameraDistance * std::tan(OffsetAngle);
		const auto OffsetLocal = glm::vec3(View * glm::vec4(OffsetX, 0.0f, CameraDistance, 1.0f));
		ViewMatrices.emplace_back(glm::translate(View, OffsetLocal));
	}
	void CreateViewMatrices() {
		for (auto i = 0; i < TileXY; ++i) {
			CreateViewMatrix(i);
		}
	}

protected:
	//!< 16 Views * 5 Draw call
	static constexpr int TileDimensionMax = 16 * 5;

	virtual uint32_t GetMaxViewports() const { return SelectedPhysDevice.second.PDP.limits.maxViewports; }
	uint32_t GetViewportDrawCount() const {
		const auto XY = static_cast<uint32_t>(TileXY);
		const auto ViewportCount = GetMaxViewports();
		return XY / ViewportCount + ((XY % ViewportCount) ? 1 : 0);
	}
	uint32_t GetViewportSetOffset(const uint32_t i) const { return GetMaxViewports() * i; }
	uint32_t GetViewportSetCount(const uint32_t i) const {
		return (std::min)(static_cast<uint32_t>(TileXY) - GetViewportSetOffset(i), GetMaxViewports());
	}

	virtual float GetDisplayAspect() const { return 0.75f; }
	virtual int GetTileX() const { return 8; }
	virtual int GetTileY() const { return 6; }

	virtual void UpdateViewProjectionBuffer() {
		const auto Count = (std::min)(static_cast<size_t>(TileXY), std::size(ViewProjectionBuffer));
		for (size_t i = 0; i < Count; ++i) {
			ViewProjectionBuffer[i] = ProjectionMatrices[i] * ViewMatrices[i];
		}
	}
	virtual void UpdateWorldBuffer() {
		//auto X = 1.0f, Y = 1.0f;
		auto X = 11 * 0.25f, Y = 6 * 0.65f;
		WorldBuffer = glm::scale(glm::mat4(1.0f), glm::vec3(X, Y, 1.0f));
	}

	virtual BufferAndDeviceMemory& GetViewProjectionBuffer() = 0;
	virtual BufferAndDeviceMemory& GetLenticularBuffer() = 0;
	virtual BufferAndDeviceMemory& GetWorldBuffer() = 0;

	virtual Texture& GetRTColor() = 0;
	virtual Texture& GetRTDepth() = 0;

	virtual Texture& GetColorMap() = 0;
	virtual Texture& GetDisplacementMap() = 0;

protected:
	int QuiltX = 3360, QuiltY = 3360;
	float HalfViewCone = 40.0f; //!< この時点では素の角度を指定 [For now save as degree]
	long WinX = 0, WinY = 0;
	unsigned long WinWidth = 1536, WinHeight = 2048;

	int TileXY = 0;
	float OffsetAngleCoef = 0.0f;
	const float Fov = ToRadian(14.0f);
	const float CameraSize = 5.0f;
	const float CameraDistance = -CameraSize / std::tan(Fov / 2.0f);

	std::vector<VkViewport> QuiltViewports;
	std::vector<VkRect2D> QuiltScissorRects;

	std::vector<glm::mat4> ProjectionMatrices;
	std::vector<glm::mat4> ViewMatrices;

	glm::mat4 View;

	glm::mat4 ViewProjectionBuffer[TileDimensionMax];
	glm::mat4 WorldBuffer;
};

class DisplacementVK : public ViewsVK 
{
private:
	using Super = ViewsVK;
public:
	DisplacementVK() : Super() {
		SetParam(HardWareEnum::Go);
	}

	virtual void CreateGeometry() override {
		Super::CreateGeometry({
			//!< [Pass0] オフスクリーン描画 (テッセレーション、マルチビュー) 用 
			Super::GeometryCreateInfo({.VtxCount = 1, .InstCount = 1 }),

			//!< [Pass1] フルスクリーン描画用
			Super::GeometryCreateInfo({.VtxCount = 4, .InstCount = 1 }),
		});
	}

	virtual void CreateUniformBuffer() override {
		//!< [Pass0] マルチビュープロジェクションバッファ
		CreateHostVisibleBuffer(UniformBuffers.emplace_back(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ViewProjectionBuffer), &ViewProjectionBuffer);
	
		//!< [Pass1] レンチキュラー、ワールドバッファ
		CreateHostVisibleBuffer(UniformBuffers.emplace_back(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(LenticularBuffer), &LenticularBuffer);
		CreateHostVisibleBuffer(UniformBuffers.emplace_back(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(WorldBuffer), &WorldBuffer);	
	}
	
	virtual void CreateDisplacementTexture() = 0;
	virtual void CreateTexture() override {
		//!< [Pass0] レンダーターゲット (デプステスト有) [0, 1]
		CreateTexture_Render(VK_FORMAT_B8G8R8A8_UNORM, QuiltX, QuiltY);
		CreateTexture_Depth(VK_FORMAT_D24_UNORM_S8_UINT, QuiltX, QuiltY);

		//!< ディスプレースメントマップ (カラー、深度) 読み込み [2, 3]
		CreateDisplacementTexture();
	}
	void CreateCommandBuffer() override {
		//!< プライマリ
		AllocateCommandBuffers(CreateCommandPool(PrimaryCommandBuffers), std::size(Swapchain.ImageAndViews), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		
		//!< [Pass0] オフスクリーン (テッセレーション、マルチビュー) 描画用セカンダリ
		AllocateCommandBuffers(CreateCommandPool(SecondaryCommandBuffers), std::size(Swapchain.ImageAndViews), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		//!< [Pass1] フルスクリーン描画用セカンダリ
		AllocateCommandBuffers(CreateCommandPool(SecondaryCommandBuffers), std::size(Swapchain.ImageAndViews), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	}
	virtual void CreatePipelineLayout() override {
		CreateSamplerLR();

		//!< [Pass0] [0]マルチビュープロジェクションバッファ、[1]テクスチャマップ、[2]ディスプレースメントマップ、[3]ワールドバッファ
		const std::array ISs = { Samplers[0] };
		Super::CreatePipelineLayout({
			CreateDescriptorSetLayout({
				//!< [0](UB) ViewProjection
				VkDescriptorSetLayoutBinding({.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT, .pImmutableSamplers = nullptr }),
				//!< [1](S2D) Color
				VkDescriptorSetLayoutBinding({.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(std::size(ISs)), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .pImmutableSamplers = std::data(ISs) }),
				//!< [2](S2D) Displacement
				VkDescriptorSetLayoutBinding({.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(std::size(ISs)), .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, .pImmutableSamplers = std::data(ISs) }),
				//!< [3](UB) World
				VkDescriptorSetLayoutBinding({.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, .pImmutableSamplers = nullptr }),
			})
		});

		//!< [Pass1] [0s]キルトテクスチャマップ、[1]レンチキュラーバッファ
		Super::CreatePipelineLayout({
			CreateDescriptorSetLayout({
				//!< [0](S2D) Quilt
				VkDescriptorSetLayoutBinding({.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(std::size(ISs)), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .pImmutableSamplers = std::data(ISs) }),
				//!< [1](UB) Lenticular
				VkDescriptorSetLayoutBinding({.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .pImmutableSamplers = nullptr }),
			})
		});
	}
	
	virtual void CreateRenderPass() override {
		//!< [Pass0] オフスクリーン (テッセレーション、マルチビュー) (デプステスト有)
		Super::CreateRenderPass_Depth(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		//!< [Pass1] フルスクリーン (クリア不要)
		Super::CreateRenderPass_None();
	}

	virtual void CreatePipeline() override {
		Pipelines.emplace_back();
		Pipelines.emplace_back();

		std::vector<std::thread> Threads = {};

		const auto BasePath = std::filesystem::path("..") / ".." / "Shaders";
		//!< [Pass0] オフスクリーン (テッセレーション、マルチビュー)
		const std::array SMs_Pass0 = {
			CreateShaderModule(BasePath / "Views.vert.spv"),
			CreateShaderModule(BasePath / "Views.frag.spv"),
			CreateShaderModule(BasePath / "Views.tese.spv"),
			CreateShaderModule(BasePath / "Views.tesc.spv"),
			CreateShaderModule(BasePath / "Views.geom.spv"),
		};
		Threads.emplace_back(std::thread([&] {
			VK::CreatePipeline(Pipelines[0],
				SMs_Pass0[0], SMs_Pass0[1], SMs_Pass0[2], SMs_Pass0[3], SMs_Pass0[4],
				PipelineLayouts[0],
				RenderPasses[0]);
			}));

		//!< [Pass1] フルスクリーン 
		const std::array SMs_Pass1 = {
			CreateShaderModule(BasePath / "Quilt.vert.spv"),
#ifdef DISPLAY_QUILT
			CreateShaderModule(BasePath / "QuiltRaw.frag.spv"),
#else
			CreateShaderModule(BasePath / "Quilt.frag.spv"),
#endif
		};
		Threads.emplace_back(std::thread([&] {
			VK::CreatePipeline(Pipelines[1],
				SMs_Pass1[0], SMs_Pass1[1],
				PipelineLayouts[1],
				RenderPasses[1]);
			}));

		for (auto& i : Threads) { i.join(); }

		for (const auto i : SMs_Pass1) {
			vkDestroyShaderModule(Device, i, nullptr);
		}
		for (const auto i : SMs_Pass0) {
			vkDestroyShaderModule(Device, i, nullptr);
		}
	}

	virtual void CreateFramebuffer() override {
		//!< オフスクリーン + フルスクリーン (スワップチェイン数)
		Framebuffers.reserve(1 + std::size(Swapchain.ImageAndViews));

		//!< [Pass0] オフスクリーン (テッセレーション、マルチビュー)
		{
			const auto RP = RenderPasses[0];
			const auto RTCV = GetRTColor().ImageView.second;
			const auto RTDV = GetRTDepth().ImageView.second;

			const std::array IVs = { RTCV, RTDV };
			const VkFramebufferCreateInfo FCI = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.renderPass = RP,
				.attachmentCount = static_cast<uint32_t>(std::size(IVs)), .pAttachments = std::data(IVs),
				.width = static_cast<uint32_t>(QuiltX), .height = static_cast<uint32_t>(QuiltY),
				.layers = 1
			};
			VERIFY_SUCCEEDED(vkCreateFramebuffer(Device, &FCI, nullptr, &Framebuffers.emplace_back()));
		}

		//!< [Pass1] フルスクリーン
		Super::CreateFramebuffer(RenderPasses[1]);
	}

	void CreateDescriptor_Pass0() {
		const auto DSL = DescriptorSetLayouts[0];

		AllocateDescriptorSets(
			CreateDescriptorPool({
				//!< マルチビュープロジェクションバッファ
				VkDescriptorPoolSize({.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1 }),
				//!< テクスチャマップ、ディスプレースメントマップ
				VkDescriptorPoolSize({.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 2 }),
				//!< ワールドバッファ
				VkDescriptorPoolSize({.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 }),
				}),
			{ DSL });
		{
			const auto DynamicOffset = GetMaxViewports() * sizeof(ViewProjectionBuffer[0]);

			const auto UB_0 = GetViewProjectionBuffer().first;
			const auto IV_0 = GetColorMap().ImageView.second;
			const auto IV_1 = GetDisplacementMap().ImageView.second;
			const auto UB_2 = GetWorldBuffer().first;

			const auto DS = DescriptorSets[0];

			struct DescriptorUpdateInfo {
				VkDescriptorBufferInfo DBI_0[1];
				VkDescriptorImageInfo DII_0[1];
				VkDescriptorImageInfo DII_1[1];
				VkDescriptorBufferInfo DBI_1[1];
			};
			const auto DUI = DescriptorUpdateInfo({
				//!< ダイナミックオフセット (UNIFORM_BUFFER_DYNAMIC) を使用する場合、.range には VK_WHOLE_SIZE では無くオフセット毎に使用するサイズを指定する
				.DBI_0 = VkDescriptorBufferInfo({.buffer = UB_0, .offset = 0, .range = DynamicOffset }),
				.DII_0 = VkDescriptorImageInfo({.sampler = VK_NULL_HANDLE, .imageView = IV_0, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }),
				.DII_1 = VkDescriptorImageInfo({.sampler = VK_NULL_HANDLE, .imageView = IV_1, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }),
				.DBI_1 = VkDescriptorBufferInfo({.buffer = UB_2, .offset = 0, .range = VK_WHOLE_SIZE }),
				});
			VkDescriptorUpdateTemplate DUT;
			CreateDescriptorUpdateTemplate(DUT, {
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 0, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DBI_0)), .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
					.offset = offsetof(DescriptorUpdateInfo, DBI_0), .stride = sizeof(DUI)
				}),
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 1, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DII_0)), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.offset = offsetof(DescriptorUpdateInfo, DII_0), .stride = sizeof(DUI)
				}),
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 2, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DII_1)), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.offset = offsetof(DescriptorUpdateInfo, DII_1), .stride = sizeof(DUI)
				}),
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 3, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DBI_1)), .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.offset = offsetof(DescriptorUpdateInfo, DBI_1), .stride = sizeof(DUI)
				}),
				}, DSL);
			vkUpdateDescriptorSetWithTemplate(Device, DS, DUT, &DUI);
			vkDestroyDescriptorUpdateTemplate(Device, DUT, nullptr);
		}
	}
	void CreateDescriptor_Pass1() {
		const auto DSL = DescriptorSetLayouts[1];

		AllocateDescriptorSets(
			CreateDescriptorPool({
				//!< キルトテクスチャマップ
				VkDescriptorPoolSize({.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 }),
				//!< レンチキュラーバッファ
				VkDescriptorPoolSize({.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 }),
				}),
			{ DSL });
		{			
			const auto IV = GetRTColor().ImageView.second;
			const auto UB = GetLenticularBuffer().first;

			const auto DS = DescriptorSets[1];

			struct DescriptorUpdateInfo {
				VkDescriptorImageInfo DII[1];
				VkDescriptorBufferInfo DBI[1];
			};
			const auto DUI = DescriptorUpdateInfo({
				.DII = VkDescriptorImageInfo({.sampler = VK_NULL_HANDLE, .imageView = IV, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }),
				.DBI = VkDescriptorBufferInfo({.buffer = UB, .offset = 0, .range = VK_WHOLE_SIZE}),
				});
			VkDescriptorUpdateTemplate DUT;
			CreateDescriptorUpdateTemplate(DUT, {
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 0, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DII)), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.offset = offsetof(DescriptorUpdateInfo, DII), .stride = sizeof(DUI)
				}),
				VkDescriptorUpdateTemplateEntry({
					.dstBinding = 1, .dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(std::size(DUI.DBI)), .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.offset = offsetof(DescriptorUpdateInfo, DBI), .stride = sizeof(DUI)
				}),
				}, DSL);
			vkUpdateDescriptorSetWithTemplate(Device, DS, DUT, &DUI);
			vkDestroyDescriptorUpdateTemplate(Device, DUT, nullptr);
		}
	}
	virtual void CreateDescriptor() override {
		//!< [Pass0] オフスクリーン (テッセレーション、マルチビュー)
		CreateDescriptor_Pass0();

		//!< [Pass1] フルスクリーン
		CreateDescriptor_Pass1();
	}

	void PopulateSecondaryCommandBuffer_Pass0(const int i) {
		const auto RP = RenderPasses[0];
		const auto CB = SecondaryCommandBuffers[0].second[i];

		const VkCommandBufferInheritanceInfo CBII = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext = nullptr,
			.renderPass = RP,
			.subpass = 0,
			.framebuffer = VK_NULL_HANDLE,
			.occlusionQueryEnable = VK_FALSE, .queryFlags = 0,
			.pipelineStatistics = 0,
		};
		const VkCommandBufferBeginInfo CBBI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			.pInheritanceInfo = &CBII
		};
		VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
			const auto PL = Pipelines[0];
			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

			const auto DynamicOffset = GetMaxViewports() * sizeof(ViewProjectionBuffer[0]);
			const VkBufferMemoryRequirementsInfo2 BMRI = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
				.pNext = nullptr,
				.buffer = GetViewProjectionBuffer().first,
			};
			VkMemoryRequirements2 MR = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
				.pNext = nullptr,
				.memoryRequirements = {},
			};
			vkGetBufferMemoryRequirements2(Device, &BMRI, &MR);
			const auto PLL = PipelineLayouts[0];
			const auto DS = DescriptorSets[0];
			const auto IDB = IndirectBuffers[0].first;

			//!< マルチビュー (キルト) 描画 
			for (uint32_t j = 0; j < GetViewportDrawCount(); ++j) {
				const auto Offset = GetViewportSetOffset(j);
				const auto Count = GetViewportSetCount(j);

				vkCmdSetViewport(CB, 0, Count, &QuiltViewports[Offset]);
				vkCmdSetScissor(CB, 0, Count, &QuiltScissorRects[Offset]);

				const std::array DSs = { DS };
				const std::array DynamicOffsets = { static_cast<uint32_t>(RoundUp(DynamicOffset * j, MR.memoryRequirements.alignment)) };
				vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PLL, 0, static_cast<uint32_t>(std::size(DSs)), std::data(DSs), static_cast<uint32_t>(std::size(DynamicOffsets)), std::data(DynamicOffsets));

				vkCmdDrawIndirect(CB, IDB, 0, 1, 0);
			}
		}VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
	}
	void PopulateSecondaryCommandBuffer_Pass1(const int i) {
		const auto RP = RenderPasses[1];
		const auto CB = SecondaryCommandBuffers[1].second[i];

		const VkCommandBufferInheritanceInfo CBII = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext = nullptr,
			.renderPass = RP,
			.subpass = 0,
			.framebuffer = VK_NULL_HANDLE,
			.occlusionQueryEnable = VK_FALSE, .queryFlags = 0,
			.pipelineStatistics = 0,
		};
		const VkCommandBufferBeginInfo SCBBI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			.pInheritanceInfo = &CBII
		};
		VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &SCBBI)); {
			const auto PL = Pipelines[1];
			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

			const auto PLL = PipelineLayouts[1];
			const auto DS = DescriptorSets[1];
			const auto IDB = IndirectBuffers[1].first;

			vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
			vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

			const std::array DSs = { DS };
			constexpr std::array<uint32_t, 0> DynamicOffsets = {};
			vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PLL, 0, static_cast<uint32_t>(std::size(DSs)), std::data(DSs), static_cast<uint32_t>(std::size(DynamicOffsets)), std::data(DynamicOffsets));

			vkCmdDrawIndirect(CB, IDB, 0, 1, 0);
		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
	}
	virtual void PopulateSecondaryCommandBuffer(const int i) override {
		//!<【Pass0】オフスクリーン (テッセレーション、マルチビュー)
		PopulateSecondaryCommandBuffer_Pass0(i);

		//!<【Pass1】フルスクリーン
		PopulateSecondaryCommandBuffer_Pass1(i);
	}
	virtual void PopulatePrimaryCommandBuffer_Update([[maybe_unused]] const int i) {
	}
	void PopulatePrimaryCommandBuffer_Pass0(const int i) {
		const auto CB = PrimaryCommandBuffers[0].second[i];

		const auto RP = RenderPasses[0];
		const auto FB = Framebuffers[0];

		constexpr std::array CVs = { VkClearValue({.color = { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } }), VkClearValue({.depthStencil = {.depth = 1.0f, .stencil = 0 } }) };
		const VkRenderPassBeginInfo RPBI = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = RP,
			.framebuffer = FB,
			.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = VkExtent2D({.width = static_cast<uint32_t>(QuiltX), .height = static_cast<uint32_t>(QuiltY) })}), //!< キルトサイズ
			.clearValueCount = static_cast<uint32_t>(size(CVs)), .pClearValues = std::data(CVs)
		};
		vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); {
			const auto SCB = SecondaryCommandBuffers[0].second[i];

			const std::array SCBs = { SCB };
			vkCmdExecuteCommands(CB, static_cast<uint32_t>(std::size(SCBs)), std::data(SCBs));
		} vkCmdEndRenderPass(CB);
	}
	void PopulatePrimaryCommandBuffer_Pass1(const int i) {
		const auto CB = PrimaryCommandBuffers[0].second[i];

		const auto RP = RenderPasses[1];
		const auto FB = Framebuffers[1 + i];

		constexpr std::array<VkClearValue, 0> CVs = {};
		const VkRenderPassBeginInfo RPBI = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = RP,
			.framebuffer = FB,
			.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = Swapchain.Extent }),
			.clearValueCount = static_cast<uint32_t>(std::size(CVs)), .pClearValues = std::data(CVs)
		};
		vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); {
			const auto SCB = SecondaryCommandBuffers[1].second[i];

			const std::array SCBs = { SCB };
			vkCmdExecuteCommands(CB, static_cast<uint32_t>(std::size(SCBs)), std::data(SCBs));
		} vkCmdEndRenderPass(CB);
	}
	virtual void PopulatePrimaryCommandBuffer(const int i) override {
		const auto CB = PrimaryCommandBuffers[0].second[i];

		constexpr VkCommandBufferBeginInfo CBBI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};
		VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
			//!< アップデート (テクスチャアニメーション等)
			PopulatePrimaryCommandBuffer_Update(i);

			//!<【Pass0】オフスクリーン (テッセレーション、マルチビュー)
			PopulatePrimaryCommandBuffer_Pass0(i);

			//!< バリア
			ImageMemoryBarrier(CB,
				GetRTColor().ImageView.first,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			//!<【Pass1】フルスクリーン
			PopulatePrimaryCommandBuffer_Pass1(i);

		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
	}

	virtual void OnUpdate() override {
		Super::OnUpdate();

		Super::UpdateViewProjectionBuffer();
		Super::UpdateWorldBuffer();

		CopyToHostVisibleMemory(GetViewProjectionBuffer().second, 0, sizeof(ViewProjectionBuffer), &ViewProjectionBuffer);
		CopyToHostVisibleMemory(GetLenticularBuffer().second, 0, sizeof(LenticularBuffer), &LenticularBuffer);
		CopyToHostVisibleMemory(GetWorldBuffer().second, 0, sizeof(WorldBuffer), &WorldBuffer);
	}

protected:
	virtual float GetDisplayAspect() const override { return LenticularBuffer.DisplayAspect; }
	virtual int GetTileX() const override { return LenticularBuffer.TileX; }
	virtual int GetTileY() const override { return LenticularBuffer.TileY; }

	virtual void UpdateLenticularBuffer(const int Column, const int Row, const int Width, const int Height) {
		LenticularBuffer.TileX = Column;
		LenticularBuffer.TileY = Row;
		QuiltX = Width;
		QuiltY = Height;
		const auto ViewWidth = static_cast<float>(QuiltX) / LenticularBuffer.TileX;
		const auto ViewHeight = static_cast<float>(QuiltY) / LenticularBuffer.TileY;
		LenticularBuffer.QuiltAspect = ViewWidth / ViewHeight;

		TileXY = LenticularBuffer.TileX * LenticularBuffer.TileY;
		CHECKDIMENSION(TileXY);
	}
	
	virtual BufferAndDeviceMemory& GetViewProjectionBuffer() override { return UniformBuffers[0]; }
	virtual BufferAndDeviceMemory& GetLenticularBuffer() override { return UniformBuffers[1]; }
	virtual BufferAndDeviceMemory& GetWorldBuffer() override { return UniformBuffers[2]; }

	virtual Texture& GetRTColor() override { return Textures[0]; }
	virtual Texture& GetRTDepth() override { return Textures[1]; }

	virtual Texture& GetColorMap() override { return Textures[2]; }
	virtual Texture& GetDisplacementMap() override { return Textures[3]; }

protected:
	enum class HardWareEnum {
		Standard = 0,
		Portrait = 4,
		Go = 10,
	};
	void SetParam(const HardWareEnum HWE = HardWareEnum::Go) {
		switch (HWE) {
		case HardWareEnum::Standard:
			LenticularBuffer.Pitch = 354.5659f;
			LenticularBuffer.Tilt = -13.943652f; // BridgeSDK : -13.9436522f
			LenticularBuffer.Center = -0.19972825f;
			LenticularBuffer.Subp = 0.00013020834f;

			LenticularBuffer.DisplayAspect = 1.6f;
			LenticularBuffer.TileX = 5;
			LenticularBuffer.TileY = 9;
			LenticularBuffer.QuiltAspect = 1.6f;

			QuiltX = 4096;
			QuiltY = 4096;
			HalfViewCone = 40.0f;
			WinWidth = 2560;
			WinHeight = 1600;
			break;
		case HardWareEnum::Portrait:
			LenticularBuffer.Pitch = 246.866f;
			LenticularBuffer.Tilt = -0.185377f; //!< BridgeSDK : -4.04581785f
			LenticularBuffer.Center = 0.565845f;
			LenticularBuffer.Subp = 0.000217014f;

			LenticularBuffer.DisplayAspect = 0.75f;
			LenticularBuffer.TileX = 8;
			LenticularBuffer.TileY = 6;
			LenticularBuffer.QuiltAspect = 0.75f;

			QuiltX = 3360;
			QuiltY = 3360;
			HalfViewCone = 40.0f;
			WinWidth = 1536;
			WinHeight = 2048;
			break;
		case HardWareEnum::Go:
			LenticularBuffer.Pitch = 234.218f;
			LenticularBuffer.Tilt = -0.26678094f; //!< BridgeSDK : -2.10847139f
			LenticularBuffer.Center = 0.131987f;
			LenticularBuffer.Subp = 0.000231481f;

			LenticularBuffer.DisplayAspect = 0.5625f;
			LenticularBuffer.TileX = 11;
			LenticularBuffer.TileY = 6;
			LenticularBuffer.QuiltAspect = 0.5625f;

			QuiltX = 4092;
			QuiltY = 4092;
			HalfViewCone = 54.0f;
			WinWidth = 1440;
			WinHeight = 2560;
			break;
		default:
			BREAKPOINT();
			break;
		}
		LenticularBuffer.InvView = 1;
		LenticularBuffer.Ri = 0;
		LenticularBuffer.Bi = 2;

		WinX = 0; //1920
		WinY = 0;
		
		{
			std::cout << "Pitch = " << LenticularBuffer.Pitch << std::endl;
			std::cout << "Tilt = " << LenticularBuffer.Tilt << std::endl;
			std::cout << "Center = " << LenticularBuffer.Center << std::endl;
			std::cout << "Subp =  " << LenticularBuffer.Subp << std::endl;

			std::cout << "DisplayAspect = " << LenticularBuffer.DisplayAspect << std::endl;
			std::cout << "InvView = " << LenticularBuffer.InvView << std::endl;
			std::cout << "Ri, Bi = " << LenticularBuffer.Ri << ", " << LenticularBuffer.Bi << std::endl;
			std::cout << "TileX, TileY = " << LenticularBuffer.TileX << ", " << LenticularBuffer.TileY << std::endl;
			std::cout << "QuiltAspect = " << LenticularBuffer.QuiltAspect << std::endl;
		}

		AdjustParam();
	}
	void AdjustParam() {
#ifdef DISPLAY_MOST_LR_QUILT
		LenticularBuffer.TileX = 2; LenticularBuffer.TileY = 1;
#endif

		//!< 調整 [Adjustment]
		{
			//!< DX では Y が上であり、(ここでは) VK も DX に合わせて Y が上にしている為、Tilt の値を正にすることで辻褄を合わせている [Here Y is up, so changing tilt value to positive]
			LenticularBuffer.Tilt *= -1.0f;
			LenticularBuffer.Center *= -1.0f;

			//!< InvView は真偽を符号にして保持しておく [Convert truth or falsehood to sign]
			LenticularBuffer.InvView = LenticularBuffer.InvView ? -1 : 1;
			//!< この時点では角度が入ってくるのでラジアン変換する [Convert degree to radian]
			//!< 都合がよいので半角にして覚えておく [Save half radian for convenience]
			HalfViewCone = TO_RADIAN(HalfViewCone) * 0.5f;
		}

		TileXY = LenticularBuffer.TileX * LenticularBuffer.TileY;
		CHECKDIMENSION(TileXY);
		OffsetAngleCoef = 2.0f * HalfViewCone / (TileXY - 1.0f);
	}

	struct alignas(16) LENTICULAR_BUFFER {
		float Pitch = 246.866f; 
		float Tilt = -0.185377f;
		float Center = 0.565845f;
		float Subp = 0.000217014f;

		float DisplayAspect = 0.75f;
		int InvView = 1;
		int Ri = 0, Bi = 2;
		int TileX = 8, TileY = 6;

		float QuiltAspect = 0.75f;
	};
	LENTICULAR_BUFFER LenticularBuffer;	
};

class DisplacementDDSVK : public DisplacementVK
{
private:
	using Super = DisplacementVK;
public:
	DisplacementDDSVK() {}
	DisplacementDDSVK(const std::filesystem::path& Color, const std::filesystem::path& Depth) : ColorImagePath(Color), DepthImagePath(Depth) {}

	virtual void CreateDisplacementTexture() override {
		const std::vector Paths = {
			PathAndPipelineStage({ ColorImagePath, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT }),
			PathAndPipelineStage({ DepthImagePath, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT }),
		};
		CreateGLITextures(PrimaryCommandBuffers[0].second[0], Paths);
	}
protected:
	std::filesystem::path ColorImagePath = std::filesystem::path("..") / ".." / "Textures" / "Rocks007_2K_Color.dds";
	std::filesystem::path DepthImagePath = std::filesystem::path("..") / ".." / "Textures" / "Rocks007_2K_Displacement.dds";
};

class DisplacementCVVK : public DisplacementVK
{
private:
	using Super = DisplacementVK;
public:
	DisplacementCVVK() {}
	DisplacementCVVK(const std::filesystem::path& Color, const std::filesystem::path& Depth) : ColorImagePath(Color), DepthImagePath(Depth) {}

	virtual void CreateDisplacementTexture() override {
		std::cout << cv::getBuildInformation() << std::endl;

		auto CvColor = cv::imread(std::data(ColorImagePath.string()));
		cv::cvtColor(CvColor, CvColor, cv::COLOR_BGR2RGBA);

		auto CvDepth = cv::imread(std::data(DepthImagePath.string()));
		cv::cvtColor(CvDepth, CvDepth, cv::COLOR_BGR2GRAY);

		//cv::imshow("Color", CvColor);
		//cv::imshow("Depth", CvDepth);

		const std::vector Paths = {
			CvMatAndFormatAndPipelineStage({ CvColor, VK_FORMAT_R8G8B8A8_UNORM, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT }),
			CvMatAndFormatAndPipelineStage({ CvDepth, VK_FORMAT_R8_UNORM, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT }),
		};
		CreateCVTextures(PrimaryCommandBuffers[0].second[0], Paths);
	}
protected:
	std::filesystem::path ColorImagePath = std::filesystem::path("..") / ".." / "Textures" / "Bricks091_1K-JPG_Color.jpg";
	std::filesystem::path DepthImagePath = std::filesystem::path("..") / ".." / "Textures" / "Bricks091_1K-JPG_Displacement.jpg";
};

class DisplacementCVRGBDVK : public DisplacementVK
{
private:
	using Super = DisplacementVK;
public:
	DisplacementCVRGBDVK() {}
	DisplacementCVRGBDVK(const std::filesystem::path& RGBD) : RGBDImagePath(RGBD) {}

	virtual void CreateDisplacementTexture() override {
		std::cout << cv::getBuildInformation() << std::endl;

		auto CvRGBD = cv::imread(std::data(RGBDImagePath.string()));
		const auto Cols = CvRGBD.cols / 2;

		auto CvColor = cv::Mat(CvRGBD, cv::Rect(0, 0, Cols, CvRGBD.rows));
		cv::cvtColor(CvColor, CvColor, cv::COLOR_BGR2RGBA);

		auto CvDepth = cv::Mat(CvRGBD, cv::Rect(Cols, 0, Cols, CvRGBD.rows));
		cv::cvtColor(CvDepth, CvDepth, cv::COLOR_BGR2GRAY);

		const std::vector Paths = {
			CvMatAndFormatAndPipelineStage({ CvColor, VK_FORMAT_R8G8B8A8_UNORM, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT }),
			CvMatAndFormatAndPipelineStage({ CvDepth, VK_FORMAT_R8_UNORM, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT }),
		};
		CreateCVTextures(PrimaryCommandBuffers[0].second[0], Paths);
	}
protected:
	std::filesystem::path RGBDImagePath = std::filesystem::path("..") / ".." / "Textures" / "Bricks076C_1K.png";
};


class AnimatedDisplacementVK : public DisplacementVK
{
private:
	using Super = DisplacementVK;
public:
	virtual void CreateDisplacementTexture() override {
		//!< アニメーションテクスチャマップ (ステージングバッファ付き) [2, 3]
		for (auto i = 0; i < 2; ++i) {
			VK::CreateTexture(VK_FORMAT_B8G8R8A8_UNORM, Width, Height, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			CreateHostVisibleBuffer(Textures.back().Staging.emplace_back(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(uint32_t) * Width * Height, nullptr);
		}
	}
	virtual void PopulatePrimaryCommandBuffer_Update(const int i) override {
		if (0 == i) {
			const auto CB = PrimaryCommandBuffers[0].second[i];

			//!< (ステージングから) テクスチャ更新コマンド
			PopulateCopyCommand(CB, Textures[2].Staging.back().first, Textures[2].ImageView.first, Width, Height, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			PopulateCopyCommand(CB, Textures[3].Staging.back().first, Textures[3].ImageView.first, Width, Height, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT);
		}
	}

	virtual void OnUpdate() override {
		Super::OnUpdate();

		//!< (テクスチャの) ステージングを更新
		std::array<uint32_t, Width * Height> Pattern;
		std::random_device RndDev;
		std::ranges::generate(Pattern, [&]() { return RndDev(); });
		CopyToHostVisibleMemory(Textures[2].Staging.back().second, 0, sizeof(Pattern), std::data(Pattern));
		CopyToHostVisibleMemory(Textures[3].Staging.back().second, 0, sizeof(Pattern), std::data(Pattern));

		//cv::Mat CvColor, CvDepth;
		//std::mutex Mutex;
		//{
		//	std::lock_guard Lock(Mutex);
		//
		//	CopyToHostVisibleMemory(Textures[2].Staging.back().second, 0, CvColor.total() * CvColor.elemSize(), CvColor.ptr());
		//	CopyToHostVisibleMemory(Textures[3].Staging.back().second, 0, CvDepth.total() * CvDepth.elemSize(), CvDepth.ptr());
		//}
	}
protected:
	static const uint32_t Width = 320, Height = 240;
};