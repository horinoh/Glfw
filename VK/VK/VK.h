#pragma once

#include <iostream>
#include <vector>
#include <utility>
#include <numeric>
#include <filesystem>
#include <numbers>
#include <span>
#include <thread>
#include <mutex>
#include <source_location>
#include <random>
#include <cstddef>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "GLI.h"

#ifdef _WIN64
#pragma comment(lib, "vulkan-1.lib")
#endif

//#ifndef USE_CV
//#define USE_CV
//#endif
//#ifndef USE_HALIO
//#define USE_HAILO
//#endif

//!< 使用には USE_CV を定義する必要がある
#ifdef USE_CV
#include "CV.h"
#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "opencv_world4100d.lib")
#else
#pragma comment(lib, "opencv_world4100.lib")
#endif
#endif
#endif

//!< 使用には USE_HAILO を定義する必要がある
#ifdef USE_HAILO
#include "Hailo.h"
#ifdef _WIN64
#pragma comment(lib, "libhailort.lib")
#endif
#endif

//!< std::breakpoint() c++26
#ifdef _WIN64
#ifdef _DEBUG
#define BREAKPOINT() __debugbreak()
#else
#define BREAKPOINT()
#endif
#else
#ifdef _DEBUG
#include <signal.h>
#define BREAKPOINT() raise(SIGTRAP)
#else
#define BREAKPOINT()
#endif
#endif

//!< cf)
//!< VK_ERROR_OUT_OF_DATE_KHR = -1000001004
//!< VK_ERROR_LAYER_NOT_PRESENT = -6
//!< VK_ERROR_EXTENSION_NOT_PRESENT = -7,
#ifdef _DEBUG
#define FUNC_NAME() std::source_location::current().function_name()
#define LOG() std::cout << FUNC_NAME() << std::endl
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << FUNC_NAME() << " VkResult = " << VR << " (0x" << std::hex << static_cast<uint32_t>(VR) << std::dec << ")" << std::dec << std::endl; BREAKPOINT(); } }
#else
#define LOG()
#define VERIFY_SUCCEEDED(X) (X) 
#endif

#define SIZE_DATA(X) VK::SizeAndDataPtr({ sizeof(X), std::data(X) })
#define SIZE_DATA_NULL VK::SizeAndDataPtr({ 0, nullptr })

template<typename T> static constexpr size_t TotalSizeOf(const std::vector<T>& rhs) { return sizeof(T) * size(rhs); }
template<typename T, size_t U> static constexpr size_t TotalSizeOf(const std::array<T, U>& rhs) { return sizeof(rhs); }

class VK
{
public:
	using SizeAndDataPtr = std::pair<const size_t, const void*>;
	struct PhysDevProp
	{
		VkPhysicalDeviceProperties PDP;
		VkPhysicalDeviceMemoryProperties PDMP;
	};
	using PhysicalDeviceAndProps = std::pair<VkPhysicalDevice, PhysDevProp>;
	using QueueAndFamilyIndex = std::pair<VkQueue, uint32_t>;
	using BufferAndDeviceMemory = std::pair<VkBuffer, VkDeviceMemory>;
	using ImageAndView = std::pair<VkImage, VkImageView>;
	struct Texture {
		ImageAndView ImageView;
		VkDeviceMemory DeviceMemory;
		std::vector<BufferAndDeviceMemory> Staging;
	};
	using CommandPoolAndBuffers = std::pair<VkCommandPool, std::vector<VkCommandBuffer>>;
	using PathAndPipelineStage = std::pair<std::filesystem::path, VkPipelineStageFlags2>;
#ifdef USE_CV
	struct CvMatAndFormatAndPipelineStage
	{
		cv::Mat Mat;
		VkFormat Format;
		VkPipelineStageFlags2 PipelineStage;
	};
#endif

	virtual ~VK();

	static constexpr size_t RoundUpMask(const size_t Size, const size_t Mask) { return (Size + Mask) & ~Mask; }
	static constexpr size_t RoundUp(const size_t Size, const size_t Align) { return RoundUpMask(Size, Align - 1); }
	static constexpr size_t RoundUp256(const size_t Size) { return RoundUpMask(Size, 0xff); }

	virtual void Init() {
		CreateInstance();
		SelectPhysicalDevice();
		CreateSurface();
		SelectSurfaceFormat();
		CreateDevice();
		CreateFenceAndSemaphore();
		CreateSwapchain();
		CreateCommandBuffer();
		CreateGeometry();
		CreateUniformBuffer();
		CreateTexture();
		CreatePipelineLayout();
		CreateRenderPass();
		CreatePipeline();
		CreateFramebuffer();
		CreateDescriptor();
		CreateViewports();
	}
	virtual void Render() {
		if (ReCreateSwapchain()) {
			WaitFence();
			if (AcquireNextImage()) {
				OnUpdate();
				Submit();
				if (Present()) {
				}
			}
			++FrameCount;
		}
	}

