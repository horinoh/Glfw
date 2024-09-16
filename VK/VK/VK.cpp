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

	for (auto i : DescriptorPools) {
		//vkFreeDescriptorSets(Device, i, static_cast<uint32_t>(std::size(DescriptorSets)), std::data(DescriptorSets));
		vkDestroyDescriptorPool(Device, i, nullptr);
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
	for (auto i : DescriptorSetLayouts) {
		vkDestroyDescriptorSetLayout(Device, i, nullptr);
	}
	for (auto i : Samplers) {
		vkDestroySampler(Device, i, nullptr);
	}
	for (auto i : Textures) {
		vkFreeMemory(Device, i.second, nullptr);
		vkDestroyImageView(Device, i.first.second, nullptr);
		vkDestroyImage(Device, i.first.first, nullptr);
	}
	for (auto i : UniformBuffers) {
		vkFreeMemory(Device, i.second, nullptr);
		vkDestroyBuffer(Device, i.first, nullptr);
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
	for (auto& i : SecondaryCommandBuffers) {
		vkDestroyCommandPool(Device, i.first, nullptr);
	}
	for (auto& i : PrimaryCommandBuffers) {
		vkDestroyCommandPool(Device, i.first, nullptr);
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

	//!< デバイスプロパティを覚えておく (Remember device properties)
	vkGetPhysicalDeviceProperties(SelectedPhysDevice.first, &SelectedPhysDevice.second.PDP);
	//!< メモリプロパティを覚えておく (Remember memory properties)
	vkGetPhysicalDeviceMemoryProperties(SelectedPhysDevice.first, &SelectedPhysDevice.second.PDMP);
#ifdef _DEBUG
	std::cout << SelectedPhysDevice.second.PDP.deviceName << "is selected" << std::endl;
#endif
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
	//!< フェンスをシグナル状態で作成 (Fence, create as signaled) CPU - GPU
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

void VK::CreateCommandBuffer()
{
	AllocateCommandBuffers(CreateCommandPool(PrimaryCommandBuffers), std::size(Swapchain.ImageAndViews), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	AllocateCommandBuffers(CreateCommandPool(SecondaryCommandBuffers), std::size(Swapchain.ImageAndViews), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
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
	assert(VK_NULL_HANDLE != SM && "Shader module create failed");
	return SM;
};

void VK::CreateViewports() 
{
	Viewports.emplace_back(VkViewport({
			.x = 0.0f, .y = static_cast<float>(Swapchain.Extent.height),
			.width = static_cast<float>(Swapchain.Extent.width), .height = -static_cast<float>(Swapchain.Extent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
			}));
	ScissorRects.emplace_back(VkRect2D({ .offset = VkOffset2D({.x = 0, .y = 0 }), .extent = VkExtent2D({.width = Swapchain.Extent.width, .height = Swapchain.Extent.height }) }));
}

VK::CommandPoolAndBuffers& VK::CreateCommandPool(std::vector<VK::CommandPoolAndBuffers>& CPAB)
{
	const VkCommandPoolCreateInfo CPCI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = GraphicsQueue.second
	};
	return CreateCommandPool(CPAB, CPCI);
}
void VK::AllocateCommandBuffers(const size_t Count, VkCommandBuffer* CB, const VkCommandPool CP, const VkCommandBufferLevel CBL) 
{
	const VkCommandBufferAllocateInfo CBAI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = CP,
		.level = CBL,
		.commandBufferCount = static_cast<const uint32_t>(Count)
	};
	AllocateCommandBuffers(CB, CBAI);
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
	const auto& CB = PrimaryCommandBuffers[0].second[Swapchain.Index];
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

void VK::CreateInstance(const std::vector<const char*>& Extensions)
{
	constexpr VkApplicationInfo AI = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "VK App Name", .applicationVersion = VK_HEADER_VERSION_COMPLETE,
		.pEngineName = "VK Engine Name", .engineVersion = VK_HEADER_VERSION_COMPLETE,
		.apiVersion = VK_HEADER_VERSION_COMPLETE
	};
#ifdef _DEBUG
	const std::array LayersD = { "VK_LAYER_KHRONOS_validation" };
	auto ExtensionsD(Extensions);
	ExtensionsD.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	ExtensionsD.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
#else
	constexpr std::array<const char*, 0> Layers = {};
#endif
	const VkInstanceCreateInfo ICI = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &AI,
#ifdef _DEBUG
		.enabledLayerCount = static_cast<uint32_t>(std::size(LayersD)), .ppEnabledLayerNames = std::data(LayersD),
		.enabledExtensionCount = static_cast<uint32_t>(std::size(ExtensionsD)), .ppEnabledExtensionNames = std::data(ExtensionsD)
#else
		.enabledLayerCount = static_cast<uint32_t>(std::size(Layers)), .ppEnabledLayerNames = std::data(Layers),
		.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions)
#endif
	};
	VERIFY_SUCCEEDED(vkCreateInstance(&ICI, nullptr, &Instance));

#ifdef _DEBUG
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

uint32_t VK::GetMemoryTypeIndex(const uint32_t TypeBits, const VkMemoryPropertyFlags MPF) const
{
	for (uint32_t i = 0; i < SelectedPhysDevice.second.PDMP.memoryTypeCount; ++i) {
		if (TypeBits & (1 << i)) {
			if ((SelectedPhysDevice.second.PDMP.memoryTypes[i].propertyFlags & MPF) == MPF) {
				return i;
			}
		}
	}
	return (std::numeric_limits<uint32_t>::max)();
}
void VK::CreateBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlags MPF, const size_t Size, const void* Source) const
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

	const VkBufferMemoryRequirementsInfo2 BMRI = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
		.pNext = nullptr,
		.buffer = *Buffer
	};
	VkMemoryRequirements2 MR = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = nullptr,
	};
	vkGetBufferMemoryRequirements2(Device, &BMRI, &MR);
	
	const VkMemoryAllocateInfo MAI = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = MR.memoryRequirements.size,
		.memoryTypeIndex = GetMemoryTypeIndex(MR.memoryRequirements.memoryTypeBits, MPF)
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

