#pragma once

#include <iostream>
#include <vector>
#include <utility>
#include <numeric>
#include <filesystem>

#include <Vulkan/vulkan.h>
#include <glm/glm.hpp>

#pragma comment(lib, "vulkan-1.lib")

#ifdef _DEBUG
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << VR << std::endl; /*__debugbreak();*/ } }
#else
#define VERIFY_SUCCEEDED(X) (X) 
#endif

class VK 
{
public:
	using PhysicalDeviceAndMemoryProperties = std::pair<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties>;
	using QueueAndFamilyIndex = std::pair<VkQueue, uint32_t>;
	using BufferAndDeviceMemory = std::pair<VkBuffer, VkDeviceMemory>;

	virtual ~VK();

	virtual void CreateInstance(std::vector<const char*>& Extensions);
	virtual void SelectPhysicalDevice();
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

//protected:
	VkInstance Instance = VK_NULL_HANDLE;
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT DebugUtilsMessenger = VK_NULL_HANDLE;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger;
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger;
#endif
	
	PhysicalDeviceAndMemoryProperties SelectedPhysDevice;
	
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