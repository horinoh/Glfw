#pragma once

#include <iostream>
#include <vector>
#include <utility>
#include <numeric>
#include <filesystem>
#include <numbers>
#include <thread>

#include <Vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <gli/gli.hpp>
#include <gli/target.hpp>

#ifdef _WIN64
#pragma comment(lib, "vulkan-1.lib")
#endif

#ifdef _DEBUG
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << VR << std::endl; /*__debugbreak();*/ } }
#else
#define VERIFY_SUCCEEDED(X) (X) 
#endif

#define SIZE_DATA(X) VK::SizeAndDataPtr({ sizeof(X), std::data(X) })
#define SIZE_DATA_NULL VK::SizeAndDataPtr({ 0, nullptr })

class VK
{
public:
	struct PhysDevProp
	{
		VkPhysicalDeviceProperties PDP;
		VkPhysicalDeviceMemoryProperties PDMP;
	};
	using PhysicalDeviceAndProps = std::pair<VkPhysicalDevice, PhysDevProp>;
	using QueueAndFamilyIndex = std::pair<VkQueue, uint32_t>;
	using BufferAndDeviceMemory = std::pair<VkBuffer, VkDeviceMemory>;
	using ImageAndView = std::pair<VkImage, VkImageView>;
	using ImageAndDeviceMemory = std::pair<ImageAndView, VkDeviceMemory>;

	virtual ~VK();

	virtual void CreateInstance(std::vector<const char*>& Extensions);
	virtual void SelectPhysicalDevice();
	virtual void CreateSurface() {}
	virtual void SelectSurfaceFormat();
	virtual void CreateDevice();
	virtual void CreateFence();
	virtual void CreateSemaphore();
	virtual void CreateSwapchain(const uint32_t Width, const uint32_t Height);
	virtual void CreateCommandBuffer();

	uint32_t GetMemoryTypeIndex(const uint32_t TypeBits, const VkMemoryPropertyFlags MPF) const;

	void CreateBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlags MPF, const size_t Size, const void* Source = nullptr) const;
	void CreateBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlags MPF, const size_t Size, const void* Source = nullptr) const {
		CreateBuffer(&BADM.first, &BADM.second, BUF, MPF, Size, Source);
	}
	void CreateDeviceLocalBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size) const {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Size);
	}
	void CreateDeviceLocalBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const size_t Size) const { CreateDeviceLocalBuffer(&BADM.first, &BADM.second, BUF, Size); }

	void CreateHostVisibleBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size, const void* Source) const {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Size, Source);
	}
	void CreateHostVisibleBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const size_t Size, const void* Source) const { CreateHostVisibleBuffer(&BADM.first, &BADM.second, BUF, Size, Source); }

	void BufferMemoryBarrier(const VkCommandBuffer CB,
		const VkBuffer Buffer,
		const VkPipelineStageFlags SrcPSF, const VkPipelineStageFlags DstPSF,
		const VkAccessFlags SrcAF, const VkAccessFlags DstAF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB, const VkBuffer Staging, const VkBuffer Buffer, const size_t Size, const VkAccessFlags AF, const VkPipelineStageFlagBits PSF) const;

	virtual void CreateGeometry() {}
	virtual void CreateUniformBuffer() {}
	virtual void CreateTexture() {}
	virtual void CreatePipelineLayout();
	virtual void CreateRenderPass();
	VkShaderModule CreateShaderModule(const std::filesystem::path& Path);
	virtual void CreatePipeline() {}
	virtual void CreateFramebuffer();
	virtual void CreateDescriptor() {}
	virtual void CreateViewports();

	virtual void PopulateCommand();

	virtual void WaitFence();
	virtual void AcquireNextImage();
	virtual void OnUpdate() {}
	virtual void Submit();
	virtual void Present();

