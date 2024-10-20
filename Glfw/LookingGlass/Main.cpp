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

//#define USE_BORDERLESS	//!< �{�[�_�[���X
#define USE_EXTFULLSCREEN //!< �g�����j�^�A�t���X�N���[��

//!< �R�[���o�b�N (Callbacks)
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
		//!< glfwGetRequiredInstanceExtensions ���v���b�g�t�H�[�����̈Ⴂ���z�����Ă����
		const auto Ptr = glfwGetRequiredInstanceExtensions(&Count);
		//!< �u�|�C���^ + ���v�� span �Ŏ󂯂�
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
	//!< ������ (Initialize)
	if (!glfwInit()) {
		std::cerr << "glfwInit() failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	//!< Vulkan �T�|�[�g�̗L�� (Vulkan supported)
	if (GLFW_FALSE == glfwVulkanSupported()) {
		std::cerr << "Vulkan not supported" << std::endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//!< ���j�^�[�ƃr�f�I���[�h��� (Enumerate monitors and video modes)
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
	
	//!< OpenGL �R���e�L�X�g���쐬���Ȃ� (Not create OpenGL context)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef USE_BORDERLESS
	//!< �{�[�_�[���X�ɂ���ꍇ (Borderless)
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif

	auto Width = 800, Height = 600;
	//!< ���j�^��I��
	GLFWmonitor* Mon = nullptr;
	if (!std::empty(Monitors)) {
#ifdef USE_EXTFULLSCREEN
		//!< �g�����j�^ (�Ō�̗v�f) ��I�� (Select ext monitor)
		Mon = Monitors.back();
#else
		//!< �v���C�}�����j�^ (�f�t�H���g) ��I�� (Select primary monitor (default))
		Mon = *std::ranges::find_if(Monitors, [&](const auto& i) { return i == glfwGetPrimaryMonitor(); });
#endif
		//!< �r�f�I���[�h��I��
		int VCount;
		auto VPtr = glfwGetVideoModes(Mon, &VCount);
		const auto Vids = std::span(VPtr, VCount);
		if (!std::empty(Vids)) {
			//!< �𑜓x�̍����r�f�I���[�h�͍Ō�ɗ񋓂����悤�Ȃ̂ŁA�Ō�̗v�f��I�� (Most high resolution vide mode will be in last element)
			const auto Vid = Vids.back();
			Width = Vid.width;
			Height = Vid.height;
		}
	}

	//!< �E�C���h�E�쐬 (Create window)
	//!< �����I�Ƀ��j�^���w�肵���ꍇ�t���X�N���[���ɂȂ� (Explicitly select monitor to be fullscreen)
	//!< �t���X�N���[���ɂ��Ȃ��ꍇ�� nullptr ���w�肷�邱�� (Select nullptr to be windowed)
	const auto GlfwWin = glfwCreateWindow(Width, Height, "Title", Mon, nullptr);
	//!< �R�[���o�b�N�o�^ (Register callbacks) �E�C���h�E�쐬����ɂ���Ă���
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

	//!< ���[�v (Loop)
	while (!glfwWindowShouldClose(GlfwWin)) {
		glfwPollEvents();

		Vk.Render();
	}

	//!< GLFW ��Еt�� (Terminate)
	glfwDestroyWindow(GlfwWin);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
