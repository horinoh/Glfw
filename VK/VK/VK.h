#pragma once

#include <iostream>
#include <vector>
#include <utility>
#include <numeric>
#include <filesystem>

#include <Vulkan/vulkan.h>
#include <glm/glm.hpp>

#ifdef _WIN64
#pragma comment(lib, "vulkan-1.lib")
#endif

#ifdef _DEBUG
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << VR << std::endl; /*__debugbreak();*/ } }
#else
#define VERIFY_SUCCEEDED(X) (X) 
#endif

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
	void CreateBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlagBits MPFB, const size_t Size, const void* Source = nullptr) const;
	void CreateBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const VkMemoryPropertyFlagBits MPFB, const size_t Size, const void* Source = nullptr) const {
		CreateBuffer(&BADM.first, &BADM.second, BUF, MPFB, Size, Source);
	}
	void CreateDeviceLocalBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size) const {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Size); 
	}
	void CreateDeviceLocalBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const size_t Size) const {
		CreateDeviceLocalBuffer(&BADM.first, &BADM.second, BUF, Size);
	}
	void CreateHostVisibleBuffer(VkBuffer* Buffer, VkDeviceMemory* DeviceMemory, const VkBufferUsageFlags BUF, const size_t Size, const void* Source) const {
		CreateBuffer(Buffer, DeviceMemory, BUF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Size, Source);
	}
	void CreateHostVisibleBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const size_t Size, const void* Source) const {
		CreateHostVisibleBuffer(&BADM.first, &BADM.second, BUF, Size, Source);
	}
	void BufferMemoryBarrier(const VkCommandBuffer CB,
		const VkBuffer Buffer,
		const VkPipelineStageFlags SrcPSF, const VkPipelineStageFlags DstPSF,
		const VkAccessFlags SrcAF, const VkAccessFlags DstAF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB, const VkBuffer Staging, const VkBuffer Buffer, const size_t Size, const VkAccessFlags AF, const VkPipelineStageFlagBits PSF) const;
	virtual void CreateGeometry() {}
	virtual void CreatePipelineLayout();
	virtual void CreateRenderPass();
	VkShaderModule CreateShaderModule(const std::filesystem::path& Path);
	virtual void CreatePipeline() {}
	virtual void CreateFramebuffer();
	virtual void CreateViewports();

	virtual void PopulateCommand();

	virtual void WaitFence();
	virtual void AcquireNextImage();
	virtual void OnUpdate() {}
	virtual void Submit();
	virtual void Present();

public:
	using SizeAndDataPtr = std::pair<const size_t, const void*>;
	void CreateGeometry(const std::vector<SizeAndDataPtr>& Vtxs, 
		const SizeAndDataPtr& Idxs,
		const uint32_t IdxCount, const uint32_t VtxCount, const uint32_t InstCount);
	void CreateGeometry(const SizeAndDataPtr& Vtx, const SizeAndDataPtr& Idx, const uint32_t IdxCount, const uint32_t InstCount) {
		CreateGeometry({ Vtx }, Idx, IdxCount, 0, InstCount);
	}
	void CreateGeometry(const SizeAndDataPtr& Vtx, const uint32_t VtxCount, const uint32_t InstCount) {
		CreateGeometry({ Vtx }, SizeAndDataPtr({ 0, nullptr }), 0, VtxCount, InstCount);
	}

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
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TCS, const VkShaderModule TES, const VkShaderModule GS,
		const std::vector<VkVertexInputBindingDescription>& VIBDs, const std::vector<VkVertexInputAttributeDescription>& VIADs,
		const VkPrimitiveTopology PT,
		const uint32_t PatchControlPoints,
		const VkPolygonMode PM, const VkCullModeFlags CMF, const VkFrontFace FF,
		const VkBool32 DepthEnable,
		const VkPipelineLayout PLL,
		const VkRenderPass RP);
	//!< 頂点データによるメッシュ描画等
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

	using ImageAndView = std::pair<VkImage, VkImageView>;
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

	std::vector<VkPipelineLayout> PipelineLayouts;

	std::vector<VkRenderPass> RenderPasses;

	std::vector<VkPipeline> Pipelines;

	std::vector<VkFramebuffer> Framebuffers;

	std::vector<VkViewport> Viewports;
	std::vector<VkRect2D> ScissorRects;
};