public:
	using SizeAndDataPtr = std::pair<const size_t, const void*>;
	struct GeometryCreateInfo {
		std::vector<SizeAndDataPtr> Vtxs = {};
		SizeAndDataPtr Idx = SIZE_DATA_NULL;
		uint32_t IdxCount = 0, VtxCount = 0, InstCount = 0;
	};
	void CreateGeometry(const std::vector<GeometryCreateInfo>& GCIs);
	void CreateGeometry(const std::vector<SizeAndDataPtr>& Vtxs, const SizeAndDataPtr& Idx, const uint32_t IdxCount, const uint32_t InstCount) {
		CreateGeometry({
			VK::GeometryCreateInfo({
				.Vtxs = Vtxs,
				.Idx = Idx,
				.IdxCount = IdxCount,
				.VtxCount = 0,
				.InstCount = InstCount })
			});
	}
	void CreateGeometry(const std::vector<SizeAndDataPtr>& Vtxs, const uint32_t VtxCount, const uint32_t InstCount) {
		CreateGeometry({
			VK::GeometryCreateInfo({
				.Vtxs = Vtxs,
				.Idx = SIZE_DATA_NULL,
				.IdxCount = 0,
				.VtxCount = VtxCount,
				.InstCount = InstCount })
			});
	}
	void CreateGeometry(const SizeAndDataPtr& Vtx, const SizeAndDataPtr& Idx, const uint32_t IdxCount, const uint32_t InstCount) {
		CreateGeometry({
			VK::GeometryCreateInfo({
				.Vtxs = { Vtx },
				.Idx = Idx,
				.IdxCount = IdxCount,
				.VtxCount = 0,
				.InstCount = InstCount })
			});
	}
	void CreateGeometry(const SizeAndDataPtr& Vtx, const uint32_t VtxCount, const uint32_t InstCount) {
		CreateGeometry({
			VK::GeometryCreateInfo({
				.Vtxs = { Vtx },
				.Idx = SIZE_DATA_NULL,
				.IdxCount = 0,
				.VtxCount = VtxCount,
				.InstCount = InstCount
				})
			});
	}
	void CreateGeometry(const uint32_t VtxCount, const uint32_t InstCount) {
		CreateGeometry({
			VK::GeometryCreateInfo({
				.Vtxs = {},
				.Idx = SIZE_DATA_NULL,
				.IdxCount = 0,
				.VtxCount = VtxCount,
				.InstCount = InstCount })
			});
	}

#if 0
	void CreateImage(const VkImageCreateInfo& ICI) {
		auto& Tex = Textures.emplace_back();

		auto& Image = Textures.back().first.first;
		VERIFY_SUCCEEDED(vkCreateImage(Device, &ICI, nullptr, &Image));
		
		//!< イメージメモリを確保してバインド
		auto& DevMem = Textures.back().second;
		VkMemoryRequirements MR;
		vkGetImageMemoryRequirements(Device, Image, &MR);
		const VkMemoryAllocateInfo MAI = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = MR.size,
			.memoryTypeIndex = GetMemoryTypeIndex(MR.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		};
		VERIFY_SUCCEEDED(vkAllocateMemory(Device, &MAI, nullptr, &DevMem));
		VERIFY_SUCCEEDED(vkBindImageMemory(Device, Image, DevMem, 0));
	}
	void CreateView(const VkImageViewCreateInfo& IVCI) {
		auto& View = Textures.back().first.second;
		VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, nullptr, &View));
	}
	void CreateGLI(const std::filesystem::path& Path) {
		auto Gli = gli::load(std::data(Path.string()));

		const VkImageCreateInfo ICI = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = gli::is_target_cube(Gli.target()) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : static_cast<VkImageCreateFlags>(0),
			.imageType = VK_IMAGE_TYPE_2D, // from Gli.target()
			.format = VK_FORMAT_R8G8B8_UNORM, // from Gli.format()
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
			//.queueFamilyIndexCount = static_cast<uint32_t>(std::size(QueueFamilyIndices)), .pQueueFamilyIndices = std::data(QueueFamilyIndices),
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		CreateImage(ICI);

		const auto Image = Textures.back().first.first;
		const VkImageViewCreateInfo IVCI = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D, // from Gli.target()
			.format = VK_FORMAT_R8G8B8_UNORM, // from Gli.format()
			.components = VkComponentMapping({ // from Gli.swizzles()
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A
			}),
			.subresourceRange = VkImageSubresourceRange({
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount = VK_REMAINING_ARRAY_LAYERS
			})
		};
		CreateView(IVCI);
	}
