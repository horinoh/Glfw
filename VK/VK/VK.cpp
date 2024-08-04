#include "pch.h"

#include <array>
#include <fstream>
#include <map>
#include <bitset>

#include "VK.h"

VK::~VK() 
{
	//!< 処理の完了を待つ (Wait for idle)
	if (VK_NULL_HANDLE != Device) {
		VERIFY_SUCCEEDED(vkDeviceWaitIdle(Device));
	}

	for (auto i : Framebuffers) {
		vkDestroyFramebuffer(Device, i, nullptr);
	}
	for (auto i : Pipelines) {
		vkDestroyPipeline(Device, i, nullptr);
	}
	for (auto i : RenderPasses) {
		vkDestroyRenderPass(Device, i, nullptr);
	}
	for (auto i : PipelineLayouts) {
		vkDestroyPipelineLayout(Device, i, nullptr);
	}
	for (auto i : IndirectBuffers) {
		vkFreeMemory(Device, i.second, nullptr);
		vkDestroyBuffer(Device, i.first, nullptr);
	}
	for (auto i : IndexBuffers) {
		vkFreeMemory(Device, i.second, nullptr);
		vkDestroyBuffer(Device, i.first, nullptr);
	}
	for (auto i : VertexBuffers) {
		vkFreeMemory(Device, i.second, nullptr);
		vkDestroyBuffer(Device, i.first, nullptr);
	}
	if (VK_NULL_HANDLE != CommandPool) {
		vkDestroyCommandPool(Device, CommandPool, nullptr);
	}
	for (auto i : Swapchain.ImageAndViews) {
		vkDestroyImageView(Device, i.second, nullptr);
	}
	if (VK_NULL_HANDLE != Swapchain.VkSwapchain) {
		vkDestroySwapchainKHR(Device, Swapchain.VkSwapchain, nullptr);
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

void VK::CreateInstance(std::vector<const char*>& Extensions)
{
	constexpr VkApplicationInfo AI = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "VK App Name", .applicationVersion = VK_HEADER_VERSION_COMPLETE,
		.pEngineName = "VK Engine Name", .engineVersion = VK_HEADER_VERSION_COMPLETE,
		.apiVersion = VK_HEADER_VERSION_COMPLETE
	};
	const std::array Layers = {
		"VK_LAYER_KHRONOS_validation",
	};
#ifdef _DEBUG
	Extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	Extensions.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
#endif
	const VkInstanceCreateInfo ICI = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &AI,
		.enabledLayerCount = static_cast<uint32_t>(std::size(Layers)), .ppEnabledLayerNames = std::data(Layers),
		.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions)
	};
	VERIFY_SUCCEEDED(vkCreateInstance(&ICI, nullptr, &Instance));

#ifdef _DEBUG
	VkDebugUtilsMessengerEXT DebugUtilsMessenger = VK_NULL_HANDLE;
	vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
	vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
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
}

void VK::SelectPhysicalDevice() 
{
	std::vector<VkPhysicalDevice> PhysicalDevices;
	uint32_t Count = 0;
	VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, nullptr));
	PhysicalDevices.resize(Count);
	VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, std::data(PhysicalDevices)));

	//!< ここでは最大メモリの物理デバイスを採用する (Select max memory GPU here)
	SelectedPhysDevice.first = *std::ranges::max_element(PhysicalDevices, [](const auto& lhs, const auto& rhs) {
		VkPhysicalDeviceMemoryProperties MemPropL, MemPropR;
		vkGetPhysicalDeviceMemoryProperties(lhs, &MemPropL);
		vkGetPhysicalDeviceMemoryProperties(rhs, &MemPropR);
		const auto MemTotalL = std::accumulate(std::data(MemPropL.memoryHeaps), &MemPropL.memoryHeaps[MemPropL.memoryHeapCount], VkDeviceSize(0), [](auto Sum, const auto& rhs) { return Sum + rhs.size; });
		const auto MemTotalR = std::accumulate(std::data(MemPropR.memoryHeaps), &MemPropR.memoryHeaps[MemPropR.memoryHeapCount], VkDeviceSize(0), [](auto Sum, const auto& rhs) { return Sum + rhs.size; });
		return MemTotalL < MemTotalR;
		});
#ifdef _DEBUG
	VkPhysicalDeviceProperties PDP;
	vkGetPhysicalDeviceProperties(SelectedPhysDevice.first, &PDP);
	std::cout << PDP.deviceName << "is selected" << std::endl;
#endif
	//!< メモリプロパティを覚えておく (Remember memory properties)
	vkGetPhysicalDeviceMemoryProperties(SelectedPhysDevice.first, &SelectedPhysDevice.second);
}

