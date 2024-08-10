#include <stdlib.h>
#include <fstream>
#include <array>
#include <algorithm>
#include <bitset>
#include <map>

#include "VK.h"
#include <GLFW/glfw3.h>

#ifdef _WIN64
#pragma comment(lib, "VK.lib")
#pragma comment(lib, "glfw3dll.lib")
#endif

//#define USE_BORDERLESS
//#define USE_FULLSCREEN

//!< コールバック (Callbacks)
static void GlfwErrorCallback(int Code, const char* Description)
{
	std::cerr << "Error code = " << Code << ", " << Description << std::endl;
}
static void GlfwKeyCallback(GLFWwindow* Window, int Key, [[maybe_unused]] int Scancode, int Action, [[maybe_unused]] int Mods)
{
	if (Key == GLFW_KEY_ESCAPE && Action == GLFW_PRESS) {
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}
}

class TriangleVK : public VK
{
private:
	using Super = VK;
public:
	virtual void CreateGeometry() override { 
		//!< バーテックスバッファ、ステージングの作成 (Create vertex buffer, staging)
		using PositonColor = std::pair<glm::vec3, glm::vec3>;
		const std::array Vertices = {
			PositonColor({ 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
			PositonColor({ -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
			PositonColor({ 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
		};
		CreateDeviceLocalBuffer(VertexBuffers.emplace_back(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Vertices));
		auto VertexStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		CreateHostVisibleBuffer(&VertexStagingBuffer.first, &VertexStagingBuffer.second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(Vertices), std::data(Vertices));

		//!< インデックスバッファ、ステージングの作成 (Create index buffer, staging)
		constexpr std::array Indices = {
			uint32_t(0), uint32_t(1), uint32_t(2)
		};
		CreateDeviceLocalBuffer(IndexBuffers.emplace_back(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Indices));
		auto IndexStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		CreateHostVisibleBuffer(&IndexStagingBuffer.first, &IndexStagingBuffer.second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(Indices), std::data(Indices));

		//!< インダイレクトバッファ、ステージングの作成 (Create indirect buffer, staging)
		constexpr VkDrawIndexedIndirectCommand DIIC = {
			.indexCount = static_cast<uint32_t>(std::size(Indices)),
			.instanceCount = 1,
			.firstIndex = 0,
			.vertexOffset = 0,
			.firstInstance = 0
		};
		CreateDeviceLocalBuffer(IndirectBuffers.emplace_back(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(DIIC));
		auto IndirectStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		CreateHostVisibleBuffer(&IndirectStagingBuffer.first, &IndirectStagingBuffer.second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(DIIC), &DIIC);

		//!< コピーコマンド作成 (Populate copy command)
		const auto& CB = CommandBuffers[0];
		constexpr VkCommandBufferBeginInfo CBBI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr
		};
		VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
			PopulateCopyCommand(CB, VertexStagingBuffer.first, VertexBuffers[0].first, sizeof(Vertices), VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			PopulateCopyCommand(CB, IndexStagingBuffer.first, IndexBuffers[0].first, sizeof(Indices), VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			PopulateCopyCommand(CB, IndirectStagingBuffer.first, IndirectBuffers[0].first, sizeof(DIIC), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		//!< コピーコマンド発行 (Submit copy command)
		{
			const std::array<VkSemaphoreSubmitInfo, 0> WaitSSIs = {};
			const std::array CBSIs = {
				VkCommandBufferSubmitInfo({
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
					.pNext = nullptr,
					.commandBuffer = CB,
					.deviceMask = 0
					})
			};
			const std::array<VkSemaphoreSubmitInfo, 0> SignalSSIs = {};
			const std::array SIs = {
				VkSubmitInfo2({
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
					.pNext = nullptr,
					.flags = 0,
					.waitSemaphoreInfoCount = static_cast<uint32_t>(std::size(WaitSSIs)), .pWaitSemaphoreInfos = std::data(WaitSSIs),
					.commandBufferInfoCount = static_cast<uint32_t>(std::size(CBSIs)), .pCommandBufferInfos = std::data(CBSIs),
					.signalSemaphoreInfoCount = static_cast<uint32_t>(std::size(SignalSSIs)), .pSignalSemaphoreInfos = std::data(SignalSSIs)
				})
			};
			VERIFY_SUCCEEDED(vkQueueSubmit2(GraphicsQueue.first, static_cast<uint32_t>(std::size(SIs)), std::data(SIs), VK_NULL_HANDLE));
			VERIFY_SUCCEEDED(vkQueueWaitIdle(GraphicsQueue.first));
		}

		if (VK_NULL_HANDLE != VertexStagingBuffer.second) {
			vkFreeMemory(Device, VertexStagingBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != VertexStagingBuffer.first) {
			vkDestroyBuffer(Device, VertexStagingBuffer.first, nullptr);
		}
		if (VK_NULL_HANDLE != IndexStagingBuffer.second) {
			vkFreeMemory(Device, IndexStagingBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != IndexStagingBuffer.first) {
			vkDestroyBuffer(Device, IndexStagingBuffer.first, nullptr);
		}
		if (VK_NULL_HANDLE != IndirectStagingBuffer.second) {
			vkFreeMemory(Device, IndirectStagingBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != IndirectStagingBuffer.first) {
			vkDestroyBuffer(Device, IndirectStagingBuffer.first, nullptr);
		}
	}
	virtual void CreatePipeline() override {
		const std::array SMs = {
			CreateShaderModule(std::filesystem::path(".") / "Glfw.vert.spv"),
			CreateShaderModule(std::filesystem::path(".") / "Glfw.frag.spv"),
		};
		const std::array PSSCIs = {
			VkPipelineShaderStageCreateInfo({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = SMs[0], .pName = "main", .pSpecializationInfo = nullptr }),
			VkPipelineShaderStageCreateInfo({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = SMs[1], .pName = "main", .pSpecializationInfo = nullptr }),
		};
		const std::vector VIBDs = {
			VkVertexInputBindingDescription({.binding = 0, .stride = sizeof(glm::vec3) + sizeof(glm::vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }),
		};
		const std::vector VIADs = {
			VkVertexInputAttributeDescription({.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 }),
			VkVertexInputAttributeDescription({.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(glm::vec3) }),
		};
		constexpr VkPipelineRasterizationStateCreateInfo PRSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE, .depthBiasConstantFactor = 0.0f, .depthBiasClamp = 0.0f, .depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f
		};
		constexpr VkPipelineDepthStencilStateCreateInfo PDSSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = VkStencilOpState({
				.failOp = VK_STENCIL_OP_KEEP,
				.passOp = VK_STENCIL_OP_KEEP,
				.depthFailOp = VK_STENCIL_OP_KEEP,
				.compareOp = VK_COMPARE_OP_NEVER,
				.compareMask = 0, .writeMask = 0, .reference = 0
				}),
			.back = VkStencilOpState({
				.failOp = VK_STENCIL_OP_KEEP,
				.passOp = VK_STENCIL_OP_KEEP,
				.depthFailOp = VK_STENCIL_OP_KEEP,
				.compareOp = VK_COMPARE_OP_ALWAYS,
				.compareMask = 0, .writeMask = 0, .reference = 0
				}),
				.minDepthBounds = 0.0f, .maxDepthBounds = 1.0f
		};
		constexpr std::array PCBASs = {
			VkPipelineColorBlendAttachmentState({
				.blendEnable = VK_FALSE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_ONE, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE, .colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			}),
		};
		const VkPipelineVertexInputStateCreateInfo PVISCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = static_cast<uint32_t>(std::size(VIBDs)), .pVertexBindingDescriptions = std::data(VIBDs),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(VIADs)), .pVertexAttributeDescriptions = std::data(VIADs)
		};
		constexpr VkPipelineInputAssemblyStateCreateInfo PIASCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			.primitiveRestartEnable = VK_FALSE
		};
		constexpr VkPipelineTessellationStateCreateInfo PTSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.patchControlPoints = 0
		};
		constexpr VkPipelineViewportStateCreateInfo PVSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1, .pViewports = nullptr,
			.scissorCount = 1, .pScissors = nullptr
		};
		constexpr VkSampleMask SM = 0xffffffff;
		const VkPipelineMultisampleStateCreateInfo PMSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE, .minSampleShading = 0.0f,
			.pSampleMask = &SM,
			.alphaToCoverageEnable = VK_FALSE, .alphaToOneEnable = VK_FALSE
		};
		const VkPipelineColorBlendStateCreateInfo PCBSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE, .logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = static_cast<uint32_t>(std::size(PCBASs)), .pAttachments = std::data(PCBASs),
			.blendConstants = { 1.0f, 1.0f, 1.0f, 1.0f }
		};
		constexpr std::array DSs = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, };
		const VkPipelineDynamicStateCreateInfo PDSCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = static_cast<uint32_t>(std::size(DSs)), .pDynamicStates = std::data(DSs)
		};
		const std::array GPCIs = {
			VkGraphicsPipelineCreateInfo({
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = nullptr,
#ifdef _DEBUG
				.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT,
#else
				.flags = 0,
#endif
				.stageCount = static_cast<uint32_t>(std::size(PSSCIs)), .pStages = std::data(PSSCIs),
				.pVertexInputState = &PVISCI,
				.pInputAssemblyState = &PIASCI,
				.pTessellationState = &PTSCI,
				.pViewportState = &PVSCI,
				.pRasterizationState = &PRSCI,
				.pMultisampleState = &PMSCI,
				.pDepthStencilState = &PDSSCI,
				.pColorBlendState = &PCBSCI,
				.pDynamicState = &PDSCI,
				.layout = PipelineLayouts[0],
				.renderPass = RenderPasses[0], .subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE, .basePipelineIndex = -1
			})
		};
		VERIFY_SUCCEEDED(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, static_cast<uint32_t>(std::size(GPCIs)), std::data(GPCIs), nullptr, &Pipelines.emplace_back()));

		for (const auto i : SMs) {
			vkDestroyShaderModule(Device, i, nullptr);
		}
	}
	virtual void PopulateCommand() override { 
		for (auto i = 0; i < std::size(Swapchain.ImageAndViews); ++i) {
			const auto FB = Framebuffers[i];
			const auto CB = CommandBuffers[i];
			constexpr VkCommandBufferBeginInfo CBBI = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr
			};
			VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
				vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
				vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

				vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipelines[0]);

				constexpr std::array CVs = { VkClearValue({.color = { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } }) };
				const VkRenderPassBeginInfo RPBI = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.pNext = nullptr,
					.renderPass = RenderPasses[0],
					.framebuffer = FB,
					.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = Swapchain.Extent }),
					.clearValueCount = static_cast<uint32_t>(size(CVs)), .pClearValues = data(CVs)
				};
				vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
					const std::array VBs = { VertexBuffers[0].first};
					const std::array Offsets = { VkDeviceSize(0) };
					vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), std::data(Offsets));
					vkCmdBindIndexBuffer(CB, IndexBuffers[0].first, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexedIndirect(CB, IndirectBuffers[0].first, 0, 1, 0);
				} vkCmdEndRenderPass(CB);
			} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		}
	}
};

