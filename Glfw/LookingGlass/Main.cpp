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

class DisplacementDDSGlfwVK : public DisplacementDDSVK, public Glfw
{
private:
	using Super = DisplacementDDSVK;
public:
	DisplacementDDSGlfwVK(GLFWwindow* Win) : Glfw(Win) {}
	DisplacementDDSGlfwVK(GLFWwindow* Win, const std::filesystem::path& Color, const std::filesystem::path& Depth) : Super(Color, Depth), Glfw(Win) {}

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
};

#ifdef USE_CV
class DisplacementCVGlfwVK : public DisplacementCVVK, public Glfw
{
private:
	using Super = DisplacementCVVK;
public:
	DisplacementCVGlfwVK(GLFWwindow* Win) : Glfw(Win) {}
	DisplacementCVGlfwVK(GLFWwindow* Win, const std::filesystem::path& Color, const std::filesystem::path& Depth) : Super(Color, Depth), Glfw(Win) {}

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
};

class DisplacementCVRGBDGlfwVK : public DisplacementCVRGBDVK, public Glfw
{
private:
	using Super = DisplacementCVRGBDVK;
public:
	DisplacementCVRGBDGlfwVK(GLFWwindow* Win) : Glfw(Win) {}
	DisplacementCVRGBDGlfwVK(GLFWwindow* Win, const std::filesystem::path& RGBD) : Super(RGBD), Glfw(Win) {}

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
};
#endif

class AnimatedDisplacementGlfwVK : public AnimatedDisplacementVK, public Glfw
{
private:
	using Super = DisplacementVK;
public:
	AnimatedDisplacementGlfwVK(GLFWwindow* Win) : Glfw(Win) {}

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
};

#ifdef USE_CV
class VideoDisplacementGlfwVK : public VideoDisplacementVK, public Glfw
{
private:
	using Super = VideoDisplacementVK;
public:
	VideoDisplacementGlfwVK(GLFWwindow* Win) : Glfw(Win) {}
	VideoDisplacementGlfwVK(GLFWwindow* Win, const std::filesystem::path& RGBD) : Super(RGBD), Glfw(Win) {}

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
};
#ifdef USE_HAILO
class DepthEstimationDisplacementGlfwVK : public DepthEstimationDisplacementVK, public Glfw
{
private:
	using Super = DepthEstimationDisplacementVK;
public:
	DepthEstimationDisplacementGlfwVK(GLFWwindow* Win) : Glfw(Win) {}
	DepthEstimationDisplacementGlfwVK(GLFWwindow* Win, const std::filesystem::path& Video) : Super(Video), Glfw(Win) {}

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
};
#endif //!< USE_HAILO
#endif //!< USE_CV

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

	DisplacementDDSGlfwVK Vk(GlfwWin
		//, std::filesystem::path("..") / ".." / "Textures" / "Rocks007_2K_Color.dds"
		//, std::filesystem::path("..") / ".." / "Textures" / "Rocks007_2K_Displacement.dds"
	);
#ifdef USE_CV
	//DisplacementCVGlfwVK Vk(GlfwWin
	//	//, std::filesystem::path("..") / ".." / "Textures" / "Bricks091_1K-JPG_Color.jpg"
	//	//, std::filesystem::path("..") / ".." / "Textures" / "Bricks091_1K-JPG_Displacement.jpg"
	//);
	//DisplacementCVRGBDGlfwVK Vk(GlfwWin
	//	//, std::filesystem::path("..") / ".." / "Textures" / "Bricks076C_1K.png"
	//);
#endif
	//AnimatedDisplacementGlfwVK Vk(GlfwWin);
#ifdef USE_CV
	//VideoDisplacementGlfwVK Vk(GlfwWin
	//	//, std::filesystem::path("..") / ".." / "Textures" / "RGBD0.mp4"
	//);
#ifdef USE_HAILO
	//DepthEstimationDisplacementGlfwVK Vk(GlfwWin
	//	//, std::filesystem::path("..") / ".." / "Textures" / "instance_segmentation.mp4"
	//);
#endif
#endif

	Vk.Init();

	Vk.PopulateCommandBuffer();

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