void VK::SelectSurfaceFormat() 
{
	uint32_t Count = 0;
	VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysDevice.first, Surface, &Count, nullptr));
	std::vector<VkSurfaceFormatKHR> SFs(Count);
	VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(SelectedPhysDevice.first, Surface, &Count, std::data(SFs)));

	//!< ここでは最初に見つかった FORMAT_UNDEFINED でないものを採用する (Select first no FORMAT_UNDEFINED here)
	SelectedSurfaceFormat = *std::ranges::find_if(SFs, [](const auto& rhs) {
		if (VK_FORMAT_UNDEFINED != rhs.format) {
			return true;
		}
		return false;
		});
}

void VK::CreateDevice()
{
	//!< デバイス作成前に、キュー情報取得 (Before create device, get queue information)
	using FamilyIndexAndPriorities = std::map<uint32_t, std::vector<float>>;
	FamilyIndexAndPriorities FIAP;
	uint32_t GraphicsIndexInFamily;
	uint32_t PresentIndexInFamily;
	{
		std::vector<VkQueueFamilyProperties> QFPs;
		uint32_t Count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(SelectedPhysDevice.first, &Count, nullptr);
		QFPs.resize(Count);
		vkGetPhysicalDeviceQueueFamilyProperties(SelectedPhysDevice.first, &Count, std::data(QFPs));

		//!< 機能を持つキューファミリインデックスを立てる (If queue family index has function, set bit)
		std::bitset<32> GraphicsMask;
		std::bitset<32> PresentMask;
		for (auto i = 0; i < std::size(QFPs); ++i) {
			const auto& QFP = QFPs[i];
			if (VK_QUEUE_GRAPHICS_BIT & QFP.queueFlags) {
				GraphicsMask.set(i);
			}
			auto HasPresent = VK_FALSE;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceSupportKHR(SelectedPhysDevice.first, i, Surface, &HasPresent));
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
		vkGetPhysicalDeviceFeatures(SelectedPhysDevice.first, &PDF);
		const VkDeviceCreateInfo DCI = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &PDV13F,
			.flags = 0,
			.queueCreateInfoCount = static_cast<uint32_t>(std::size(DQCIs)), .pQueueCreateInfos = std::data(DQCIs),
			.enabledLayerCount = 0, .ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions),
			.pEnabledFeatures = &PDF
		};
		VERIFY_SUCCEEDED(vkCreateDevice(SelectedPhysDevice.first, &DCI, nullptr, &Device));
	}

	//!< デバイス作成後に、キューファミリインデックスとファミリ内でのインデックスから、キューを取得 (After create device, get queue from family index, index in family)
	vkGetDeviceQueue(Device, GraphicsQueue.second, GraphicsIndexInFamily, &GraphicsQueue.first);
	vkGetDeviceQueue(Device, PresentQueue.second, PresentIndexInFamily, &PresentQueue.first);
}

void VK::CreateFence()
{
	constexpr VkFenceCreateInfo FCI = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VERIFY_SUCCEEDED(vkCreateFence(Device, &FCI, nullptr, &Fence));
}

void VK::CreateSemaphore()
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

void VK::CreateSwapchain(const uint32_t Width, const uint32_t Height)
{
	VkSurfaceCapabilitiesKHR SC;
	VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(SelectedPhysDevice.first, Surface, &SC));

	Swapchain.Extent = 0xffffffff != SC.currentExtent.width ? SC.currentExtent : VkExtent2D({ .width = (std::clamp)(Width, SC.maxImageExtent.width, SC.minImageExtent.width), .height = (std::clamp)(Height, SC.minImageExtent.height, SC.minImageExtent.height) });

	std::vector<uint32_t> QueueFamilyIndices;
	if (GraphicsQueue.second != PresentQueue.second) {
		QueueFamilyIndices.emplace_back(GraphicsQueue.second);
		QueueFamilyIndices.emplace_back(PresentQueue.second);
	}

	VkPresentModeKHR SelectedPresentMode;
	{
		uint32_t Count;
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysDevice.first, Surface, &Count, nullptr));
		std::vector<VkPresentModeKHR> PMs(Count);
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(SelectedPhysDevice.first, Surface, &Count, std::data(PMs)));

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
		.imageExtent = Swapchain.Extent,
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
	VERIFY_SUCCEEDED(vkCreateSwapchainKHR(Device, &SCI, nullptr, &Swapchain.VkSwapchain));

	//!< イメージビュー (Image views)
	{
		std::vector<VkImage> Images;
		uint32_t Count;
		VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain.VkSwapchain, &Count, nullptr));
		Images.resize(Count);
		VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain.VkSwapchain, &Count, std::data(Images)));

		Swapchain.ImageAndViews.reserve(Count);
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
			VkImageView IV;
			VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, nullptr, &IV));

			Swapchain.ImageAndViews.emplace_back(ImageAndView({ i, IV }));
		}
	}
}

