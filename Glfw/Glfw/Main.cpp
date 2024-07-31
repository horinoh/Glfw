#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <bitset>
#include <map>
#include <numeric>

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
		//!< エクステンション (Extensions)
		uint32_t ExtCount = 0;
		//!< glfwGetRequiredInstanceExtensions がプラットフォーム毎の違いを吸収してくれる
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
	//!< デバッグユーティリティ (Debug utils)
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT DebugUtilsMessenger = VK_NULL_HANDLE;
	auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
	auto vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
	{
		constexpr VkDebugUtilsMessengerCreateInfoEXT DUMCI = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
			.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT DUMSFB, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT DUMTF, [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* DUMCD, [[maybe_unused]] void* UserData) {
				if (DUMSFB >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
					std::cerr << DUMCD->pMessage << std::endl;
					return VK_TRUE;
				}
				return VK_FALSE;
			},
			.pUserData = nullptr
		};
		VERIFY_SUCCEEDED(vkCreateDebugUtilsMessenger(Instance, &DUMCI, nullptr, &DebugUtilsMessenger));
	}
#endif

	//!< 物理デバイス (Physical device) (GPU)
	std::vector<VkPhysicalDevice> PhysicalDevices;
	VkPhysicalDevice SelectedPhysicalDevice;
	VkPhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties;
	{
		uint32_t Count = 0;
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, nullptr));
		PhysicalDevices.resize(Count);
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, std::data(PhysicalDevices)));

		//!< ここでは最大メモリの物理デバイスを採用する (Select max memory GPU here)
		SelectedPhysicalDevice = *std::ranges::max_element(PhysicalDevices, [](const auto& lhs, const auto& rhs) {
			VkPhysicalDeviceMemoryProperties MemPropL, MemPropR;
			vkGetPhysicalDeviceMemoryProperties(lhs, &MemPropL);
			vkGetPhysicalDeviceMemoryProperties(rhs, &MemPropR);
			const auto MemTotalL = std::accumulate(std::data(MemPropL.memoryHeaps), &MemPropL.memoryHeaps[MemPropL.memoryHeapCount], VkDeviceSize(0), [](auto Sum, const auto& rhs) { return Sum + rhs.size; });
			const auto MemTotalR = std::accumulate(std::data(MemPropR.memoryHeaps), &MemPropR.memoryHeaps[MemPropR.memoryHeapCount], VkDeviceSize(0), [](auto Sum, const auto& rhs) { return Sum + rhs.size; });
			return MemTotalL < MemTotalR;
			});

		vkGetPhysicalDeviceMemoryProperties(SelectedPhysicalDevice, &PhysicalDeviceMemoryProperties);

#ifdef _DEBUG
		VkPhysicalDeviceProperties PDP;
		vkGetPhysicalDeviceProperties(SelectedPhysicalDevice, &PDP);
		std::cout << PDP.deviceName << "is selected" << std::endl;
