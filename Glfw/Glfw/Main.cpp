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

#define USE_INDEX

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
	TriangleVK(GLFWwindow* Win) : GlfwWindow(Win) {}

	virtual void CreateSurface() override {
		VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, GlfwWindow, nullptr, &Surface));
	}
	virtual void CreateGeometry() override {
		using PositonColor = std::pair<glm::vec3, glm::vec3>;
		const std::array Vertices = {
			PositonColor({ 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
			PositonColor({ -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
			PositonColor({ 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
		};
#ifdef USE_INDEX
		constexpr std::array Indices = {
			uint32_t(0), uint32_t(1), uint32_t(2) 
		};
		VK::CreateGeometry(SIZE_DATA(Vertices),
			SIZE_DATA(Indices),
			std::size(Indices), 1);
#else
		VK::CreateGeometry(SIZE_DATA(Vertices)),
			std::size(Vertices), 1);
#endif
	}
	virtual void CreatePipeline() override {
		Pipelines.emplace_back();
		
		const std::array SMs = {
			CreateShaderModule(std::filesystem::path(".") / "Glfw.vert.spv"),
			CreateShaderModule(std::filesystem::path(".") / "Glfw.frag.spv"),
		};

		std::vector<std::thread> Threads = {};

		const std::vector VIBDs = {
			VkVertexInputBindingDescription({.binding = 0, .stride = sizeof(glm::vec3) + sizeof(glm::vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }),
		};
		const std::vector VIADs = {
			VkVertexInputAttributeDescription({.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 }),
			VkVertexInputAttributeDescription({.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(glm::vec3) }),
		};
		Threads.emplace_back(std::thread([&] {
			VK::CreatePipeline(Pipelines[0],
			SMs[0], SMs[1],
			VIBDs, VIADs,
			VK_FALSE,
			PipelineLayouts[0], 
			RenderPasses[0]);
		}));
		
		for (auto& i : Threads) { i.join(); }

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
#ifdef USE_INDEX
					vkCmdBindIndexBuffer(CB, IndexBuffers[0].first, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexedIndirect(CB, IndirectBuffers[0].first, 0, 1, 0);
#else
					vkCmdDrawIndirect(CB, IndirectBuffers[0].first, 0, 1, 0);
#endif
				} vkCmdEndRenderPass(CB);
			} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		}
	}

protected:
	GLFWwindow* GlfwWindow = nullptr;
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
	const auto GlfwWin = glfwCreateWindow(Width, Height, "Title", glfwGetPrimaryMonitor(), nullptr);
#else
	const auto GlfwWin = glfwCreateWindow(Width, Height, "Title", nullptr, nullptr);
	{
		int WinLeft, WinTop, WinRight, WinBottom;
		glfwGetWindowFrameSize(GlfwWin, &WinLeft, &WinTop, &WinRight, &WinBottom);
		std::cout << "Window frame size (LTRB) = " << WinLeft << ", " << WinTop << ", " << WinRight << " ," << WinBottom << std::endl;

		int WinX, WinY;
		glfwGetWindowPos(GlfwWin, &WinX, &WinY);		
		std::cout << "Window pos = " << WinX << ", " << WinY << std::endl;
		int WinWidth, WinHeight;
		glfwGetWindowSize(GlfwWin, &WinWidth, &WinHeight);
		std::cout << "Window size = " << WinWidth << "x" << WinHeight << std::endl;

		int FBWidth, GBHeight;
		glfwGetFramebufferSize(GlfwWin, &FBWidth, &GBHeight);
		std::cout << "Framebuffer size = " << FBWidth << "x" << GBHeight << std::endl;
	}
#endif

	//!< コールバック登録 (Register callbacks)
	glfwSetErrorCallback(GlfwErrorCallback);
	glfwSetKeyCallback(GlfwWin, GlfwKeyCallback);

	TriangleVK TriVK(GlfwWin);

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
	TriVK.CreateSurface();
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
		glfwGetFramebufferSize(GlfwWin, &FBWidth, &FBHeight);
		TriVK.CreateSwapchain(static_cast<const uint32_t>(FBWidth), static_cast<const uint32_t>(FBHeight));
	}

	//!< コマンドバッファ (Command buffer)
	TriVK.CreateCommandBuffer();

	//!< ジオメトリ (Geometory)
	TriVK.CreateGeometry();

	//!< ユニフォームバッファ
	TriVK.CreateUniformBuffer();

	//!< テクスチャ
	TriVK.CreateTexture();

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
	while (!glfwWindowShouldClose(GlfwWin)) {
		glfwPollEvents();

		TriVK.WaitFence();
		TriVK.AcquireNextImage();
		TriVK.OnUpdate();
		TriVK.Submit();
		TriVK.Present();
	}

	//!< GLFW 後片付け (Terminate)
	glfwDestroyWindow(GlfwWin);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