void VK::CreateCommandBuffer()
{
	const VkCommandPoolCreateInfo CPCI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = GraphicsQueue.second
	};
	VERIFY_SUCCEEDED(vkCreateCommandPool(Device, &CPCI, nullptr, &CommandPool));

	CommandBuffers.resize(std::size(Swapchain.ImageAndViews));
	const VkCommandBufferAllocateInfo CBAI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = CommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = static_cast<uint32_t>(std::size(CommandBuffers))
	};
	VERIFY_SUCCEEDED(vkAllocateCommandBuffers(Device, &CBAI, std::data(CommandBuffers)));
}

uint32_t VK::GetMemoryTypeIndex(const uint32_t TypeBits, const VkMemoryPropertyFlags MPF) const
{
	for (uint32_t i = 0; i < SelectedPhysDevice.second.memoryTypeCount; ++i) {
		if (TypeBits & (1 << i)) {
			if ((SelectedPhysDevice.second.memoryTypes[i].propertyFlags & MPF) == MPF) {
				return i;
			}
		}
	}
	return (std::numeric_limits<uint32_t>::max)();
}
void VK::CreateBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlagBits MPFB, const size_t Size, const void* Source) const
{
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
}
void VK::BufferMemoryBarrier(const VkCommandBuffer CB,
	const VkBuffer Buffer,
	const VkPipelineStageFlags SrcPSF, const VkPipelineStageFlags DstPSF,
	const VkAccessFlags SrcAF, const VkAccessFlags DstAF) const
{
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
}
void VK::PopulateCopyCommand(const VkCommandBuffer CB, const VkBuffer Staging, const VkBuffer Buffer, const size_t Size, const VkAccessFlags AF, const VkPipelineStageFlagBits PSF) const
{
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
}

void VK::CreatePipelineLayout()
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
	VERIFY_SUCCEEDED(vkCreatePipelineLayout(Device, &PLCI, nullptr, &PipelineLayouts.emplace_back()));
}

void VK::CreateRenderPass()
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
	VERIFY_SUCCEEDED(vkCreateRenderPass(Device, &RPCI, nullptr, &RenderPasses.emplace_back()));
}

VkShaderModule VK::CreateShaderModule(const std::filesystem::path& Path)
{
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

void VK::CreateFramebuffer()
{
	Framebuffers.reserve(std::size(Swapchain.ImageAndViews));
	for (const auto& i : Swapchain.ImageAndViews) {
		const std::array IVs = { i.second };
		const VkFramebufferCreateInfo FCI = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = RenderPasses[0],
			.attachmentCount = static_cast<uint32_t>(std::size(IVs)), .pAttachments = std::data(IVs),
			.width = Swapchain.Extent.width, .height = Swapchain.Extent.height,
			.layers = 1
		};
		VERIFY_SUCCEEDED(vkCreateFramebuffer(Device, &FCI, nullptr, &Framebuffers.emplace_back()));
	}
}

void VK::CreateViewports() 
{
	Viewports.emplace_back(VkViewport({
			.x = 0.0f, .y = static_cast<float>(Swapchain.Extent.height),
			.width = static_cast<float>(Swapchain.Extent.width), .height = -static_cast<float>(Swapchain.Extent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
			}));
	ScissorRects.emplace_back(VkRect2D({ .offset = VkOffset2D({.x = 0, .y = 0 }), .extent = VkExtent2D({.width = Swapchain.Extent.width, .height = Swapchain.Extent.height }) }));
}

void VK::PopulateCommand()
{
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
			} vkCmdEndRenderPass(CB);
		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
	}
}

void VK::WaitFence()
{
	const std::array Fences = { Fence };
	VERIFY_SUCCEEDED(vkWaitForFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences), VK_TRUE, (std::numeric_limits<uint64_t>::max)()));
	vkResetFences(Device, static_cast<uint32_t>(std::size(Fences)), std::data(Fences));
}

void VK::AcquireNextImage() 
{
	//!< 次のイメージインデックスを取得、イメージが取得出来たらセマフォ(A) がシグナルされる (Acquire next image index, on acquire semaphore A will be signaled)
	VERIFY_SUCCEEDED(vkAcquireNextImageKHR(Device, Swapchain.VkSwapchain, (std::numeric_limits<uint64_t>::max)(), NextImageAcquiredSemaphore, VK_NULL_HANDLE, &Swapchain.Index));
}

void VK::Submit()
{
	const auto& CB = CommandBuffers[Swapchain.Index];
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
void VK::Present()
{
	//!< セマフォ (B) がシグナル (レンダリング完了) される迄待つ
	const std::array WaitSems = { RenderFinishedSemaphore };
	const std::array Swapchains = { Swapchain.VkSwapchain };
	const std::array ImageIndices = { Swapchain.Index };
	const VkPresentInfoKHR PI = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(std::size(WaitSems)), .pWaitSemaphores = std::data(WaitSems),
		.swapchainCount = static_cast<uint32_t>(std::size(Swapchains)), .pSwapchains = std::data(Swapchains), .pImageIndices = std::data(ImageIndices),
		.pResults = nullptr
	};
	VERIFY_SUCCEEDED(vkQueuePresentKHR(PresentQueue.first, &PI));
}
