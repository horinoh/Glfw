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

//#define USE_BORDERLESS	//!< ボーダーレス
//#define USE_EXTFULLSCREEN //!< 拡張モニタ、フルスクリーン

#define USE_INDEX
#define USE_SECONDARY_CB

//!< コールバック (Callbacks)
static void GlfwErrorCallback(int Code, const char* Description)
{
	std::cerr << "[Glfw] ErrorCallback Code = " << Code << ", Description = " << Description << std::endl;
}
static void GlfwKeyCallback(GLFWwindow* Window, int Key, [[maybe_unused]] int Scancode, int Action, [[maybe_unused]] int Mods)
{
	std::cerr << "[Glfw] KeyCallback Key = " << Key << ", Scancode = " << Scancode << ", Action = " << Action << std::endl;
	if (Key == GLFW_KEY_ESCAPE && Action == GLFW_PRESS) {
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}
}
static void GlfwWindowSizeCallback([[maybe_unused]] GLFWwindow* Window, int Width, int Height)
{
	std::cerr << "[Glfw] SizeCallback Width = " << Width << ", Height = " << Height << std::endl;
}

class Glfw 
{
public:
	Glfw(GLFWwindow* Win) : GlfwWindow(Win) {
		uint32_t Count = 0;
		//!< glfwGetRequiredInstanceExtensions がプラットフォーム毎の違いを吸収してくれる
		const auto Ptr = glfwGetRequiredInstanceExtensions(&Count);
		//!< 「ポインタ + 個数」を span で受ける
		const auto Span = std::span(Ptr, Count);
		InstanceExtensions.assign(std::begin(Span), std::end(Span));
		for (auto& i : InstanceExtensions) {
			std::cout << "Add Extension : " << i << std::endl;
		}

		glfwGetFramebufferSize(GlfwWindow, &FBWidth, &FBHeight);
	}
protected:
	GLFWwindow* GlfwWindow = nullptr;

	std::vector<const char*> InstanceExtensions;
	int FBWidth, FBHeight;
};

class ClearGlfwVK : public VK, public Glfw
{
private:
	using Super = VK;
public:
	ClearGlfwVK(GLFWwindow* Win) : Glfw(Win) {}

	virtual void CreateInstance() override {
		Super::CreateInstance(InstanceExtensions);
		LOG();
	}
	virtual void CreateSurface() override {
		VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, GlfwWindow, nullptr, &Surface));
		LOG();
	}
	virtual bool CreateSwapchain() override {
		if (Super::CreateSwapchain(static_cast<uint32_t>(FBWidth), static_cast<uint32_t>(FBHeight))) {
			LOG();
			return true;
		}
		return false; 
	}
	virtual void PopulatePrimaryCommandBuffer(const int i) override {
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
			vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
				vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
				vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));
			} vkCmdEndRenderPass(CB);
		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		LOG();
	}
};

class TriangleGlfwVK : public VK, public Glfw
{
private:
	using Super = VK;
public:
	TriangleGlfwVK(GLFWwindow* Win) : Glfw(Win) {}

	virtual void CreateInstance() override {
		Super::CreateInstance(InstanceExtensions);
		LOG();
	}
	virtual void CreateSurface() override {
		VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, GlfwWindow, nullptr, &Surface));
		LOG();
	}
	virtual bool CreateSwapchain() override {
		if (Super::CreateSwapchain(static_cast<uint32_t>(FBWidth), static_cast<uint32_t>(FBHeight))) {
			LOG();
			return true;
		}
		return false;
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
			static_cast<uint32_t>(std::size(Indices)), 1);
#else
		VK::CreateGeometry(SIZE_DATA(Vertices), std::size(Vertices), 1);
#endif
		LOG();
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
		LOG();
	}
#ifdef USE_SECONDARY_CB
	virtual void PopulateSecondaryCommandBuffer(const int i) override {
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

			const auto VB = VertexBuffers[0].first;
			const auto IDB = IndirectBuffers[0].first;

			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

			vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
			vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

			const std::array VBs = { VB };
			const std::array Offsets = { VkDeviceSize(0) };
			vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), std::data(Offsets));
#ifdef USE_INDEX
			const auto IB = IndexBuffers[0].first;
			vkCmdBindIndexBuffer(CB, IB, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexedIndirect(CB, IDB, 0, 1, 0);
#else
			vkCmdDrawIndirect(CB, IDB, 0, 1, 0);
#endif
		}VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
	}