#endif

	void CreatePipeline(VkPipeline& PL,
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
		const VkRenderPass RP);
	//!< 引数をよく使うものに絞ったもの
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TCS, const VkShaderModule TES, const VkShaderModule GS,
		const std::vector<VkVertexInputBindingDescription>& VIBDs, const std::vector<VkVertexInputAttributeDescription>& VIADs,
		const VkPrimitiveTopology PT,
		const uint32_t PatchControlPoints,
		const VkPolygonMode PM, const VkCullModeFlags CMF, const VkFrontFace FF,
		const VkBool32 DepthEnable,
		const VkPipelineLayout PLL,
		const VkRenderPass RP);
	//!< 頂点データによる通常メッシュ描画等
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS,
		const std::vector<VkVertexInputBindingDescription>& VIBDs, const std::vector<VkVertexInputAttributeDescription>& VIADs,
		const VkBool32 DepthEnable,
		const VkPipelineLayout PLL,
		const VkRenderPass RP) {
		CreatePipeline(PL,
			VS, FS, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
			VIBDs, VIADs,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			0,
			VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			DepthEnable,
			PLL,
			RP);
	}
	//!< 全画面ポリゴン描画等
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS,
		const VkPipelineLayout PLL,
		const VkRenderPass RP) {
		CreatePipeline(PL,
			VS, FS, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
			{}, {},
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			0,
			VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,
			PLL,
			RP);
	}
	//!< テッセレーションによる描画等
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TCS, const VkShaderModule TES, const VkShaderModule GS,
		const VkPipelineLayout PLL,
		const VkRenderPass RP) {
		CreatePipeline(PL,
			VS, FS, TCS, TES, GS,
			{}, {},
			VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
			1,
			VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_TRUE,
			PLL,
			RP);
	}

	void CreateSampler(const VkSamplerCreateInfo& SCI) { VERIFY_SUCCEEDED(vkCreateSampler(Device, &SCI, nullptr, &Samplers.emplace_back())); }
	void CreateSampler(const VkFilter Filter, const VkSamplerMipmapMode SMM, const VkSamplerAddressMode SAM) {
		const VkSamplerCreateInfo SCI = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = Filter, .minFilter = Filter, .mipmapMode = SMM,
			.addressModeU = SAM, .addressModeV = SAM, .addressModeW = SAM,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE, .maxAnisotropy = 1.0f,
			.compareEnable = VK_FALSE, .compareOp = VK_COMPARE_OP_NEVER,
			.minLod = 0.0f, .maxLod = 1.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			.unnormalizedCoordinates = VK_FALSE
		};
		CreateSampler(SCI);
	}
	void CreateSamplerLR() { CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT); }
	void CreateSamplerLC() { CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE); }
	void CreateSamplerNR() { CreateSampler(VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT); }
	void CreateSamplerNC() { CreateSampler(VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE); }

	VkDescriptorSetLayout CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& DSLCI) {
		auto& DSL = DescriptorSetLayouts.emplace_back();
		VERIFY_SUCCEEDED(vkCreateDescriptorSetLayout(Device, &DSLCI, nullptr, &DSL));
		return DSL;
	}
	VkDescriptorSetLayout CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& DSLBs) {
		const VkDescriptorSetLayoutCreateInfo DSLCI = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = static_cast<uint32_t>(std::size(DSLBs)), .pBindings = std::data(DSLBs)
		};
		return CreateDescriptorSetLayout(DSLCI);
	}

	void CreatePipelineLayout(const VkPipelineLayoutCreateInfo& PLCI) { VERIFY_SUCCEEDED(vkCreatePipelineLayout(Device, &PLCI, nullptr, &PipelineLayouts.emplace_back())); }
	void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& DSLs) {
		constexpr std::array<VkPushConstantRange, 0> PCRs;
		const VkPipelineLayoutCreateInfo PLCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = static_cast<uint32_t>(std::size(DSLs)), .pSetLayouts = std::data(DSLs),
			.pushConstantRangeCount = static_cast<uint32_t>(std::size(PCRs)), .pPushConstantRanges = std::data(PCRs)
		};
		CreatePipelineLayout(PLCI);
	}

	VkDescriptorPool CreateDescriptorPool(const VkDescriptorPoolCreateInfo& DPCI) {
		auto& DP = DescriptorPools.emplace_back();
		VERIFY_SUCCEEDED(vkCreateDescriptorPool(Device, &DPCI, nullptr, &DP));
		return DP;
	}
	VkDescriptorPool CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& DPSs) {
		const VkDescriptorPoolCreateInfo DPCI = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = std::ranges::max_element(DPSs, [](const auto& lhs, const auto& rhs) { return lhs.descriptorCount < rhs.descriptorCount; })->descriptorCount,
			.poolSizeCount = static_cast<uint32_t>(std::size(DPSs)), .pPoolSizes = std::data(DPSs)
		};
		return CreateDescriptorPool(DPCI);
	}

	void AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& DSAI) { VERIFY_SUCCEEDED(vkAllocateDescriptorSets(Device, &DSAI, &DescriptorSets.emplace_back())); }
	void AllocateDescriptorSets(const VkDescriptorPool DP, const std::vector<VkDescriptorSetLayout>& DSLs) {
		const VkDescriptorSetAllocateInfo DSAI = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = DP,
			.descriptorSetCount = static_cast<uint32_t>(std::size(DSLs)), .pSetLayouts = std::data(DSLs)
		};
		AllocateDescriptorSets(DSAI);
	}

	VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDescriptorUpdateTemplate& DUT, const VkDescriptorUpdateTemplateCreateInfo& DUTCI) {
		VERIFY_SUCCEEDED(vkCreateDescriptorUpdateTemplate(Device, &DUTCI, nullptr, &DUT));
		return DUT;
	}
	VkDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(VkDescriptorUpdateTemplate &DUT, const std::vector<VkDescriptorUpdateTemplateEntry>& DUTEs, const VkDescriptorSetLayout DSL) {
		const VkDescriptorUpdateTemplateCreateInfo DUTCI = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.descriptorUpdateEntryCount = static_cast<uint32_t>(std::size(DUTEs)), .pDescriptorUpdateEntries = std::data(DUTEs),
			.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,
			.descriptorSetLayout = DSL,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.pipelineLayout = VK_NULL_HANDLE, .set = 0
		};
		return CreateDescriptorUpdateTemplate(DUT, DUTCI);
	}

	virtual void SubmitAndWait(const VkCommandBuffer CB);