int main()
{
	//!< 初期化 (Initialize)
	if (!glfwInit()) {
		std::cerr << "glfwInit() failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	//!< Vulkan サポートの有無 (Vulkan supported)
	if (GLFW_FALSE == glfwVulkanSupported()) {
		std::cerr << "Vulkan not supported" << std::endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//!< モニター列挙 (Enumerate monitors)
	int MonitorCount;
	const auto PriMon = glfwGetPrimaryMonitor();
	const auto Monitors = glfwGetMonitors(&MonitorCount);
	for (auto i = 0; i < MonitorCount; ++i) {
		const auto Mon = Monitors[i];
		std::cout << ((PriMon == Mon) ? "P" : " ") << "[" << i << "] " << glfwGetMonitorName(Mon) << std::endl;
		int VideModeCount;
		const auto VideoModes = glfwGetVideoModes(Mon, &VideModeCount);
		for (auto j = 0; j < VideModeCount; ++j) {
			const auto VM = VideoModes[j];
			if (j > 3) {
				std::cout << "\t..." << std::endl;
				break;
			}
			std::cout << "\t[" << j << "] " << VM.width << "x" << VM.height << " @" << VM.refreshRate << std::endl;
		}
	}

	//!< OpenGL コンテキストを作成しない (Not create OpenGL context)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef USE_BORDERLESS
	//!< ボーダーレスにする場合 (Borderless)
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif
	//!< ウインドウ作成 (Create window)
	//constexpr auto Width = 1280, Height = 720;
	constexpr auto Width = 1920, Height = 1080;
#ifdef USE_FULLSCREEN
	//!< フルスクリーンにする場合 (Fullscreen)
	const auto Window = glfwCreateWindow(Width, Height, "Title", glfwGetPrimaryMonitor(), nullptr);
#else
	const auto Window = glfwCreateWindow(Width, Height, "Title", nullptr, nullptr);
	{
		int WinLeft, WinTop, WinRight, WinBottom;
		glfwGetWindowFrameSize(Window, &WinLeft, &WinTop, &WinRight, &WinBottom);
		std::cout << "Window frame size (LTRB) = " << WinLeft << ", " << WinTop << ", " << WinRight << " ," << WinBottom << std::endl;

		int WinX, WinY;
		glfwGetWindowPos(Window, &WinX, &WinY);		
		std::cout << "Window pos = " << WinX << ", " << WinY << std::endl;
		int WinWidth, WinHeight;
		glfwGetWindowSize(Window, &WinWidth, &WinHeight);
		std::cout << "Window size = " << WinWidth << "x" << WinHeight << std::endl;

		int FBWidth, GBHeight;
		glfwGetFramebufferSize(Window, &FBWidth, &GBHeight);
		std::cout << "Framebuffer size = " << FBWidth << "x" << GBHeight << std::endl;
	}
#endif

	//!< コールバック登録 (Register callbacks)
	glfwSetErrorCallback(GlfwErrorCallback);
	glfwSetKeyCallback(Window, GlfwKeyCallback);

	TriangleVK TriVK;

	//!< インスタンス (Instance)
	{
		//!< エクステンション (Extensions)
		std::vector<const char*> Extensions;
		uint32_t Count = 0;
		//!< glfwGetRequiredInstanceExtensions がプラットフォーム毎の違いを吸収してくれる
		const auto Exts = glfwGetRequiredInstanceExtensions(&Count);
		for (uint32_t i = 0; i < Count; ++i) {
			std::cout << "Add Extension : " << Exts[i] << std::endl;
			Extensions.emplace_back(Exts[i]);
		}
		TriVK.CreateInstance(Extensions);
	}

	//!< 物理デバイス (Physical device) (GPU)
	TriVK.SelectPhysicalDevice();

	//!< サーフェス (Surface)
	//!< glfwCreateWindowSurface がプラットフォーム毎の違いを吸収してくれる
	VERIFY_SUCCEEDED(glfwCreateWindowSurface(TriVK.Instance, Window, nullptr, &TriVK.Surface));
	TriVK.SelectSurfaceFormat();

	//!< デバイス、キュー (Device, Queue)
	TriVK.CreateDevice();

	//!< フェンスをシグナル状態で作成 (Fence, create as signaled) CPU - GPU
	TriVK.CreateFence();

	//!< セマフォ (Semaphore) GPU - GPU
	TriVK.CreateSemaphore();

	//!< スワップチェイン (Swapchain)
	{
		int FBWidth, FBHeight;
		glfwGetFramebufferSize(Window, &FBWidth, &FBHeight);
		TriVK.CreateSwapchain(static_cast<const uint32_t>(FBWidth), static_cast<const uint32_t>(FBHeight));
	}

	//!< コマンドバッファ (Command buffer)
	TriVK.CreateCommandBuffer();

	//!< ジオメトリ (Geometory)
	TriVK.CreateGeometry();

	//!< パイプラインレイアウト (Pipeline layout)
	TriVK.CreatePipelineLayout();

	//!< レンダーパス (Render pass)
	TriVK.CreateRenderPass();

	//!< パイプライン (Pipeline)
	TriVK.CreatePipeline();

	//!< フレームバッファ (Frame buffer)
	TriVK.CreateFramebuffer();

	//!< ビューポート、シザー (Viewport, scissor)
	TriVK.CreateViewports();

	//!< コマンド作成 (Populate command)
	TriVK.PopulateCommand();

	//!< ループ (Loop)
	while (!glfwWindowShouldClose(Window)) {
		glfwPollEvents();

		TriVK.WaitFence();
		TriVK.AcquireNextImage();
		TriVK.OnUpdate();
		TriVK.Submit();
		TriVK.Present();
	}

	//!< GLFW 後片付け (Terminate)
	glfwDestroyWindow(Window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