void VK::CreateImage(VkImage* Image, VkDeviceMemory* DeviceMemory, const VkImageCreateInfo& ICI)
{
	VERIFY_SUCCEEDED(vkCreateImage(Device, &ICI, nullptr, Image));

	const VkImageMemoryRequirementsInfo2 IMRI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
		.pNext = nullptr,
		.image = *Image
	};
	VkMemoryRequirements2 MR = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = nullptr,
	};
	vkGetImageMemoryRequirements2(Device, &IMRI, &MR);

	const VkMemoryAllocateInfo MAI = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = MR.memoryRequirements.size,
		.memoryTypeIndex = GetMemoryTypeIndex(MR.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VERIFY_SUCCEEDED(vkAllocateMemory(Device, &MAI, nullptr, DeviceMemory));
	VERIFY_SUCCEEDED(vkBindImageMemory(Device, *Image, *DeviceMemory, 0));
}

void VK::BufferMemoryBarrier(const VkCommandBuffer CB,
	const VkBuffer Buffer,
	const VkPipelineStageFlags2 SrcPSF, const VkPipelineStageFlags2 DstPSF,
	const VkAccessFlags2 SrcAF, const VkAccessFlags2 DstAF) const
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
void VK::ImageMemoryBarrier(const VkCommandBuffer CB,
	const VkImage Image,
	const VkPipelineStageFlags2 SrcPSF, const VkPipelineStageFlags2 DstPSF,
	const VkAccessFlags2 SrcAF, const VkAccessFlags2 DstAF,
	const VkImageLayout OldIL, const VkImageLayout NewIL) const
{
	constexpr std::array<VkMemoryBarrier2, 0> MBs = {};
	constexpr std::array<VkBufferMemoryBarrier2, 0> BMBs = {};
	const std::array IMBs = {
		VkImageMemoryBarrier2({
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = SrcPSF, .srcAccessMask = SrcAF, .dstStageMask = DstPSF, .dstAccessMask = DstAF,
			.oldLayout = OldIL, .newLayout = NewIL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = Image,
			.subresourceRange = VkImageSubresourceRange({
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			})
		}),
	};
	const VkDependencyInfo DI = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
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

void VK::CreateGeometry(const std::vector<VK::GeometryCreateInfo>& GCIs)
{
	struct GeometryCreateCommand {
		const VK::GeometryCreateInfo* GCI = nullptr;

		std::vector<BufferAndDeviceMemory> VertexStagingBuffers = {};
		BufferAndDeviceMemory IndexStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		BufferAndDeviceMemory IndirectStagingBuffer = BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE });
		
		VkDrawIndexedIndirectCommand DIIC;
		VkDrawIndirectCommand DIC;

		size_t VertexStart = 0, IndexStart = 0, IndirectStart = 0;
	};
	std::vector<GeometryCreateCommand> GCCs;

	for (const auto& i : GCIs) {
		auto& GCC = GCCs.emplace_back();
		GCC.GCI = &i;

		//!< バーテックスバッファ、ステージングの作成 (Create vertex buffer, staging)
		for (const auto& j : i.Vtxs) {
			auto& VSB = GCC.VertexStagingBuffers.emplace_back(BufferAndDeviceMemory({ VK_NULL_HANDLE, VK_NULL_HANDLE }));
			GCC.VertexStart = std::size(VertexBuffers);
			CreateDeviceLocalBuffer(VertexBuffers.emplace_back(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, j.first);
			CreateHostVisibleBuffer(VSB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, j.first, j.second);
		}

		//!< インデックスバッファ、ステージングの作成 (Create index buffer, staging)
		const auto HasIdx = i.Idx.first != 0 && i.Idx.second != nullptr && i.IdxCount != 0;
		if (HasIdx) {
			GCC.IndexStart = std::size(IndexBuffers);
			CreateDeviceLocalBuffer(IndexBuffers.emplace_back(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, i.Idx.first);
			CreateHostVisibleBuffer(GCC.IndexStagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, i.Idx.first, i.Idx.second);
		}

		//!< インダイレクトバッファ、ステージングの作成 (Create indirect buffer, staging)
		GCC.IndirectStart = std::size(IndirectBuffers);
		if (HasIdx) {
			GCC.DIIC = VkDrawIndexedIndirectCommand({
				.indexCount = i.IdxCount,
				.instanceCount = i.InstCount,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = 0
			});
			CreateDeviceLocalBuffer(IndirectBuffers.emplace_back(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(GCC.DIIC));
			CreateHostVisibleBuffer(GCC.IndirectStagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(GCC.DIIC), &GCC.DIIC);
		}
		else {
			GCC.DIC = VkDrawIndirectCommand({
				.vertexCount = i.VtxCount,
				.instanceCount = i.InstCount,
				.firstVertex = 0,
				.firstInstance = 0
			});
			CreateDeviceLocalBuffer(IndirectBuffers.emplace_back(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(GCC.DIC));
			CreateHostVisibleBuffer(GCC.IndirectStagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(GCC.DIC), &GCC.DIC);
		}
	}

	const auto& CB = PrimaryCommandBuffers[0].second[0];
	constexpr VkCommandBufferBeginInfo CBBI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr
	};
	VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
		for (const auto& i : GCCs) {
			for (auto j = 0; j < std::size(i.GCI->Vtxs); ++j) {
				PopulateCopyCommand(CB, i.VertexStagingBuffers[j].first, VertexBuffers[i.VertexStart + j].first, i.GCI->Vtxs[j].first, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			}
			if (VK_NULL_HANDLE != i.IndexStagingBuffer.first) {
				PopulateCopyCommand(CB, i.IndexStagingBuffer.first, IndexBuffers[i.IndexStart].first, i.GCI->Idx.first, VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
				PopulateCopyCommand(CB, i.IndirectStagingBuffer.first, IndirectBuffers[i.IndirectStart].first, sizeof(i.DIIC), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
			}
			else {
				PopulateCopyCommand(CB, i.IndirectStagingBuffer.first, IndirectBuffers[i.IndirectStart].first, sizeof(i.DIC), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
			}
		}
	} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));

	//!< コピーコマンド発行 (Submit copy command)
	SubmitAndWait(CB);

	for (const auto& i : GCCs) {
		for (auto& j : i.VertexStagingBuffers) {
			vkFreeMemory(Device, j.second, nullptr);
			vkDestroyBuffer(Device, j.first, nullptr);
		}
		if (VK_NULL_HANDLE != i.IndexStagingBuffer.first) {
			vkFreeMemory(Device, i.IndexStagingBuffer.second, nullptr);
			vkDestroyBuffer(Device, i.IndexStagingBuffer.first, nullptr);
		}
		vkFreeMemory(Device, i.IndirectStagingBuffer.second, nullptr);
		vkDestroyBuffer(Device, i.IndirectStagingBuffer.first, nullptr);
	}
}

void VK::CreateTexture(const VkFormat Format, const uint32_t Width, const uint32_t Height, const VkImageUsageFlags IUF, const VkImageAspectFlags IAF) 
{
	auto& Tex = Textures.emplace_back();
	auto& Image = Tex.first.first;
	auto& ImageView = Tex.first.second;
	auto& DeviceMemory = Tex.second;

	constexpr std::array<uint32_t, 0> QFIs = {};
	const VkImageCreateInfo ICI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = Format,
		.extent = VkExtent3D({.width = Width, .height = Height, .depth = 1 }),
		.mipLevels = 1, .arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = IUF,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = static_cast<uint32_t>(std::size(QFIs)), .pQueueFamilyIndices = std::data(QFIs),
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	CreateImage(&Image, &DeviceMemory, ICI);

	const VkImageViewCreateInfo IVCI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = Image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = Format,
		.components = VkComponentMapping({
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A
		}),
		.subresourceRange = VkImageSubresourceRange({
			.aspectMask = IAF,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
		})
	};
	CreateImageView(&ImageView, IVCI);
}

VkFormat VK::ToVkFormat(const gli::format GLIFormat) 
{
	switch (GLIFormat) {
		using enum gli::format;
	case FORMAT_BGR8_UNORM_PACK32: return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
	case FORMAT_L8_UNORM_PACK8: return VK_FORMAT_R4G4_UNORM_PACK8;
	}
	assert(true && "Format not found");
	return VK_FORMAT_UNDEFINED;
}
VkImageType VK::ToVkImageType(const gli::target GLITarget)
{
	switch (GLITarget) {
		using enum gli::target;
	case TARGET_1D:
	case TARGET_1D_ARRAY:
		return VK_IMAGE_TYPE_1D;
	case TARGET_2D:
	case TARGET_2D_ARRAY:
	case TARGET_CUBE:
	case TARGET_CUBE_ARRAY:
		return VK_IMAGE_TYPE_2D;
	case TARGET_3D:
		return VK_IMAGE_TYPE_3D;
	}
	assert(true && "Image type not found");
	return VK_IMAGE_TYPE_MAX_ENUM;
}
VkImageViewType VK::ToVkImageViewType(const gli::target GLITarget)
{
	switch (GLITarget) {
		using enum gli::target;
	case TARGET_1D: return VK_IMAGE_VIEW_TYPE_1D;
	case TARGET_1D_ARRAY:return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	case TARGET_2D:return VK_IMAGE_VIEW_TYPE_2D;
	case TARGET_2D_ARRAY:return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case TARGET_3D:return VK_IMAGE_VIEW_TYPE_3D;
	case TARGET_CUBE:return VK_IMAGE_VIEW_TYPE_CUBE;
	case TARGET_CUBE_ARRAY:return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
	assert(true && "Image view type not found");
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}
VkComponentSwizzle VK::ToVkComponentSwizzle(const gli::swizzle GLISwizzle)
{
	switch (GLISwizzle) {
	case gli::SWIZZLE_ZERO: return VK_COMPONENT_SWIZZLE_ZERO;
	case gli::SWIZZLE_ONE: return VK_COMPONENT_SWIZZLE_ONE;
	case gli::SWIZZLE_RED: return VK_COMPONENT_SWIZZLE_R;
	case gli::SWIZZLE_GREEN: return VK_COMPONENT_SWIZZLE_G;
	case gli::SWIZZLE_BLUE: return VK_COMPONENT_SWIZZLE_B;
	case gli::SWIZZLE_ALPHA: return VK_COMPONENT_SWIZZLE_A;
	}
	assert(true && "Swizzle not found");
	return VK_COMPONENT_SWIZZLE_IDENTITY;
}
VkComponentMapping VK::ToVkComponentMapping(const gli::texture::swizzles_type GLISwizzleType)
{
	return VkComponentMapping({ 
		.r = ToVkComponentSwizzle(GLISwizzleType.r), 
		.g = ToVkComponentSwizzle(GLISwizzleType.g), 
		.b = ToVkComponentSwizzle(GLISwizzleType.b),
		.a = ToVkComponentSwizzle(GLISwizzleType.a) });
}
void VK::CreateGLITexture(const std::filesystem::path& Path) 
{
	auto& Tex = Textures.emplace_back();
	auto& Image = Tex.first.first;
	auto& ImageView = Tex.first.second;
	auto& DeviceMemory = Tex.second;

	auto Gli = gli::load(std::data(Path.string()));
	constexpr std::array<uint32_t, 0> QFIs = {};
	const VkImageCreateInfo ICI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = gli::is_target_cube(Gli.target()) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : static_cast<VkImageCreateFlags>(0),
		.imageType = ToVkImageType(Gli.target()), 
		.format = ToVkFormat(Gli.format()),
		.extent = VkExtent3D({
			.width = static_cast<const uint32_t>(Gli.extent(0).x),
			.height = static_cast<const uint32_t>(Gli.extent(0).y),
			.depth = static_cast<const uint32_t>(Gli.extent(0).z)
		}),
		.mipLevels = static_cast<const uint32_t>(Gli.levels()),
		.arrayLayers = static_cast<const uint32_t>(Gli.layers()) * static_cast<const uint32_t>(Gli.faces()),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = static_cast<uint32_t>(std::size(QFIs)), .pQueueFamilyIndices = std::data(QFIs),
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	CreateImage(&Image, &DeviceMemory, ICI);

	const VkImageViewCreateInfo IVCI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = Image,
		.viewType = ToVkImageViewType(Gli.target()),
		.format = ToVkFormat(Gli.format()),
		.components = ToVkComponentMapping(Gli.swizzles()),
		.subresourceRange = VkImageSubresourceRange({
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
		})
	};
	CreateImageView(&ImageView, IVCI);
}

void VK::CreateRenderPass(const VkAttachmentLoadOp ALO, const VkAttachmentStoreOp ASO,
	const VkImageLayout Init, const VkImageLayout Final)
{
	const std::vector ADs = {
		VkAttachmentDescription({
			.flags = 0,
			.format = SelectedSurfaceFormat.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = ALO, .storeOp = ASO,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = Init, .finalLayout = Final
		}),
	};

	constexpr std::array<VkAttachmentReference, 0> IAs = {};
	constexpr std::array CAs = { VkAttachmentReference({.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }), };
	constexpr std::array RAs = { VkAttachmentReference({.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_UNDEFINED }), };
	constexpr std::array<uint32_t, 0> PAs = {};
	const std::vector SDs = {
		VkSubpassDescription({
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = static_cast<uint32_t>(std::size(IAs)), .pInputAttachments = std::data(IAs),
			.colorAttachmentCount = static_cast<uint32_t>(std::size(CAs)), .pColorAttachments = std::data(CAs),
			.pResolveAttachments = std::data(RAs),
			.pDepthStencilAttachment = nullptr,
			.preserveAttachmentCount = static_cast<uint32_t>(std::size(PAs)), .pPreserveAttachments = std::data(PAs)
		}),
	};
	CreateRenderPass(ADs, SDs);
}
void VK::CreateRenderPass_Depth(const VkImageLayout Init, const VkImageLayout Final)
{
	const std::vector ADs = {
		VkAttachmentDescription({
			.flags = 0,
			.format = SelectedSurfaceFormat.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = Init, .finalLayout = Final
		}),
		VkAttachmentDescription({
			.flags = 0,
			.format = VK_FORMAT_D24_UNORM_S8_UINT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}),
	};

	constexpr std::array<VkAttachmentReference, 0> IAs = {};
	constexpr std::array CAs = { VkAttachmentReference({.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }), };
	constexpr std::array RAs = { VkAttachmentReference({.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_UNDEFINED }), };
	constexpr auto DA = VkAttachmentReference({ .attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
	constexpr std::array<uint32_t, 0> PAs = {};
	const std::vector SDs = {
		VkSubpassDescription({
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = static_cast<uint32_t>(std::size(IAs)), .pInputAttachments = std::data(IAs),
			.colorAttachmentCount = static_cast<uint32_t>(std::size(CAs)), .pColorAttachments = std::data(CAs), 
			.pResolveAttachments = std::data(RAs),
			.pDepthStencilAttachment = &DA,
			.preserveAttachmentCount = static_cast<uint32_t>(std::size(PAs)), .pPreserveAttachments = std::data(PAs)
		}),
	};
	CreateRenderPass(ADs, SDs);
}

void VK::CreatePipeline(VkPipeline& PL,
	const std::vector<VkPipelineShaderStageCreateInfo>& PSSCIs,
	const VkPipelineVertexInputStateCreateInfo& PVISCI,
	const VkPipelineInputAssemblyStateCreateInfo& PIASCI,
	const VkPipelineTessellationStateCreateInfo& PTSCI,
	const VkPipelineViewportStateCreateInfo& PVSCI,
	const VkPipelineRasterizationStateCreateInfo& PRSCI,
	const VkPipelineMultisampleStateCreateInfo& PMSCI,
	const VkPipelineDepthStencilStateCreateInfo& PDSSCI,
	const VkPipelineColorBlendStateCreateInfo& PCBSCI,
	const VkPipelineDynamicStateCreateInfo& PDSCI,
	const VkPipelineLayout PLL,
	const VkRenderPass RP)
{
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
			.layout = PLL,
			.renderPass = RP, .subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE, .basePipelineIndex = -1
		})
	};
	VERIFY_SUCCEEDED(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, static_cast<uint32_t>(std::size(GPCIs)), std::data(GPCIs), nullptr, &PL));
}
void VK::CreatePipeline(VkPipeline& PL, 
	const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TES, const VkShaderModule TCS, const VkShaderModule GS,
	const std::vector<VkVertexInputBindingDescription>& VIBDs, const std::vector<VkVertexInputAttributeDescription>& VIADs,
	const VkPrimitiveTopology PT,
	const uint32_t PatchControlPoints,
	const VkPolygonMode PM, const VkCullModeFlags CMF, const VkFrontFace FF,
	const VkBool32 DepthEnable,
	const VkPipelineLayout PLL,
	const VkRenderPass RP) 
{
	std::vector<VkPipelineShaderStageCreateInfo> PSSCIs;
	if (VK_NULL_HANDLE != VS) {
		PSSCIs.emplace_back(VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = VS, .pName = "main", .pSpecializationInfo = nullptr }));
	}
	if (VK_NULL_HANDLE != FS) {
		PSSCIs.emplace_back(VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = FS, .pName = "main", .pSpecializationInfo = nullptr }));
	}
	if (VK_NULL_HANDLE != TES) {
		PSSCIs.emplace_back(VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, .module = TES, .pName = "main", .pSpecializationInfo = nullptr }));
	}
	if (VK_NULL_HANDLE != TCS) {
		PSSCIs.emplace_back(VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, .module = TCS, .pName = "main", .pSpecializationInfo = nullptr }));
	}
	if (VK_NULL_HANDLE != GS) {
		PSSCIs.emplace_back(VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = GS, .pName = "main", .pSpecializationInfo = nullptr }));
	}

	const VkPipelineVertexInputStateCreateInfo PVISCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = static_cast<uint32_t>(std::size(VIBDs)), .pVertexBindingDescriptions = std::data(VIBDs),
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(VIADs)), .pVertexAttributeDescriptions = std::data(VIADs)
	};

	const VkPipelineInputAssemblyStateCreateInfo PIASCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = PT,
		.primitiveRestartEnable = VK_FALSE
	};

	const VkPipelineTessellationStateCreateInfo PTSCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.patchControlPoints = PatchControlPoints
	};

	//!< ダイナミックステートにするのでここでは決め打ち
	constexpr VkPipelineViewportStateCreateInfo PVSCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1, .pViewports = nullptr,
		.scissorCount = 1, .pScissors = nullptr
	};

	const VkPipelineRasterizationStateCreateInfo PRSCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = PM,
		.cullMode = CMF,
		.frontFace = FF,
		.depthBiasEnable = VK_FALSE, .depthBiasConstantFactor = 0.0f, .depthBiasClamp = 0.0f, .depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
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

	const VkPipelineDepthStencilStateCreateInfo PDSSCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = DepthEnable, .depthWriteEnable = DepthEnable, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
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

	CreatePipeline(PL, PSSCIs, PVISCI, PIASCI, PTSCI, PVSCI, PRSCI, PMSCI, PDSSCI, PCBSCI, PDSCI, PLL, RP);
}

