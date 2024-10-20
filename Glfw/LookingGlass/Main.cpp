#include <stdlib.h>
#include <fstream>
#include <array>
#include <algorithm>
#include <bitset>
#include <map>

#include "VK.h"
#include <GLFW/glfw3.h>

#include "LookingGlassVK.h"

#ifdef _WIN64
#pragma comment(lib, "VK.lib")
#pragma comment(lib, "glfw3dll.lib")
#endif

//#define USE_BORDERLESS	//!< ボーダーレス
#define USE_EXTFULLSCREEN //!< 拡張モニタ、フルスクリーン

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

class DisplacementGlfwVK : public DisplacementVK, public Glfw
{
private:
	using Super = DisplacementVK;
public:
	DisplacementGlfwVK(GLFWwindow* Win) : Glfw(Win) {}

	virtual void CreateInstance() override {
		Super::CreateInstance(InstanceExtensions);
		LOG();
	}
	virtual void CreateSurface() override {
		VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, GlfwWindow, nullptr, &Surface));
		LOG();
	}
	virtual void CreateSwapchain() override {
		Super::CreateSwapchain(static_cast<const uint32_t>(FBWidth), static_cast<const uint32_t>(FBHeight));
		LOG();
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
		for (int VIndex = 0; auto& j : VideoModes) {
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

	DisplacementGlfwVK Vk(GlfwWin);
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
