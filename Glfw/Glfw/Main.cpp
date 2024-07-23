#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <bitset>
#include <map>

#include <Vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "glfw3dll.lib")

//#define USE_BORDERLESS
//#define USE_FULLSCREEN

#ifdef _DEBUG
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << VR << std::endl; /*__debugbreak();*/ } }
#else
#define VERIFY_SUCCEEDED(X) (X) 
#endif

//!< コールバック (Callbacks)
static void ErrorCallback(int Code, const char* Description)
{
	std::cerr << "Error code = " << Code << ", " << Description << std::endl;
}
static void KeyCallback(GLFWwindow* Window, int Key, [[maybe_unused]] int Scancode, int Action, [[maybe_unused]] int Mods)
{
	if (Key == GLFW_KEY_ESCAPE && Action == GLFW_PRESS) {
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}
}

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

	//!< OpenGL コンテキストを作成しない (Not create OpenGL context)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef USE_BORDERLESS
	//!< ボーダーレスにする場合 (Borderless)
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif

	//!< モニター列挙 (Enumerate monitors)
	int MonitorCount;
	const auto Monitors = glfwGetMonitors(&MonitorCount);
	for (auto i = 0; i < MonitorCount; ++i) {
		const auto Mon = Monitors[i];
		std::cout << "[" << i << "] " << glfwGetMonitorName(Mon) << std::endl;
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

	//!< ウインドウ作成 (Create window)
	//constexpr auto Width = 1280, Height = 720;
	constexpr auto Width = 1920, Height = 1080;
#ifdef USE_FULLSCREEN
	//!< フルスクリーンにする場合 (Fullscreen)
	const auto Window = glfwCreateWindow(Width, Height, "Title", glfwGetPrimaryMonitor(), nullptr);
#else
	const auto Window = glfwCreateWindow(Width, Height, "Title", nullptr, nullptr);
#endif

	//!< フレームバッファサイズ (ボーダーレスにするとウインドウサイズと同じになるはず) (Framebuffer size)
	{
		int FBWidth, GBHeight;
		glfwGetFramebufferSize(Window, &FBWidth, &GBHeight);
		std::cout << "Framebuffer = " << FBWidth << "x" << GBHeight << std::endl;
	}

	//!< コールバック登録 (Register callbacks)
	glfwSetErrorCallback(ErrorCallback);
	glfwSetKeyCallback(Window, KeyCallback);

	//!< インスタンス (Instance)
	VkInstance Instance = VK_NULL_HANDLE;
	{
		constexpr VkApplicationInfo AI = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = "Glfw App Name", .applicationVersion = VK_HEADER_VERSION_COMPLETE,
			.pEngineName = "Glfw Engine Name", .engineVersion = VK_HEADER_VERSION_COMPLETE,
			.apiVersion = VK_HEADER_VERSION_COMPLETE
		};
		const std::array Layers = {
			"VK_LAYER_KHRONOS_validation",
		};
		std::vector<const char*> Extensions = {
	#ifdef _DEBUG
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
	#endif
		};
		//!< エクステンション (glfw がプラットフォーム毎の違いを吸収してくれる) (Extensions)
		uint32_t ExtCount = 0;
		const auto Exts = glfwGetRequiredInstanceExtensions(&ExtCount);
		for (uint32_t i = 0; i < ExtCount; ++i) {
			std::cout << "Add Extension : " << Exts[i] << std::endl;
			Extensions.emplace_back(Exts[i]);
		}
		const VkInstanceCreateInfo ICI = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = &AI,
			.enabledLayerCount = static_cast<uint32_t>(std::size(Layers)), .ppEnabledLayerNames = std::data(Layers),
			.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions)
		};
		VERIFY_SUCCEEDED(vkCreateInstance(&ICI, nullptr, &Instance));
	}

	//!< 物理デバイス (GPU) (Physical device)
	std::vector<VkPhysicalDevice> PhysicalDevices;
	VkPhysicalDevice SelectedPhysicalDevice;
	VkPhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties;
	{
		uint32_t Count = 0;
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, nullptr));
		PhysicalDevices.resize(Count);
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, std::data(PhysicalDevices)));
		SelectedPhysicalDevice = PhysicalDevices[0];

		vkGetPhysicalDeviceMemoryProperties(SelectedPhysicalDevice, &PhysicalDeviceMemoryProperties);
	}

	//!< サーフェス (glfw がプラットフォーム毎の違いを吸収してくれる) (Surface)
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR SelectedSurfaceFormat;
	VkPresentModeKHR SelectedPresentMode;
	{
		{
			VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, Window, nullptr, &Surface));
			uint32_t Count = 0;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysicalDevice, Surface, &Count, nullptr));
			std::vector<VkSurfaceFormatKHR> SFs(Count);
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysicalDevice, Surface, &Count, std::data(SFs)));
			SelectedSurfaceFormat = SFs[0];
		}
		{
			uint32_t Count;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysicalDevice, Surface, &Count, nullptr));
			std::vector<VkPresentModeKHR> PMs(Count);
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysicalDevice, Surface, &Count, std::data(PMs)));
			SelectedPresentMode = PMs[0];
		}
	}

	//!< デバイス、キュー (Device, Queue)
	VkDevice Device = VK_NULL_HANDLE;
	//!< キューとファミリインデックス
	using QueueAndFamilyIndex = std::pair<VkQueue, uint32_t>;
	QueueAndFamilyIndex GraphicsQueue({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });
	QueueAndFamilyIndex PresentQueue({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });
	{
		//!< キュー情報取得
		using FamilyIndexAndPriorities = std::map<uint32_t, std::vector<float>>;
		FamilyIndexAndPriorities FIAP;
		uint32_t GraphicsIndexInFamily;
		uint32_t PresentIndexInFamily;
		{
			std::vector<VkQueueFamilyProperties> QFPs;
			uint32_t Count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(SelectedPhysicalDevice, &Count, nullptr);
			QFPs.resize(Count);
			vkGetPhysicalDeviceQueueFamilyProperties(SelectedPhysicalDevice, &Count, std::data(QFPs));

			//!< 機能を持つキューファミリインデックスを立てる
			std::bitset<32> GraphicsMask;
			std::bitset<32> PresentMask;
			for (auto i = 0; i < std::size(QFPs); ++i) {
				const auto& QFP = QFPs[i];
				if (VK_QUEUE_GRAPHICS_BIT & QFP.queueFlags) {
					GraphicsMask.set(i);
				}
				auto HasPresent = VK_FALSE;
				VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceSupportKHR(SelectedPhysicalDevice, i, Surface, &HasPresent));
				if (HasPresent) {
					PresentMask.set(i);
				}
			}

			//!< ここでは機能を持つ最初のファミリインデックスを採用する
			GraphicsQueue.second = [&]() {
				for (uint32_t i = 0; i < std::size(GraphicsMask); ++i) { if (GraphicsMask.test(i)) { return i; } } 
				return (std::numeric_limits<uint32_t>::max)(); 
			}();
			PresentQueue.second = [&]() {
				for (uint32_t i = 0; i < std::size(PresentMask); ++i) { if (PresentMask.test(i)) { return i; } }
				return (std::numeric_limits<uint32_t>::max)();
			}();

			//!< (キューファミリインデックス毎に) プライオリティを追加、ファミリ内でのインデックスは一旦覚えておく
			GraphicsIndexInFamily = static_cast<uint32_t>(std::size(FIAP[GraphicsQueue.second]));
			FIAP[GraphicsQueue.second].emplace_back(0.5f);
			PresentIndexInFamily = static_cast<uint32_t>(std::size(FIAP[PresentQueue.second]));
			FIAP[PresentQueue.second].emplace_back(0.5f);
		}

		//!< デバイス作成
		{
			std::vector<VkDeviceQueueCreateInfo> DQCIs;
			for (auto i : FIAP) {
				DQCIs.emplace_back(VkDeviceQueueCreateInfo({
									.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
									.pNext = nullptr,
									.flags = 0,
									.queueFamilyIndex = i.first,
									.queueCount = static_cast<uint32_t>(std::size(i.second)), .pQueuePriorities = std::data(i.second)
									}) );
			}
			const std::array Extensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};
			VkPhysicalDeviceFeatures PDF;
			vkGetPhysicalDeviceFeatures(SelectedPhysicalDevice, &PDF);
			const VkDeviceCreateInfo DCI = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueCreateInfoCount = static_cast<uint32_t>(std::size(DQCIs)), .pQueueCreateInfos = std::data(DQCIs),
				.enabledLayerCount = 0, .ppEnabledLayerNames = nullptr,
				.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions),
				.pEnabledFeatures = &PDF
			};
			VERIFY_SUCCEEDED(vkCreateDevice(SelectedPhysicalDevice, &DCI, nullptr, &Device));
		}

		//!< デバイス作成後に、キューファミリインデックスとファミリ内でのインデックスから、キューを取得
		{
			vkGetDeviceQueue(Device, GraphicsQueue.second, GraphicsIndexInFamily, &GraphicsQueue.first);
			vkGetDeviceQueue(Device, PresentQueue.second, PresentIndexInFamily, &PresentQueue.first);
		}
	}
	//!< フェンス (Fence)
	VkFence Fence = VK_NULL_HANDLE;
	{
		constexpr VkFenceCreateInfo FCI = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr, 
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		VERIFY_SUCCEEDED(vkCreateFence(Device, &FCI, nullptr, &Fence));
	}
	//!< セマフォ (Semaphore)
	VkSemaphore NextImageAcquiredSemaphore = VK_NULL_HANDLE, RenderFinishedSemaphore = VK_NULL_HANDLE;
	{
		constexpr VkSemaphoreCreateInfo SCI = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 
			.pNext = nullptr, .flags = 0
		};
		VERIFY_SUCCEEDED(vkCreateSemaphore(Device, &SCI, nullptr, &NextImageAcquiredSemaphore));
		VERIFY_SUCCEEDED(vkCreateSemaphore(Device, &SCI, nullptr, &RenderFinishedSemaphore));
	}
	//!< スワップチェイン (Swapchain)
	VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
	VkExtent2D SurfaceExtent2D;
	std::vector<VkImageView> ImageViews;
	uint32_t SwapchainImageIndex = 0;
	{
		VkSurfaceCapabilitiesKHR SC;
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(SelectedPhysicalDevice, Surface, &SC));

		int FBWidth, FBHeight;
		glfwGetFramebufferSize(Window, &FBWidth, &FBHeight);
		SurfaceExtent2D = 0xffffffff != SC.currentExtent.width ? SC.currentExtent : VkExtent2D({ .width = (std::clamp)(static_cast<uint32_t>(FBWidth), SC.maxImageExtent.width, SC.minImageExtent.width), .height = (std::clamp)(static_cast<uint32_t>(FBHeight), SC.minImageExtent.height, SC.minImageExtent.height) });

		const VkSwapchainCreateInfoKHR SCI = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.flags = 0,
			.surface = Surface,
			.minImageCount = 3,
			.imageFormat = SelectedSurfaceFormat.format, .imageColorSpace = SelectedSurfaceFormat.colorSpace,
			.imageExtent = SurfaceExtent2D,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			//.imageSharingMode = empty(QueueFamilyIndices) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			//.queueFamilyIndexCount = static_cast<uint32_t>(size(QueueFamilyIndices)), .pQueueFamilyIndices = data(QueueFamilyIndices),
			.queueFamilyIndexCount = 1, .pQueueFamilyIndices = 0,
			.preTransform = (VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR & SC.supportedTransforms) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : SC.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = SelectedPresentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE
		};
		VERIFY_SUCCEEDED(vkCreateSwapchainKHR(Device, &SCI, nullptr, &Swapchain));
		
		//!< イメージビュー (Image views)
		{
			uint32_t Count;
			VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain, &Count, nullptr));
			std::vector<VkImage> Images(Count);
			VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain, &Count, data(Images)));

			ImageViews.reserve(std::size(Images));
			for (auto i : Images) {
				const VkImageViewCreateInfo IVCI = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.image = i,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = SelectedSurfaceFormat.format,
					.components = VkComponentMapping({.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY, }),
					.subresourceRange = VkImageSubresourceRange({.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 })
				};
				VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, nullptr, &ImageViews.emplace_back()));
			}
		}
	}
	//!< コマンドバッファ (Command buffer)
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	std::array<VkCommandBuffer, 3> CommandBuffers;
	{
		const VkCommandPoolCreateInfo CPCI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = 0
		};
		VERIFY_SUCCEEDED(vkCreateCommandPool(Device, &CPCI, nullptr, &CommandPool));

		const VkCommandBufferAllocateInfo CBAI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = static_cast<uint32_t>(std::size(CommandBuffers))
		};
		VERIFY_SUCCEEDED(vkAllocateCommandBuffers(Device, &CBAI, std::data(CommandBuffers)));
	}
	//!< ジオメトリ (Geometry)
	{
		const auto& CB = CommandBuffers[0];
		const auto PDMP = PhysicalDeviceMemoryProperties;

		const std::array Vertices = {
			std::pair<glm::vec3, glm::vec3>{{ 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f}},
			std::pair<glm::vec3, glm::vec3>{{ -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f}},
			std::pair<glm::vec3, glm::vec3>{{ 0.5f, -0.5f, 0.0f }, {  0.0f, 0.0f, 1.0f}},
		};
		constexpr std::array Indices = { uint32_t(0), uint32_t(1), uint32_t(2) };
	}

	//!< パイプラインレイアウト (Pipeline layout)
	VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
	{
		constexpr std::array<VkDescriptorSetLayout, 0> DSLs = {};
		constexpr std::array<VkPushConstantRange, 0> PCRs = {};
		const VkPipelineLayoutCreateInfo PLCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = static_cast<uint32_t>(std::size(DSLs)), .pSetLayouts = std::data(DSLs),
			.pushConstantRangeCount = static_cast<uint32_t>(std::size(PCRs)), .pPushConstantRanges = std::data(PCRs)
		};
		VERIFY_SUCCEEDED(vkCreatePipelineLayout(Device, &PLCI, nullptr, &PipelineLayout));
	}

	//!< レンダーパス (Render pass)
	VkRenderPass RenderPass = VK_NULL_HANDLE;
	{
		const std::array ADs = {
			VkAttachmentDescription({
				.flags = 0,
				.format = SelectedSurfaceFormat.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			}), 
		};
		constexpr std::array<VkAttachmentReference, 0> IAs = {};
		constexpr std::array CAs = { VkAttachmentReference({.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }), };
		constexpr std::array RAs = { VkAttachmentReference({.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_UNDEFINED }), };
		constexpr std::array<uint32_t, 0> PAs = {};
		const std::array SDs = {
			VkSubpassDescription({
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = static_cast<uint32_t>(std::size(IAs)), .pInputAttachments = std::data(IAs),
				.colorAttachmentCount = static_cast<uint32_t>(std::size(CAs)), .pColorAttachments = std::data(CAs), .pResolveAttachments = std::data(RAs),
				.pDepthStencilAttachment = nullptr,
				.preserveAttachmentCount = static_cast<uint32_t>(std::size(PAs)), .pPreserveAttachments = std::data(PAs)
			}), 
		};
		const std::array<VkSubpassDependency, 0> Deps;
		const VkRenderPassCreateInfo RPCI = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = static_cast<uint32_t>(std::size(ADs)), .pAttachments = std::data(ADs),
			.subpassCount = static_cast<uint32_t>(std::size(SDs)), .pSubpasses = std::data(SDs),
			.dependencyCount = static_cast<uint32_t>(std::size(Deps)), .pDependencies = std::data(Deps)
		};
		VERIFY_SUCCEEDED(vkCreateRenderPass(Device, &RPCI, nullptr, &RenderPass));
	}

	auto CreateShaderModule = [&](const std::filesystem::path& Path) {
		VkShaderModule SM = VK_NULL_HANDLE;
		std::ifstream In(data(Path.string()), std::ios::in | std::ios::binary);
		if (!In.fail()) {
			In.seekg(0, std::ios_base::end);
			const auto Size = In.tellg();
			if (Size) {
				In.seekg(0, std::ios_base::beg);
				std::vector<std::byte> Code(Size);
				In.read(reinterpret_cast<char*>(std::data(Code)), std::size(Code));
				const VkShaderModuleCreateInfo SMCI = {
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.codeSize = static_cast<size_t>(std::size(Code)), .pCode = reinterpret_cast<uint32_t*>(std::data(Code))
				};
				VERIFY_SUCCEEDED(vkCreateShaderModule(Device, &SMCI, nullptr, &SM));
			}
			In.close();
		}
		return SM;
	};
	//!< パイプライン (Pipeline)
	VkPipeline Pipeline = VK_NULL_HANDLE;
	{
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
				.layout = PipelineLayout,
				.renderPass = RenderPass, .subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE, .basePipelineIndex = -1
			})
		};
		VERIFY_SUCCEEDED(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, static_cast<uint32_t>(std::size(GPCIs)), std::data(GPCIs), nullptr, &Pipeline));

		for (auto i : SMs) {
			vkDestroyShaderModule(Device, i, nullptr);
		}
	}
	//!< フレームバッファ (Frame buffer)
	std::vector<VkFramebuffer> Framebuffers;
	{
		Framebuffers.reserve(std::size(ImageViews));
		for (const auto& i : ImageViews) {
			const std::array IVs = { i };
			const VkFramebufferCreateInfo FCI = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.renderPass = RenderPass, 
				.attachmentCount = static_cast<uint32_t>(std::size(IVs)), .pAttachments = std::data(IVs),
				.width = SurfaceExtent2D.width, .height = SurfaceExtent2D.height,
				.layers = 1
			};
			VERIFY_SUCCEEDED(vkCreateFramebuffer(Device, &FCI, nullptr, &Framebuffers.emplace_back()));
		}
	}

	//!< ビューポート
	const std::array Viewports = {
		VkViewport({
			.x = 0.0f, .y = static_cast<float>(SurfaceExtent2D.height),
			.width = static_cast<float>(SurfaceExtent2D.width), .height = -static_cast<float>(SurfaceExtent2D.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		})
	};
	const std::array ScissorRects = {
		VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = VkExtent2D({.width = SurfaceExtent2D.width, .height = SurfaceExtent2D.height }) }),
	};

	//!< コマンド作成
	{
		for (auto i = 0; i < 1; ++i) {
			const auto FB = Framebuffers[i];
			const auto CB = CommandBuffers[i];
			constexpr VkCommandBufferBeginInfo CBBI = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr
			};
			VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
				constexpr std::array CVs = { VkClearValue({.color = { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } }) };
				const VkRenderPassBeginInfo RPBI = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.pNext = nullptr,
					.renderPass = RenderPass,
					.framebuffer = FB,
					.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = SurfaceExtent2D }),
					.clearValueCount = static_cast<uint32_t>(size(CVs)), .pClearValues = data(CVs)
				};
				vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); {
					vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
					vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

					vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

					//const std::array VBs = { VertexBuffer.Buffer };
					//const std::array Offsets = { VkDeviceSize(0) };
					//vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), data(Offsets));
					//vkCmdBindIndexBuffer(CB, IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
					//vkCmdDrawIndexedIndirect(CB, IndirectBuffer.Buffer, 0, 1, 0);
				} vkCmdEndRenderPass(CB);
			} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		}
	}

	//!< ループ (Loop)
	while (!glfwWindowShouldClose(Window)) {
		glfwPollEvents();

		//!< フェンス待ち (Wait for fence)
		{
			const std::array Fences = { Fence };
			VERIFY_SUCCEEDED(vkWaitForFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences), VK_TRUE, (std::numeric_limits<uint64_t>::max)()));
			vkResetFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences));
		}

		VERIFY_SUCCEEDED(vkAcquireNextImageKHR(Device, Swapchain, (std::numeric_limits<uint64_t>::max)(), NextImageAcquiredSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex));

		//!< サブミット (Submit)
		{
			const std::array WaitSems = { NextImageAcquiredSemaphore };
			const std::array WaitStages = { VkPipelineStageFlags(VK_PIPELINE_STAGE_TRANSFER_BIT) };
			const std::array CBs = { CommandBuffers[SwapchainImageIndex], };
			const std::array SigSems = { RenderFinishedSemaphore };
			const std::array SIs = {
				VkSubmitInfo({
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = static_cast<uint32_t>(std::size(WaitSems)), .pWaitSemaphores = std::data(WaitSems), .pWaitDstStageMask = std::data(WaitStages), 
					.commandBufferCount = static_cast<uint32_t>(std::size(CBs)), .pCommandBuffers = std::data(CBs),
					.signalSemaphoreCount = static_cast<uint32_t>(std::size(SigSems)), .pSignalSemaphores = std::data(SigSems)
				}),
			};
			VERIFY_SUCCEEDED(vkQueueSubmit(GraphicsQueue.first, static_cast<uint32_t>(std::size(SIs)), std::data(SIs), Fence));
		}

		//!< プレゼンテーション (Present)
		{
			const std::array WaitSems = { RenderFinishedSemaphore };
			const std::array Swapchains = { Swapchain };
			const std::array ImageIndices = { SwapchainImageIndex };
			const VkPresentInfoKHR PI = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = nullptr,
				.waitSemaphoreCount = static_cast<uint32_t>(std::size(WaitSems)), .pWaitSemaphores = std::data(WaitSems),
				.swapchainCount = static_cast<uint32_t>(std::size(Swapchains)), .pSwapchains = std::data(Swapchains), .pImageIndices = std::data(ImageIndices),
				.pResults = nullptr
			};
			VERIFY_SUCCEEDED(vkQueuePresentKHR(PresentQueue.first, &PI));
		}
	}
	
	//!< VK 後片付け (Terminate)
	if (VK_NULL_HANDLE != Instance) {
		for (auto i : Framebuffers) {
			vkDestroyFramebuffer(Device, i, nullptr);
		}
		if (VK_NULL_HANDLE != Pipeline) {
			vkDestroyPipeline(Device, Pipeline, nullptr);
		}
		if (VK_NULL_HANDLE != RenderPass) {
			vkDestroyRenderPass(Device, RenderPass, nullptr);
		}
		if (VK_NULL_HANDLE != PipelineLayout) {
			vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
		}
		if (VK_NULL_HANDLE != CommandPool) {
			vkDestroyCommandPool(Device, CommandPool, nullptr);
		}
		for (auto i : ImageViews) {
			vkDestroyImageView(Device, i, nullptr);
		}
		if (VK_NULL_HANDLE != Swapchain) {
			vkDestroySwapchainKHR(Device, Swapchain, nullptr);
		}
		if (VK_NULL_HANDLE != RenderFinishedSemaphore) {
			vkDestroySemaphore(Device, RenderFinishedSemaphore, nullptr);
		}
		if (VK_NULL_HANDLE != NextImageAcquiredSemaphore) {
			vkDestroySemaphore(Device, NextImageAcquiredSemaphore, nullptr);
		}
		if (VK_NULL_HANDLE != Fence) {
			vkDestroyFence(Device, Fence, nullptr);
		}
		if (VK_NULL_HANDLE != Device) {
			vkDestroyDevice(Device, nullptr);
		}
		if (VK_NULL_HANDLE != Surface) {
			vkDestroySurfaceKHR(Instance, Surface, nullptr);
		}
		vkDestroyInstance(Instance, nullptr);
	}

	//!< GLFW 後片付け (Terminate)
	glfwDestroyWindow(Window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