#endif
	}

	//!< サーフェス (Surface)
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR SelectedSurfaceFormat;
	{
		//!< glfwCreateWindowSurface がプラットフォーム毎の違いを吸収してくれる
		VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, Window, nullptr, &Surface));
		uint32_t Count = 0;
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysicalDevice, Surface, &Count, nullptr));
		std::vector<VkSurfaceFormatKHR> SFs(Count);
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysicalDevice, Surface, &Count, std::data(SFs)));

		//!< ここでは最初に見つかった FORMAT_UNDEFINED でないものを採用する (Select first no FORMAT_UNDEFINED here)
		SelectedSurfaceFormat = *std::ranges::find_if(SFs, [](const auto& rhs) {
			if (VK_FORMAT_UNDEFINED != rhs.format) {
				return true;
			}
			return false;
			});
	}

	//!< デバイス、キュー (Device, Queue)
	VkDevice Device = VK_NULL_HANDLE;
	//!< キューとファミリインデックス (Queue and family index)
	using QueueAndFamilyIndex = std::pair<VkQueue, uint32_t>;
	QueueAndFamilyIndex GraphicsQueue({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });
	QueueAndFamilyIndex PresentQueue({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });
	{
		//!< デバイス作成前に、キュー情報取得 (Before create device, get queue information)
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

			//!< 機能を持つキューファミリインデックスを立てる (If queue family index has function, set bit)
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

			//!< ここでは機能を持つ最初のファミリインデックスを採用する (Select first family index, which has function)
			GraphicsQueue.second = [&]() {
				for (uint32_t i = 0; i < std::size(GraphicsMask); ++i) {
					if (GraphicsMask.test(i)) {
						return i;
					}
				}
				return (std::numeric_limits<uint32_t>::max)();
				}();
			PresentQueue.second = [&]() {
				for (uint32_t i = 0; i < std::size(PresentMask); ++i) {
					if (PresentMask.test(i)) {
						return i;
					}
				}
				return (std::numeric_limits<uint32_t>::max)();
				}();

			//!< (キューファミリインデックス毎に) プライオリティを追加、ファミリ内でのインデックスは一旦覚えておく (For each queue family index, add priority)
			GraphicsIndexInFamily = static_cast<uint32_t>(std::size(FIAP[GraphicsQueue.second]));
			FIAP[GraphicsQueue.second].emplace_back(0.5f);
			PresentIndexInFamily = static_cast<uint32_t>(std::size(FIAP[PresentQueue.second]));
			FIAP[PresentQueue.second].emplace_back(0.5f);
		}

		//!< デバイス作成 (Create device)
		{
			std::vector<VkDeviceQueueCreateInfo> DQCIs;
			for (const auto& i : FIAP) {
				DQCIs.emplace_back(VkDeviceQueueCreateInfo({
									.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
									.pNext = nullptr,
									.flags = 0,
									.queueFamilyIndex = i.first,
									.queueCount = static_cast<uint32_t>(std::size(i.second)), .pQueuePriorities = std::data(i.second)
					}));
			}
			const std::array Extensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};
#pragma region VULKAN_FEATURE
			VkPhysicalDeviceVulkan11Features PDV11F = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				.pNext = nullptr,
				.storageBuffer16BitAccess = VK_FALSE,
				.uniformAndStorageBuffer16BitAccess = VK_FALSE,
				.storagePushConstant16 = VK_FALSE,
				.storageInputOutput16 = VK_FALSE,
				.multiview = VK_FALSE,
				.multiviewGeometryShader = VK_FALSE,
				.multiviewTessellationShader = VK_FALSE,
				.variablePointersStorageBuffer = VK_FALSE,
				.variablePointers = VK_FALSE,
				.protectedMemory = VK_FALSE,
				.samplerYcbcrConversion = VK_FALSE,
				.shaderDrawParameters = VK_FALSE,
			};
			VkPhysicalDeviceVulkan12Features PDV12F = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.pNext = &PDV11F,
				.samplerMirrorClampToEdge = VK_FALSE,
				.drawIndirectCount = VK_FALSE,
				.storageBuffer8BitAccess = VK_FALSE,
				.uniformAndStorageBuffer8BitAccess = VK_FALSE,
				.storagePushConstant8 = VK_FALSE,
				.shaderBufferInt64Atomics = VK_FALSE,
				.shaderSharedInt64Atomics = VK_FALSE,
				.shaderFloat16 = VK_FALSE,
				.shaderInt8 = VK_FALSE,
				.descriptorIndexing = VK_FALSE,
				.shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
				.shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
				.shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
				.shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
				.shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
				.shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
				.shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
				.shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
				.shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
				.shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
				.descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
				.descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
				.descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
				.descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
				.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
				.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
				.descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
				.descriptorBindingPartiallyBound = VK_FALSE,
				.descriptorBindingVariableDescriptorCount = VK_FALSE,
				.runtimeDescriptorArray = VK_FALSE,
				.samplerFilterMinmax = VK_FALSE,
				.scalarBlockLayout = VK_FALSE,
				.imagelessFramebuffer = VK_FALSE,
				.uniformBufferStandardLayout = VK_FALSE,
				.shaderSubgroupExtendedTypes = VK_FALSE,
				.separateDepthStencilLayouts = VK_FALSE,
				.hostQueryReset = VK_FALSE,
				.timelineSemaphore = VK_TRUE,
				.bufferDeviceAddress = VK_FALSE,
				.bufferDeviceAddressCaptureReplay = VK_FALSE,
				.bufferDeviceAddressMultiDevice = VK_FALSE,
				.vulkanMemoryModel = VK_FALSE,
				.vulkanMemoryModelDeviceScope = VK_FALSE,
				.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE,
				.shaderOutputViewportIndex = VK_FALSE,
				.shaderOutputLayer = VK_FALSE,
				.subgroupBroadcastDynamicId = VK_FALSE,
			};
			VkPhysicalDeviceVulkan13Features PDV13F = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &PDV12F,
				.robustImageAccess = VK_FALSE,
				.inlineUniformBlock = VK_FALSE,
				.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
				.pipelineCreationCacheControl = VK_FALSE,
				.privateData = VK_FALSE,
				.shaderDemoteToHelperInvocation = VK_FALSE,
				.shaderTerminateInvocation = VK_FALSE,
				.subgroupSizeControl = VK_FALSE,
				.computeFullSubgroups = VK_FALSE,
				.synchronization2 = VK_TRUE,
				.textureCompressionASTC_HDR = VK_FALSE,
				.shaderZeroInitializeWorkgroupMemory = VK_FALSE,
				.dynamicRendering = VK_FALSE,
				.shaderIntegerDotProduct = VK_FALSE,
				.maintenance4 = VK_FALSE,
			};