	virtual void CreateInstance() { LOG(); }
	virtual void SelectPhysicalDevice();
	virtual void CreateSurface() { LOG(); }
	virtual void SelectSurfaceFormat();
	virtual void CreateDevice();
	virtual void CreateFenceAndSemaphore();
	virtual bool CreateSwapchain() { LOG(); return true; }
	virtual void DestroySwapchain();
	virtual bool ReCreateSwapchain();
	virtual void CreateCommandBuffer();
	virtual void CreateGeometry() { LOG(); }
	virtual void CreateUniformBuffer() { LOG(); }
	virtual void CreateTexture() { LOG(); }
	virtual void CreatePipelineLayout();
	virtual void CreateRenderPass() { 
		CreateRenderPass_Clear(); 	
		LOG();
	}
	virtual void CreatePipeline() { LOG(); }
	virtual void CreateFramebuffer() {
		Framebuffers.reserve(std::size(Swapchain.ImageAndViews));
		CreateFramebuffer(RenderPasses[0]);
		LOG();
	}
	virtual void CreateDescriptor() { LOG(); }
	void DestroyViewports() { Viewports.clear(); ScissorRects.clear(); }
	virtual void CreateViewports();

	virtual void PopulateCommandBuffer() {
		for (auto i = 0; i < static_cast<int>(std::size(Swapchain.ImageAndViews)); ++i) {
			PopulateSecondaryCommandBuffer(i);
			PopulatePrimaryCommandBuffer(i);
		}
		LOG();
	}

	virtual void WaitFence();
	virtual bool AcquireNextImage();
	virtual void OnUpdate() { if (0 == FrameCount) { LOG(); } }
	virtual void Submit();
	virtual bool Present();

public:
	void CreateInstance(const std::vector<const char*>& Extensions);
	bool CreateSwapchain(const uint32_t Width, const uint32_t Height);

	void CopyToHostVisibleMemory(const VkDeviceMemory DeviceMemory, const VkDeviceSize Offset, const VkDeviceSize Size, const void* Source, const VkDeviceSize MappedRangeOffset = 0, const VkDeviceSize MappedRangeSize = VK_WHOLE_SIZE) const;

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
	void CreateHostVisibleBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const gli::texture& Gli) const { CreateHostVisibleBuffer(&BADM.first, &BADM.second, BUF, Gli.size(), Gli.data()); }
#ifdef USE_CV
	void CreateHostVisibleBuffer(BufferAndDeviceMemory& BADM, const VkBufferUsageFlags BUF, const cv::Mat& CvMat) const { CreateHostVisibleBuffer(&BADM.first, &BADM.second, BUF, CvMat.total() * CvMat.elemSize(), reinterpret_cast<const void*>(CvMat.ptr())); }