#endif
	virtual void PopulatePrimaryCommandBuffer(const int i) override {
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
#ifdef USE_SECONDARY_CB
			vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); {
				const auto SCB = SecondaryCommandBuffers[0].second[i];
				const std::array SCBs = { SCB };
				vkCmdExecuteCommands(CB, static_cast<uint32_t>(std::size(SCBs)), std::data(SCBs));
			} vkCmdEndRenderPass(CB);
#else
			vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
				const auto PL = Pipelines[0];

				vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PL);

				vkCmdSetViewport(CB, 0, static_cast<uint32_t>(std::size(Viewports)), std::data(Viewports));
				vkCmdSetScissor(CB, 0, static_cast<uint32_t>(std::size(ScissorRects)), std::data(ScissorRects));

				const auto VB = VertexBuffers[0].first;
				const auto IDB = IndirectBuffers[0].first;

				const std::array VBs = { VB };
				const std::array Offsets = { VkDeviceSize(0) };
				vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(std::size(VBs)), std::data(VBs), std::data(Offsets));
#ifdef USE_INDEX
				const auto IB = IndexBuffers[0].first;
				vkCmdBindIndexBuffer(CB, IB, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexedIndirect(CB, IDB, 0, 1, 0);
#else
				vkCmdDrawIndirect(CB, IDB, 0, 1, 0);
#endif
			} vkCmdEndRenderPass(CB);
#endif
		} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
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

	//!< モニターとビデオモードを列挙 (Enumerate monitors and video modes)
	int MCount;
	const auto MPtr = glfwGetMonitors(&MCount);
	const auto Monitors = std::span(MPtr, MCount);
	for (int MIndex = 0; auto & i : Monitors) {
		std::cout << ((glfwGetPrimaryMonitor() == i) ? "Pri" : "---") << "[" << MIndex++ << "] " << glfwGetMonitorName(i) << std::endl;

		int VCount;
		auto VPtr = glfwGetVideoModes(i, &VCount);
		const auto VideoModes = std::span(VPtr, VCount);
		for (int VIndex = 0; auto & j : VideoModes) {
			std::cout << "\t[" << VIndex++ << "] " << j.width << "x" << j.height << " @" << j.refreshRate << std::endl;
		}
	}

	//!< OpenGL コンテキストを作成しない (Not create OpenGL context)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef USE_BORDERLESS
	//!< ボーダーレスにする場合 (Borderless)
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif

	auto Width = 800, Height = 600;
	//!< モニタを選択
	GLFWmonitor* Mon = nullptr;
	if (!std::empty(Monitors)) {
#ifdef USE_EXTFULLSCREEN
		//!< 拡張モニタ (最後の要素) を選択 (Select ext monitor)
		Mon = Monitors.back();
#else
		//!< プライマリモニタ (デフォルト) を選択 (Select primary monitor (default))
		Mon = *std::ranges::find_if(Monitors, [&](const auto& i) { return i == glfwGetPrimaryMonitor(); });
#endif
		//!< ビデオモードを選択
		int VCount;
		auto VPtr = glfwGetVideoModes(Mon, &VCount);
		const auto Vids = std::span(VPtr, VCount);
		if (!std::empty(Vids)) {
			//!< 解像度の高いビデオモードは最後に列挙されるようなので、最後の要素を選択 (Most high resolution vide mode will be in last element)
			const auto Vid = Vids.back();
			Width = Vid.width;
			Height = Vid.height;
		}
	}

	//!< ウインドウ作成 (Create window)
	//!< 明示的にモニタを指定した場合フルスクリーンになる (Explicitly select monitor to be fullscreen)
	//!< フルスクリーンにしない場合は nullptr を指定すること (Select nullptr to be windowed)
	const auto GlfwWin = glfwCreateWindow(Width, Height, "Title", Mon, nullptr);
	//!< コールバック登録 (Register callbacks) ウインドウ作成直後にやっておく
	glfwSetErrorCallback(GlfwErrorCallback);
	glfwSetKeyCallback(GlfwWin, GlfwKeyCallback);
	glfwSetWindowSizeCallback(GlfwWin, GlfwWindowSizeCallback);
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

	//ClearGlfwVK Vk(GlfwWin);
	TriangleGlfwVK Vk(GlfwWin);
	Vk.Init();

	Vk.PopulateCommandBuffer();
	Vk.OnUpdate();

	//!< ループ (Loop)
	while (!glfwWindowShouldClose(GlfwWin)) {
		glfwPollEvents();

		Vk.Render();
	}

	//!< GLFW 後片付け (Terminate)
	glfwDestroyWindow(GlfwWin);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
