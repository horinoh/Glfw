#include <stdlib.h>
#include <iostream>
#include <vector>
#include <array>

#include <Vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "vulkan-1.lib")

//#define USE_BORDERLESS
//#define USE_FULLSCREEN

#ifdef _DEBUG
#define VERIFY_SUCCEEDED(X) { const auto VR = (X); if(VK_SUCCESS != VR) { std::cerr << VR << std::endl; /*__debugbreak();*/ } }
#else
#define VERIFY_SUCCEEDED(X) (X) 
#endif

//!< �R�[���o�b�N
static void ErrorCallback(int Code, const char* Description)
{
	std::cerr << "Error code = " << Code << ", " << Description << std::endl;
}
static void KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods)
{
	if (Key == GLFW_KEY_ESCAPE && Action == GLFW_PRESS) {
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}
}

int main()
{
	//!< ������
	if (!glfwInit()) {
		std::cerr << "glfwInit() failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	//!< Vulkan �T�|�[�g�̗L��
	if (GLFW_FALSE == glfwVulkanSupported()) {
		std::cerr << "Vulkan not supported" << std::endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//!< OpenGL �R���e�L�X�g���쐬���Ȃ�
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef USE_BORDERLESS
	//!< �{�[�_�[���X�ɂ���ꍇ
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif

	//!< ���j�^�[��
	int MonitorCount;
	const auto Monitors = glfwGetMonitors(&MonitorCount);
	for (auto i = 0; i < MonitorCount; ++i) {
		const auto Mon = Monitors[i];
		std::cout << "[" << i << "] " << glfwGetMonitorName(Mon) << std::endl;
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

	//!< �E�C���h�E�쐬
	//constexpr auto Width = 1280, Height = 720;
	constexpr auto Width = 1920, Height = 1080;
#ifdef USE_FULLSCREEN
	//!< �t���X�N���[���ɂ���ꍇ
	const auto Window = glfwCreateWindow(Width, Height, "Title", glfwGetPrimaryMonitor(), nullptr);
#else
	const auto Window = glfwCreateWindow(Width, Height, "Title", nullptr, nullptr);
#endif

	//!< �t���[���o�b�t�@�T�C�Y (�{�[�_�[���X�ɂ���ƃE�C���h�E�T�C�Y�Ɠ����ɂȂ�͂�)
	int FBWidth, GBHeight;
	glfwGetFramebufferSize(Window, &FBWidth, &GBHeight);
	std::cout << "Framebuffer = " << FBWidth << "x" << GBHeight << std::endl;

	//!< �R�[���o�b�N�o�^
	glfwSetErrorCallback(ErrorCallback);
	glfwSetKeyCallback(Window, KeyCallback);

	//!< �C���X�^���X
	VkInstance Instance = VK_NULL_HANDLE;
	{
		constexpr VkApplicationInfo AI = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = "Glfw App Name", .applicationVersion = VK_HEADER_VERSION_COMPLETE,
			.pEngineName = "Glfw Engine Name", .engineVersion = VK_HEADER_VERSION_COMPLETE,
			.apiVersion = VK_HEADER_VERSION_COMPLETE
		};
		const std::array Layers = {
			"VK_LAYER_KHRONOS_validation",
		};
		std::vector<const char*> Extensions = {
	#ifdef _DEBUG
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
	#endif
		};
		//!< �G�N�X�e���V���� (glfw ���v���b�g�t�H�[�����̈Ⴂ���z�����Ă����)
		uint32_t ExtCount = 0;
		const auto Exts = glfwGetRequiredInstanceExtensions(&ExtCount);
		for (uint32_t i = 0; i < ExtCount; ++i) {
			std::cout << "Add Extension : " << Exts[i] << std::endl;
			Extensions.emplace_back(Exts[i]);
		}
		const VkInstanceCreateInfo ICI = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = &AI,
			.enabledLayerCount = static_cast<uint32_t>(std::size(Layers)), .ppEnabledLayerNames = std::data(Layers),
			.enabledExtensionCount = static_cast<uint32_t>(std::size(Extensions)), .ppEnabledExtensionNames = std::data(Extensions)
		};
		VERIFY_SUCCEEDED(vkCreateInstance(&ICI, nullptr, &Instance));
	}

	//!< �T�[�t�F�X (glfw ���v���b�g�t�H�[�����̈Ⴂ���z�����Ă����)
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	VERIFY_SUCCEEDED(glfwCreateWindowSurface(Instance, Window, nullptr, &Surface));

	//!< ���[�v
	while (!glfwWindowShouldClose(Window)) {
		glfwPollEvents();
	}

	//!< ��Еt��
	if (VK_NULL_HANDLE != Instance) {
		if (VK_NULL_HANDLE != Surface) {
			vkDestroySurfaceKHR(Instance, Surface, nullptr);
		}
		vkDestroyInstance(Instance, nullptr);
	}
	glfwDestroyWindow(Window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