#endif

	void CreateImage(VkImage* Image, VkDeviceMemory* DeviceMemory, const VkImageCreateInfo& ICI);
	void CreateImageView(VkImageView* ImageView, const VkImageViewCreateInfo& IVCI) { VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, nullptr, ImageView)); }

	void BufferMemoryBarrier(const VkCommandBuffer CB,
		const VkBuffer Buffer,
		const VkPipelineStageFlags2 SrcPSF, const VkPipelineStageFlags2 DstPSF,
		const VkAccessFlags2 SrcAF, const VkAccessFlags2 DstAF) const;
	void ImageMemoryBarrier(const VkCommandBuffer CB,
		const VkImage Image,
		const VkPipelineStageFlags2 SrcPSF, const VkPipelineStageFlags2 DstPSF,
		const VkAccessFlags2 SrcAF, const VkAccessFlags2 DstAF,
		const VkImageLayout OldIL, const VkImageLayout NewIL,
		const VkImageSubresourceRange& ISR) const;
	void ImageMemoryBarrier(const VkCommandBuffer CB,
		const VkImage Image,
		const VkPipelineStageFlags2 SrcPSF, const VkPipelineStageFlags2 DstPSF,
		const VkAccessFlags2 SrcAF, const VkAccessFlags2 DstAF,
		const VkImageLayout OldIL, const VkImageLayout NewIL) const {
		constexpr auto ISR = VkImageSubresourceRange({
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS
		});
		ImageMemoryBarrier(CB, Image, SrcPSF, DstPSF, SrcAF, DstAF, OldIL, NewIL, ISR);
	}
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const VkBuffer Staging, const VkBuffer Buffer, const size_t Size, 
		const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB, 
		const BufferAndDeviceMemory& Staging, const BufferAndDeviceMemory& Buffer, const size_t Size,
		const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const {
		PopulateCopyCommand(CB,
			Staging.first, Buffer.first, Size, 
			AF, PSF);
	}
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const BufferAndDeviceMemory& Staging, const BufferAndDeviceMemory& Buffer, const SizeAndDataPtr& Size, 
		const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const {
		PopulateCopyCommand(CB, 
			Staging, Buffer, Size.first,
			AF, PSF);
	}

	void PopulateCopyCommand(const VkCommandBuffer CB,
		const VkBuffer Staging, const VkImage Image, const std::span<const VkBufferImageCopy2>& BICs, const VkImageSubresourceRange& ISR, 
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB, 
		const VkBuffer Staging, const VkImage Image, const gli::texture& Gli, 
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const BufferAndDeviceMemory& Staging, const Texture& Image, const gli::texture& Gli, 
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const {
		PopulateCopyCommand(CB, 
			Staging.first, Image.ImageView.first, Gli, 
			IL, AF, PSF);
	}
#ifdef USE_CV
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const VkBuffer Staging, const VkImage Image, const cv::Mat& CvMat,
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const;
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const BufferAndDeviceMemory& Staging, const Texture& Image, const cv::Mat& CvMat,
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const {
		PopulateCopyCommand(CB,
			Staging.first, Image.ImageView.first, CvMat, 
			IL, AF, PSF);
	}
#endif
	void PopulateCopyCommand(const VkCommandBuffer CB,
		const VkBuffer Staging, const VkImage Image, const uint32_t Width, const uint32_t Height,
		const VkImageLayout IL, const VkAccessFlags2 AF, const VkPipelineStageFlagBits2 PSF) const
	{
		const std::array BICs = {
			VkBufferImageCopy2({
				.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
				.pNext = nullptr,
				.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
				.imageSubresource = VkImageSubresourceLayers({.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 }),
				.imageOffset = VkOffset3D({.x = 0, .y = 0, .z = 0 }),
				.imageExtent = VkExtent3D({.width = Width, .height = Height, .depth = 1 }) }),
		};
		constexpr auto ISR = VkImageSubresourceRange({
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS
		});
		PopulateCopyCommand(CB,
			Staging, Image, BICs, ISR, 
			IL, AF, PSF);
	}

	CommandPoolAndBuffers& CreateCommandPool(std::vector<CommandPoolAndBuffers>& CPABs, const VkCommandPoolCreateInfo& CPCI) {
		auto& CPAB = CPABs.emplace_back();
		VERIFY_SUCCEEDED(vkCreateCommandPool(Device, &CPCI, nullptr, &CPAB.first)); 
		return CPAB;
	}
	CommandPoolAndBuffers& CreateCommandPool(std::vector<CommandPoolAndBuffers>& CPAB);

	void AllocateCommandBuffers(VkCommandBuffer* CB, const VkCommandBufferAllocateInfo& CBAI) { VERIFY_SUCCEEDED(vkAllocateCommandBuffers(Device, &CBAI, CB)); }
	void AllocateCommandBuffers(const size_t Count, VkCommandBuffer* CB, const VkCommandPool CP, const VkCommandBufferLevel CBL);
	void AllocateCommandBuffers(CommandPoolAndBuffers& CPAB, const size_t Count, const VkCommandBufferLevel CBL) {
		CPAB.second.resize(Count);
		AllocateCommandBuffers(std::size(CPAB.second), std::data(CPAB.second), CPAB.first, CBL);
	}

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

	Texture& CreateTexture(const VkFormat Format, const uint32_t Width, const uint32_t Height, const VkImageUsageFlags IUF = VK_IMAGE_USAGE_SAMPLED_BIT, const VkImageAspectFlags IAF = VK_IMAGE_ASPECT_COLOR_BIT);
	Texture& CreateTexture_Depth(const VkFormat Format, const uint32_t Width, const uint32_t Height) {
		return CreateTexture(Format, Width, Height, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	}
	Texture& CreateTexture_Render(const VkFormat Format, const uint32_t Width, const uint32_t Height) {
		return CreateTexture(Format, Width, Height, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	}

	[[nodiscard]] static VkFormat ToVkFormat(const gli::format GLIFormat);
	[[nodiscard]] static VkImageType ToVkImageType(const gli::target GLITarget);
	[[nodiscard]] static VkImageViewType ToVkImageViewType(const gli::target GLITarget);
	[[nodiscard]] static VkComponentSwizzle ToVkComponentSwizzle(const gli::swizzle GLISwizzle);
	[[nodiscard]] static VkComponentMapping ToVkComponentMapping(const gli::texture::swizzles_type GLISwizzleType);
	Texture& CreateGLITexture(const std::filesystem::path& Path, gli::texture& Gli);
	void CreateGLITextures(const VkCommandBuffer CB, const std::vector<PathAndPipelineStage>& Paths);
#ifdef USE_CV
	Texture& CreateCVTexture(const cv::Mat& CvMat, const VkFormat Format);
	void CreateCVTextures(const VkCommandBuffer CB, const std::vector<CvMatAndFormatAndPipelineStage>& CvMats);
#endif
	VkShaderModule CreateShaderModule(const std::filesystem::path& Path);

	void CreateRenderPass(const VkRenderPassCreateInfo& RPCI) { VERIFY_SUCCEEDED(vkCreateRenderPass(Device, &RPCI, nullptr, &RenderPasses.emplace_back())); }
	void CreateRenderPass(const std::vector<VkAttachmentDescription>& ADs, const std::vector<VkSubpassDescription>& SDs) {
		const std::array<VkSubpassDependency, 0> Deps;
		const VkRenderPassCreateInfo RPCI = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = static_cast<uint32_t>(std::size(ADs)), .pAttachments = std::data(ADs),
			.subpassCount = static_cast<uint32_t>(std::size(SDs)), .pSubpasses = std::data(SDs),
			.dependencyCount = static_cast<uint32_t>(std::size(Deps)), .pDependencies = std::data(Deps)
		};
		CreateRenderPass(RPCI);
	}
	void CreateRenderPass(const VkAttachmentLoadOp ALO, const VkAttachmentStoreOp ASO,
		const VkImageLayout Init = VK_IMAGE_LAYOUT_UNDEFINED, const VkImageLayout Final = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	void CreateRenderPass_None(const VkImageLayout Init = VK_IMAGE_LAYOUT_UNDEFINED, const VkImageLayout Final = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		CreateRenderPass(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, Init, Final);
	}
	void CreateRenderPass_Clear(const VkImageLayout Init = VK_IMAGE_LAYOUT_UNDEFINED, const VkImageLayout Final = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) { 
		CreateRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, Init, Final); 
	}
	void CreateRenderPass_Depth(const VkImageLayout Init = VK_IMAGE_LAYOUT_UNDEFINED, const VkImageLayout Final = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
	//!< 引数をよく使うものに絞ったもの (Frequently used arguments only)
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TES, const VkShaderModule TCS, const VkShaderModule GS,
		const std::vector<VkVertexInputBindingDescription>& VIBDs, const std::vector<VkVertexInputAttributeDescription>& VIADs,
		const VkPrimitiveTopology PT,
		const uint32_t PatchControlPoints,
		const VkPolygonMode PM, const VkCullModeFlags CMF, const VkFrontFace FF,
		const VkBool32 DepthEnable,
		const VkPipelineLayout PLL,
		const VkRenderPass RP);
	//!< 頂点データによる通常メッシュ描画等 (Draw mesh with vertices)
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
	//!< 全画面ポリゴン描画等 (Fullscreen polygon draw)
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
	//!< テッセレーションによる描画等 (Tesselation draw)
	void CreatePipeline(VkPipeline& PL,
		const VkShaderModule VS, const VkShaderModule FS, const VkShaderModule TES, const VkShaderModule TCS, const VkShaderModule GS,
		const VkPipelineLayout PLL,
		const VkRenderPass RP) {
		CreatePipeline(PL,
			VS, FS, TES, TCS, GS,
			{}, {},
			VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
			1,
			VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_TRUE,
			PLL,
			RP);
	}
	void CreateFramebuffer(const VkRenderPass RP);

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

	virtual void PopulateSecondaryCommandBuffer(const int i);
	virtual void PopulatePrimaryCommandBuffer(const int i);

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

	std::vector<CommandPoolAndBuffers> PrimaryCommandBuffers;
	std::vector<CommandPoolAndBuffers> SecondaryCommandBuffers; //!< VK ではプールをセカンダリ用に分ける必要は無いが、DX に合わせて別にしている

	std::vector<BufferAndDeviceMemory> VertexBuffers;
	std::vector<BufferAndDeviceMemory> IndexBuffers;
	std::vector<BufferAndDeviceMemory> IndirectBuffers;

	std::vector<BufferAndDeviceMemory> UniformBuffers;

	std::vector<Texture> Textures;

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

	uint32_t FrameCount = 0;
};