protected:
	VkInstance Instance = VK_NULL_HANDLE;
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT DebugUtilsMessenger = VK_NULL_HANDLE;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger;
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger;
#endif
	
	PhysicalDeviceAndProps SelectedPhysDevice;

	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR SelectedSurfaceFormat;
	
	VkDevice Device = VK_NULL_HANDLE;

	QueueAndFamilyIndex GraphicsQueue = QueueAndFamilyIndex({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });
	QueueAndFamilyIndex PresentQueue = QueueAndFamilyIndex({ VK_NULL_HANDLE, (std::numeric_limits<uint32_t>::max)() });

	VkFence Fence = VK_NULL_HANDLE;

	VkSemaphore NextImageAcquiredSemaphore = VK_NULL_HANDLE;
	VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;

	struct Swapchain
	{
		VkSwapchainKHR VkSwapchain = VK_NULL_HANDLE;
		std::vector<ImageAndView> ImageAndViews;
		uint32_t Index = 0;
		VkExtent2D Extent;
	};
	Swapchain Swapchain;

	VkCommandPool CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> CommandBuffers;

	std::vector<BufferAndDeviceMemory> VertexBuffers;
	std::vector<BufferAndDeviceMemory> IndexBuffers;
	std::vector<BufferAndDeviceMemory> IndirectBuffers;

	std::vector<BufferAndDeviceMemory> UniformBuffers;

	std::vector<ImageAndDeviceMemory> Textures;

	std::vector<VkSampler> Samplers;
	std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
	std::vector<VkPipelineLayout> PipelineLayouts;

	std::vector<VkRenderPass> RenderPasses;

	std::vector<VkPipeline> Pipelines;

	std::vector<VkFramebuffer> Framebuffers;

	std::vector<VkDescriptorPool> DescriptorPools;
	std::vector<VkDescriptorSet> DescriptorSets;

	std::vector<VkViewport> Viewports;
	std::vector<VkRect2D> ScissorRects;
};

//#include "LKGVK.h"