#pragma endregion
			VkPhysicalDeviceFeatures PDF;
			vkGetPhysicalDeviceFeatures(SelectedPhysicalDevice, &PDF);
			const VkDeviceCreateInfo DCI = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &PDV13F,
				.flags = 0,
				.queueCreateInfoCount = static_cast<uint32_t>(std::size(DQCIs)), .pQueueCreateInfos = std::data(DQCIs),
				.enabledLayerCount = 0, .ppEnabledLayerNames = nullptr,
				.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions),
				.pEnabledFeatures = &PDF
			};
			VERIFY_SUCCEEDED(vkCreateDevice(SelectedPhysicalDevice, &DCI, nullptr, &Device));
		}

		//!< デバイス作成後に、キューファミリインデックスとファミリ内でのインデックスから、キューを取得 (After create device, get queue from family index, index in family)
		vkGetDeviceQueue(Device, GraphicsQueue.second, GraphicsIndexInFamily, &GraphicsQueue.first);
		vkGetDeviceQueue(Device, PresentQueue.second, PresentIndexInFamily, &PresentQueue.first);
	}
	//!< フェンスをシグナル状態で作成 (Fence, create as signaled) CPU - GPU
	VkFence Fence = VK_NULL_HANDLE;
	{
		constexpr VkFenceCreateInfo FCI = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		VERIFY_SUCCEEDED(vkCreateFence(Device, &FCI, nullptr, &Fence));
	}
	//!< セマフォ (Semaphore) GPU - GPU
	VkSemaphore NextImageAcquiredSemaphore = VK_NULL_HANDLE, RenderFinishedSemaphore = VK_NULL_HANDLE;
	{
		constexpr VkSemaphoreTypeCreateInfo STCI = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.pNext = nullptr,
			.semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
			.initialValue = 0
		};
		const VkSemaphoreCreateInfo SCI = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &STCI,
			.flags = 0
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

		std::vector<uint32_t> QueueFamilyIndices;
		if (GraphicsQueue.second != PresentQueue.second) {
			QueueFamilyIndices.emplace_back(GraphicsQueue.second);
			QueueFamilyIndices.emplace_back(PresentQueue.second);
		}

		VkPresentModeKHR SelectedPresentMode;
		{
			uint32_t Count;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysicalDevice, Surface, &Count, nullptr));
			std::vector<VkPresentModeKHR> PMs(Count);
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysicalDevice, Surface, &Count, std::data(PMs)));

			//!< MAILBOX がサポートされるなら採用する (Select if MAILBOX supported)
			auto It = std::ranges::find(PMs, VK_PRESENT_MODE_MAILBOX_KHR);
			if (It == std::end(PMs)) {
				//!< 見つからない場合は FIFO (FIFO は必ずサポートされる)
				It = std::ranges::find(PMs, VK_PRESENT_MODE_FIFO_KHR);
			}
			SelectedPresentMode = *It;
		}

		const VkSwapchainCreateInfoKHR SCI = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.flags = 0,
			.surface = Surface,
			//!< イメージ数は最小 + 1 を希望 (Want minImageCount + 1)
			.minImageCount = (std::min)(SC.minImageCount + 1, 0 == SC.maxImageCount ? (std::numeric_limits<uint32_t>::max)() : SC.maxImageCount),
			.imageFormat = SelectedSurfaceFormat.format, .imageColorSpace = SelectedSurfaceFormat.colorSpace,
			.imageExtent = SurfaceExtent2D,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = std::empty(QueueFamilyIndices) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
			.queueFamilyIndexCount = static_cast<uint32_t>(std::size(QueueFamilyIndices)), .pQueueFamilyIndices = std::data(QueueFamilyIndices),
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
			VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain, &Count, std::data(Images)));
			
			ImageViews.reserve(std::size(Images));
			for (const auto& i : Images) {
				const VkImageViewCreateInfo IVCI = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.image = i,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = SelectedSurfaceFormat.format,
					.components = VkComponentMapping({.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY, }),
					.subresourceRange = VkImageSubresourceRange({
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
						.baseMipLevel = 0, .levelCount = 1, 
						.baseArrayLayer = 0, .layerCount = 1
					})
				};
				VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, nullptr, &ImageViews.emplace_back()));
			}
		}
	}
	//!< コマンドバッファ (Command buffer)
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> CommandBuffers(std::size(ImageViews));
	{
		const VkCommandPoolCreateInfo CPCI = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = GraphicsQueue.second
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

	auto GetMemoryTypeIndex = [&](const uint32_t TypeBits, const VkMemoryPropertyFlags MPF) {
		for (uint32_t i = 0; i < PhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
			if (TypeBits & (1 << i)) {
				if ((PhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & MPF) == MPF) {
					return i;
				}
			}
		}
		return (std::numeric_limits<uint32_t>::max)();
		};
	//!< 汎用バッファ作成関数
	auto CreateBuffer = [&](VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlagBits MPFB, const size_t Size, const void* Source = nullptr) {
		constexpr std::array<uint32_t, 0> QFI = {};
		const VkBufferCreateInfo BCI = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = Size,
			.usage = BUF,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = static_cast<uint32_t>(std::size(QFI)), .pQueueFamilyIndices = std::data(QFI)
		};
		VERIFY_SUCCEEDED(vkCreateBuffer(Device, &BCI, nullptr, Buffer));

		VkMemoryRequirements MR;
		vkGetBufferMemoryRequirements(Device, *Buffer, &MR);
		const VkMemoryAllocateInfo MAI = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = MR.size,
			.memoryTypeIndex = GetMemoryTypeIndex(MR.memoryTypeBits, MPFB)
		};
		VERIFY_SUCCEEDED(vkAllocateMemory(Device, &MAI, nullptr, DeviceMemory));
		VERIFY_SUCCEEDED(vkBindBufferMemory(Device, *Buffer, *DeviceMemory, 0));

		if (nullptr != Source) {
			constexpr auto MapSize = VK_WHOLE_SIZE;
			void* Data;
			VERIFY_SUCCEEDED(vkMapMemory(Device, *DeviceMemory, 0, MapSize, static_cast<VkMemoryMapFlags>(0), &Data)); {
				memcpy(Data, Source, Size);
				//!< メモリコンテンツが変更されたことを明示的にドライバへ知らせる(vkMapMemory()した状態でやること)
				if (!(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & BUF)) {
					const std::array MMRs = {
						VkMappedMemoryRange({
							.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
							.pNext = nullptr,
							.memory = *DeviceMemory,
							.offset = 0, .size = MapSize
						}),
					};
					VERIFY_SUCCEEDED(vkFlushMappedMemoryRanges(Device, static_cast<uint32_t>(std::size(MMRs)), std::data(MMRs)));
				}
			} vkUnmapMemory(Device, *DeviceMemory);
		}
		};
	//!< デバイスローカルバッファ作成関数
	auto CreateDeviceLocalBuffer = [&](VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size) {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Size);
		};
	//!< ホストビジブルバッファ作成関数
	auto CreateHostVisibleBuffer = [&](VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size, const void* Source) {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Size, Source);
		};
	//!< バッファメモリバリア関数
	auto BufferMemoryBarrier = [&](const VkCommandBuffer CB,
		const VkBuffer Buffer,
		const VkPipelineStageFlags SrcPSF, const VkPipelineStageFlags DstPSF,
		const VkAccessFlags SrcAF, const VkAccessFlags DstAF) {
			constexpr std::array<VkMemoryBarrier2, 0> MBs = {};
			const std::array BMBs = {
				VkBufferMemoryBarrier2({
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = SrcPSF, .srcAccessMask = SrcAF, .dstStageMask = DstPSF, .dstAccessMask = DstAF,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = Buffer, .offset = 0, .size = VK_WHOLE_SIZE
				}),
			};
			constexpr std::array<VkImageMemoryBarrier2, 0> IMBs = {};
			const VkDependencyInfo DI = {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.pNext = nullptr,
				.dependencyFlags = 0,
				.memoryBarrierCount = static_cast<uint32_t>(std::size(MBs)), .pMemoryBarriers = std::data(MBs),
				.bufferMemoryBarrierCount = static_cast<uint32_t>(std::size(BMBs)), .pBufferMemoryBarriers = std::data(BMBs),
				.imageMemoryBarrierCount = static_cast<uint32_t>(std::size(IMBs)), .pImageMemoryBarriers = std::data(IMBs),
			};
			vkCmdPipelineBarrier2(CB, &DI);
		};
	//!< コピーコマンド作成関数
	auto PopulateCopyCommand = [&](const VkCommandBuffer CB, const VkBuffer Staging, const VkBuffer Buffer, const size_t Size, const VkAccessFlags AF, const VkPipelineStageFlagBits PSF) {
		BufferMemoryBarrier(CB, Buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, VK_ACCESS_MEMORY_WRITE_BIT);
		{
			const std::array BCs = { VkBufferCopy({.srcOffset = 0, .dstOffset = 0, .size = Size }), };
			vkCmdCopyBuffer(CB, Staging, Buffer, static_cast<uint32_t>(std::size(BCs)), std::data(BCs));
		}
		BufferMemoryBarrier(CB, Buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, PSF,
			VK_ACCESS_MEMORY_WRITE_BIT, AF);
		};
	//!< ジオメトリ (Geometry)
	using BufferAndDeviceMemory = std::pair<VkBuffer, VkDeviceMemory>;
	auto VertexBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
	auto IndexBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
	auto IndirectBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
	{
		//!< バーテックスバッファ、ステージングの作成 (Create vertex buffer, staging)
		using PositonColor = std::pair<glm::vec3, glm::vec3>;
		const std::array Vertices = {
			PositonColor({ 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
			PositonColor({ -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
			PositonColor({ 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
		};
		CreateDeviceLocalBuffer(&VertexBuffer.first, &VertexBuffer.second, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Vertices));
		auto VertexStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		CreateHostVisibleBuffer(&VertexStagingBuffer.first, &VertexStagingBuffer.second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(Vertices), std::data(Vertices));
		
		//!< インデックスバッファ、ステージングの作成 (Create index buffer, staging)
		constexpr std::array Indices = {
			uint32_t(0), uint32_t(1), uint32_t(2) 
		};
		CreateDeviceLocalBuffer(&IndexBuffer.first, &IndexBuffer.second, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Indices));
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
		CreateDeviceLocalBuffer(&IndirectBuffer.first, &IndirectBuffer.second, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(DIIC));
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
			PopulateCopyCommand(CB, VertexStagingBuffer.first, VertexBuffer.first, sizeof(Vertices), VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			PopulateCopyCommand(CB, IndexStagingBuffer.first, IndexBuffer.first, sizeof(Indices), VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			PopulateCopyCommand(CB, IndirectStagingBuffer.first, IndirectBuffer.first, sizeof(DIIC), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
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

	//!< シェーダモジュール作成関数
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

		for (const auto i : SMs) {
			vkDestroyShaderModule(Device, i, nullptr);
		}
	}
	//!< フレームバッファ (Frame buffer)
	std::vector<VkFramebuffer> Framebuffers;
	Framebuffers.reserve(std::size(ImageViews));
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

	//!< ビューポート、シザー (Viewport, scissor)
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

	//!< コマンド作成 (Populate command)
	{
		for (auto i = 0; i < std::size(ImageViews); ++i) {
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

				vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

				constexpr std::array CVs = { VkClearValue({.color = { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } }) };
				const VkRenderPassBeginInfo RPBI = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.pNext = nullptr,
					.renderPass = RenderPass,
					.framebuffer = FB,
					.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = SurfaceExtent2D }),
					.clearValueCount = static_cast<uint32_t>(size(CVs)), .pClearValues = data(CVs)
				};
				vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
					const std::array VBs = { VertexBuffer.first };
					const std::array Offsets = { VkDeviceSize(0) };
					vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), std::data(Offsets));
					vkCmdBindIndexBuffer(CB, IndexBuffer.first, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexedIndirect(CB, IndirectBuffer.first, 0, 1, 0);
				} vkCmdEndRenderPass(CB);
			} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		}
	}

	//!< ループ (Loop)
	while (!glfwWindowShouldClose(Window)) {
		glfwPollEvents();

		//!< フェンス待ち、リセット (Wait for fence, reset) CPU - GPU
		//!< (シグナル状態で作成しているので、初回フレームは待ちにならない)
		{
			const std::array Fences = { Fence };
			VERIFY_SUCCEEDED(vkWaitForFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences), VK_TRUE, (std::numeric_limits<uint64_t>::max)()));
			vkResetFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences));
		}

		//!< 次のイメージインデックスを取得、イメージが取得出来たらセマフォ(A) がシグナルされる (Acquire next image index, on acquire semaphore A will be signaled)
		VERIFY_SUCCEEDED(vkAcquireNextImageKHR(Device, Swapchain, (std::numeric_limits<uint64_t>::max)(), NextImageAcquiredSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex));
		//!< サブミット (Submit)
		{
			const auto& CB = CommandBuffers[SwapchainImageIndex];
			//!< セマフォ (A) がシグナル (次のイメージ取得) される迄待つ
			//!< レンダリングが終わったらセマフォ (B) がシグナル (レンダリング完了) される
			const std::array WaitSSIs = { 
				VkSemaphoreSubmitInfo({
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext = nullptr,
					.semaphore = NextImageAcquiredSemaphore,
					.value = 0,
					.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					.deviceIndex = 0
					})
			};
			const std::array CBSIs = {
				VkCommandBufferSubmitInfo({
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
					.pNext = nullptr,
					.commandBuffer = CB,
					.deviceMask = 0
					})
			};
			const std::array SignalSSIs = {
				VkSemaphoreSubmitInfo({
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext = nullptr,
					.semaphore = RenderFinishedSemaphore,
					.value = 0,
					.stageMask = VK_PIPELINE_STAGE_2_NONE,
					.deviceIndex = 0
					})
			};
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
			VERIFY_SUCCEEDED(vkQueueSubmit2(GraphicsQueue.first, static_cast<uint32_t>(std::size(SIs)), std::data(SIs), Fence));
		}

		//!< プレゼンテーション (Present)
		{
			//!< セマフォ (B) がシグナル (レンダリング完了) される迄待つ
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
	
	//!< 処理の完了を待つ (Wait for idle)
	if (VK_NULL_HANDLE != Device) {
		VERIFY_SUCCEEDED(vkDeviceWaitIdle(Device));
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
		if (VK_NULL_HANDLE != IndirectBuffer.second) {
			vkFreeMemory(Device, IndirectBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != IndirectBuffer.first) {
			vkDestroyBuffer(Device, IndirectBuffer.first, nullptr);
		}
		if (VK_NULL_HANDLE != IndexBuffer.second) {
			vkFreeMemory(Device, IndexBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != IndexBuffer.first) {
			vkDestroyBuffer(Device, IndexBuffer.first, nullptr);
		}
		if (VK_NULL_HANDLE != VertexBuffer.second) {
			vkFreeMemory(Device, VertexBuffer.second, nullptr);
		}
		if (VK_NULL_HANDLE != VertexBuffer.first) {
			vkDestroyBuffer(Device, VertexBuffer.first, nullptr);
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
#ifdef _DEBUG
		if (VK_NULL_HANDLE != DebugUtilsMessenger) {
			vkDestroyDebugUtilsMessenger(Instance, DebugUtilsMessenger, nullptr);
		}
#endif
		if (VK_NULL_HANDLE != Instance) {
			vkDestroyInstance(Instance, nullptr);
		}
	}

	//!< GLFW 後片付け (Terminate)
	glfwDestroyWindow(Window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