void VK::CreateFramebuffer(const VkRenderPass RP)
{
	for (const auto& i : Swapchain.ImageAndViews) {
		const std::array IVs = { i.second };
		const VkFramebufferCreateInfo FCI = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = RP,
			.attachmentCount = static_cast<uint32_t>(std::size(IVs)), .pAttachments = std::data(IVs),
			.width = Swapchain.Extent.width, .height = Swapchain.Extent.height,
			.layers = 1
		};
		VERIFY_SUCCEEDED(vkCreateFramebuffer(Device, &FCI, nullptr, &Framebuffers.emplace_back()));
	}
}

void VK::PopulateSecondaryCommandBuffer([[maybe_unused]] const int i) 
{
#if 0
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

		const auto PLL = PipelineLayouts[0];
		const auto DS = DescriptorSets[0];

		const auto VB = VertexBuffers[0].first;
		const auto IB = IndexBuffers[0].first;
		const auto IDB = IndirectBuffers[0].first;

		vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

		vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
		vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

		const std::array DSs = { DS };
		const std::array DynamicOffsets = { uint32_t(0) };
		vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PLL, 0, static_cast<uint32_t>(std::size(DSs)), std::data(DSs), static_cast<uint32_t>(std::size(DynamicOffsets)), std::data(DynamicOffsets));

		const std::array VBs = { VB };
		const std::array Offsets = { VkDeviceSize(0) };
		vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), std::data(Offsets));
		vkCmdBindIndexBuffer(CB, IB, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexedIndirect(CB, IDB, 0, 1, 0);

	}VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
#endif
}
void VK::PopulatePrimaryCommandBuffer(const int i) 
{
	const auto CB = PrimaryCommandBuffers[0].second[i];

	constexpr VkCommandBufferBeginInfo CBBI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};
	VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
		const auto RP = RenderPasses[0];
		const auto FB = Framebuffers[i];

		constexpr std::array CVs = { VkClearValue({.color = { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } }) };
		const VkRenderPassBeginInfo RPBI = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = RP,
			.framebuffer = FB,
			.renderArea = VkRect2D({.offset = VkOffset2D({.x = 0, .y = 0 }), .extent = Swapchain.Extent }),
			.clearValueCount = static_cast<uint32_t>(size(CVs)), .pClearValues = data(CVs)
		};
#if 1
		vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
			//const auto PL = Pipelines[0];
			
			//vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

			vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
			vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));
		} vkCmdEndRenderPass(CB);
#else
		vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); {
			const auto SCB = SecondaryCommandBuffers[0].second[i];
			const std::array SCBs = { SCB };
			vkCmdExecuteCommands(CB, static_cast<uint32_t>(std::size(SCBs)), std::data(SCBs));
		} vkCmdEndRenderPass(CB);
#endif	
	} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
}

void VK::SubmitAndWait(const VkCommandBuffer CB)
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