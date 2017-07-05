#define GLFW_INCLUDE_VULKAN
//#define _NO_FRAME_CAP
#include <GLFW/glfw3.h>

#include <set>
#include <vector>
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <functional>

#include "VulkanTest.h"

using namespace std;

const int WIDTH = 800;
const int HEIGHT = 600;
const bool enableValidationLayers = ENABLE_VALIDATION_LAYER;
const bool FrameCapEnable = FRAME_CAP_ENABLE;

const vector<const char *> validationLayers =
{
	"VK_LAYER_LUNARG_standard_validation"
};

const vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static vector<char>		readFile(const string filename)
{
	ifstream			file;
	size_t				fileSize;
	vector<char>		buffer;

	file = ifstream(filename, ios::ate | ios::binary);
	if (!file.is_open())
		throw runtime_error("Failed to open file!");

	fileSize = (size_t)file.tellg();
	buffer.resize(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return (buffer);
}

VkResult	CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != NULL)
		return (func(instance, pCreateInfo, pAllocator, pCallback));
	return (VK_ERROR_EXTENSION_NOT_PRESENT);
}

void		DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
		func(instance, callback, pAllocator);
}

class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	//window (GLFW) variables
	GLFWwindow					*window;
	
	//Vulkan features
	VkInstance					instance;
	VkDebugReportCallbackEXT	callback;

	//Vulkan devices
	VkDevice					device;
	VkPhysicalDevice			physicalDevice = VK_NULL_HANDLE;

	//Vulkan queues
	VkQueue						graphicsQueue;
	VkQueue						presentQueue;

	//Vulkan surface
	VkSurfaceKHR				surface;

	//Vulkan swapchain
	VkSwapchainKHR				swapChain;
	vector<VkImage>				swapChainImages;
	VkFormat					swapChainImageFormat;
	VkExtent2D					swapChainExtent;
	vector<VkImageView>			swapChainImageViews;
	vector<VkFramebuffer>		swapChainFramebuffers;

	//Vulkan graphics pipeline
	VkPipeline					graphicsPipeline;
	VkRenderPass				renderPass;
	VkPipelineLayout			pipelineLayout;

	//Vulkan commands buffering
	VkCommandPool				commandPool;
	vector<VkCommandBuffer>		commandBuffers;

	//Vulkan semaphores
	VkSemaphore					imageAvailableSemaphore;
	VkSemaphore					renderFinishedSemaphore;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userDta)
	{
		cout << "Validation layer: " << msg << endl;

		return (VK_FALSE);
	}

	void	setupDebugCallback()
	{
		VkDebugReportCallbackCreateInfoEXT	createInfo = {};

		if (!enableValidationLayers)
			return;

		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;// | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		/*
		** make the debug verbose
		*/
		//createInfo.flags |= (VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT);

		/*
		** ce pointeur est envoyé dans le void *userDta du callback et permet
		** de donner acces à nos propres données depuis l'intérieur de la fonction de callback.
		*/
		createInfo.pUserData = NULL;

		if (CreateDebugReportCallbackEXT(instance, &createInfo, NULL, &callback) != VK_SUCCESS)
			throw runtime_error("Failed to set up the callback!");
	}

	static void		onWindowResized(GLFWwindow *window, int width, int height)
	{
		HelloTriangleApplication	*app;

		if (width == 0 || height == 0)
			return;
		app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
	}

	void	initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Test", NULL, NULL);
		glfwSetWindowSizeLimits(window, 400, 300, 7680, 4320);
		glfwSetWindowUserPointer(window, this);
		glfwSetWindowSizeCallback(window, HelloTriangleApplication::onWindowResized);
	}

	bool	checkValidationLayerSuport()
	{
		bool						layerFound;
		uint32_t					layerCount;
		vector<VkLayerProperties>	availableLayers;

		vkEnumerateInstanceLayerProperties(&layerCount, NULL);
		availableLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char *layerName : validationLayers)
		{
			layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (!layerFound)
				return (false);
		}
		return (true);
	}

	vector<const char *>	getRequiredExtensions()
	{
		vector<const char *>	extensions;
		const char					**glfwExtensions;
		unsigned int				glfwExtensionCount;

		glfwExtensionCount = 0;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		for (unsigned int i = 0; i < glfwExtensionCount; i++)
			extensions.push_back(glfwExtensions[i]);
		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		return (extensions);
	}

	void	createInstance()
	{
		unsigned int			glfwExtensionCount = 0;
		const char				**glfwExtensions;
		VkInstanceCreateInfo	createInfo = {};
		VkApplicationInfo		appInfo = {};
		VkResult				result;

		if (enableValidationLayers && !checkValidationLayerSuport())
			throw runtime_error("Validation layers requested, but not available!");

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}		

		vector<const char *> extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if ((result = vkCreateInstance(&createInfo, NULL, &instance)) != VK_SUCCESS)
			throw runtime_error("Failed to create vulkan instance!");
	}

	QueueFamilyIndices		findQueueFamilies(VkPhysicalDevice device)
	{
		int										i;
		uint32_t								queueFamilyCount;
		VkBool32								presentSuport;
		QueueFamilyIndices						indices;
		vector<VkQueueFamilyProperties>			queueFamilies;

		i = 0;
		queueFamilyCount = 0;
		presentSuport = false;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
		queueFamilies.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (const auto& queueFamily : queueFamilies)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSuport);
			if (queueFamily.queueCount > 0)
			{
				if (queueFamily.queueFlags && VK_QUEUE_GRAPHICS_BIT)
					indices.graphicsFamily = i;
				if (presentSuport)
					indices.presentFamily = i;
			}
			if (indices.isComplete())
				break;
			i++;
		}

		return (indices);
	}

	SwapChainSupportDetails		querySwapChainSupport(VkPhysicalDevice device)
	{
		uint32_t				formatCount;
		uint32_t				presentModeCount;
		SwapChainSupportDetails	details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());


		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return (details);
	}

	VkPresentModeKHR		chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
	{
		VkPresentModeKHR	bestMode;

		/*
		cout << "Imediat    : " << VK_PRESENT_MODE_IMMEDIATE_KHR  << endl;
		cout << "Mailbox    : " << VK_PRESENT_MODE_MAILBOX_KHR << endl;
		cout << "fifo       : " << VK_PRESENT_MODE_FIFO_KHR << endl;
		cout << "fifo relax : " << VK_PRESENT_MODE_FIFO_RELAXED_KHR << endl;
		*/

		bestMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const VkPresentModeKHR& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
				continue;
			if (bestMode == VK_PRESENT_MODE_MAILBOX_KHR)
				break;
			if (bestMode > availablePresentMode || (bestMode < availablePresentMode && availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR))
				bestMode = availablePresentMode;
		}
		if (!FrameCapEnable && bestMode != VK_PRESENT_MODE_MAILBOX_KHR)
			return (VK_PRESENT_MODE_IMMEDIATE_KHR);
		/*
		for (const VkPresentModeKHR& availablePresentMode : availablePresentModes)
		{
			if (enableValidationLayers)
			{
				if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
					return (VK_PRESENT_MODE_IMMEDIATE_KHR);
			}
			else if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return (availablePresentMode);
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				bestMode = availablePresentMode;
		}
		*/
		return (bestMode);
	}

	VkExtent2D				chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		int			width;
		int			height;
		VkExtent2D	actualExtent;

		glfwGetWindowSize(window, &width, &height);
		actualExtent = { (uint32_t)width, (uint32_t)height };
		if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max())
			return (capabilities.currentExtent);
		actualExtent.width = std::max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));
		return (actualExtent);
	}

	VkSurfaceFormatKHR		chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats)
	{
		VkSurfaceFormatKHR	ret;		

		ret = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
			return (ret);
		for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return (availableFormat);
		}
		return (availableFormats[0]);
	}

	bool	checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t						extensionCount;
		vector<VkExtensionProperties>	availableExtensions;
		set<string>						requiredExtensions;

		vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
		availableExtensions.resize(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data());
		requiredExtensions = set<string>(deviceExtensions.begin(), deviceExtensions.end());

		for (const VkExtensionProperties& extension : availableExtensions)
			requiredExtensions.erase(extension.extensionName);
		return (requiredExtensions.empty());
	}

	bool	isDeviceSuitable(VkPhysicalDevice device)
	{
		//VkPhysicalDeviceProperties	deviceProperties;
		//VkPhysicalDeviceFeatures	deviceFeatures;
		SwapChainSupportDetails		swapChainSupport;
		QueueFamilyIndices			indices;
		bool						extensionsSupported;
		bool						swapChainAdequate;

		swapChainAdequate = false;
		extensionsSupported = checkDeviceExtensionSupport(device);
		//vkGetPhysicalDeviceProperties(device, &deviceProperties);
		//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		indices = findQueueFamilies(device);

		if (!extensionsSupported)
			return (false);
		swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty());

		return (indices.isComplete() && swapChainAdequate);
		//cout << "\t" << deviceProperties.deviceName << endl;
	}

	void	pickPhysicalDevice()
	{
		uint32_t					deviceCount;
		vector<VkPhysicalDevice>	devices;

		deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
			throw runtime_error("Failed to find GPU(s) with vulkan suport!");
		devices.resize(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		//cout << "Devices list:" << endl;
		for (const VkPhysicalDevice& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
			throw runtime_error("failed to find a suitable GPU!");
	}

	void	createLogicalDevice()
	{
		QueueFamilyIndices				indices;
		float							queuePriority;
		VkDeviceCreateInfo				createInfo = {};
		VkPhysicalDeviceFeatures		deviceFeatures = {};
		VkDeviceQueueCreateInfo			queueCreateInfo;
		vector<VkDeviceQueueCreateInfo>	queueCreateInfos;
		set<int>						uniqueQueueFamilies;

		queuePriority = 1.0f;
		indices = findQueueFamilies(physicalDevice);
		uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

		for (int queueFamily : uniqueQueueFamilies)
		{
			queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		createInfo.enabledLayerCount = 0;
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		
		if (vkCreateDevice(physicalDevice, &createInfo, NULL, &device) != VK_SUCCESS)
			throw runtime_error("Failed to create logical evice!");

		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
	}

	void	createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
			throw runtime_error("Failed to create window surface!");
	}

	void	createSwapChain()
	{
		VkSwapchainCreateInfoKHR	createInfo = {};
		SwapChainSupportDetails		swapChainSupport;
		VkSurfaceFormatKHR			surfaceFormat;
		QueueFamilyIndices			indices;
		VkPresentModeKHR			presentMode;
		VkExtent2D					extent;
		uint32_t					queueFamilyIndices[2];
		uint32_t					imageCount;

		swapChainSupport = querySwapChainSupport(physicalDevice);
		surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		extent = chooseSwapExtent(swapChainSupport.capabilities);

		imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;

		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		indices = findQueueFamilies(physicalDevice);
		queueFamilyIndices[0] = (uint32_t)indices.graphicsFamily;
		queueFamilyIndices[1] = (uint32_t)indices.presentFamily;


		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = NULL;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // If VK_TRUE, ignore the color of pixels that are obstructed by something else. (for instance, if an other window is in front of it). Don't let like this fi i want to read pixels even in this situation
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, NULL, &swapChain) != VK_SUCCESS)
			throw runtime_error("Failed to create swap chain!");

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	void	createImageViews()
	{
		VkImageViewCreateInfo	createInfo = {};
		swapChainImageViews.resize(swapChainImages.size());

		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			createInfo.image = swapChainImages[i];
			if (vkCreateImageView(device, &createInfo, NULL, &swapChainImageViews[i]) != VK_SUCCESS)
				throw runtime_error("Failed to create image views!");
		}
	}

	VkShaderModule		createShaderModule(const vector<char> &code)
	{
		VkShaderModuleCreateInfo	createInfo = {};
		VkShaderModule				shaderModule;

		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
		if (vkCreateShaderModule(device, &createInfo, NULL, &shaderModule) != VK_SUCCESS)
			throw runtime_error("Failed to create shader module!");
		return (shaderModule);
	}

	void	createGraphicPipeline()
	{
		VkRect2D								scissor = {};
		VkViewport								viewport = {};
		vector<char>							vertShaderCode;
		vector<char>							fragShaderCode;
		VkDynamicState							dynamicStates[2];
		VkShaderModule							vertShaderModule;
		VkShaderModule							fragShaderModule;
		VkPipelineLayoutCreateInfo				pipelineLayoutInfo = {};
		VkGraphicsPipelineCreateInfo			pipelineInfo = {};
		VkPipelineShaderStageCreateInfo			shaderStages[2];
		VkPipelineShaderStageCreateInfo			vertShaderStageInfo = {};
		VkPipelineShaderStageCreateInfo			fragShaderStageInfo = {};
		VkPipelineDynamicStateCreateInfo		dynamicStateInfos = {};
		VkPipelineViewportStateCreateInfo		viewportStateInfo = {};
		VkPipelineColorBlendStateCreateInfo		colorBlendingInfo = {};
		VkPipelineColorBlendAttachmentState		colorBlendAttach = {};
		VkPipelineVertexInputStateCreateInfo	vertexInputInfo = {};
		VkPipelineMultisampleStateCreateInfo	multisampling = {};
		VkPipelineInputAssemblyStateCreateInfo	inputAssembly = {};
		VkPipelineRasterizationStateCreateInfo	rasterizer = {};

		/*Shader initialisation*/
		{
			vertShaderCode = readFile("shaders/vert.spv");
			vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStageInfo.pName = "main";
			vertShaderModule = createShaderModule(vertShaderCode);
			vertShaderStageInfo.module = vertShaderModule;

			fragShaderCode = readFile("shaders/frag.spv");
			fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStageInfo.pName = "main";
			fragShaderModule = createShaderModule(fragShaderCode);
			fragShaderStageInfo.module = fragShaderModule;

			shaderStages[0] = vertShaderStageInfo;
			shaderStages[1] = fragShaderStageInfo;
		}

		/*Vertex input initialisation*/
		{
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = NULL;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = NULL;
		}

		/*Input assembly initialisation*/
		{
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssembly.primitiveRestartEnable = VK_FALSE; 
		}

		/*Viewport initialisation*/
		{
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)swapChainExtent.width;
			viewport.height = (float)swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
		}

		/*scissor initialisation*/
		{
			scissor.offset = { 0, 0 };
			scissor.extent = swapChainExtent;
		}

		/*Viewport state initialisation*/
		{
			viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateInfo.viewportCount = 1;
			viewportStateInfo.pViewports = &viewport;
			viewportStateInfo.scissorCount = 1;
			viewportStateInfo.pScissors = &scissor;
		}

		/*Rasterizer initialisation*/
		{
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f; // Optional
			rasterizer.depthBiasClamp = 0.0f; // Optional
			rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
		}
		
		/*Anti aliasing initialisation*/
		{
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampling.minSampleShading = 1.0f; // Optional
			multisampling.pSampleMask = NULL; // Optional
			multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
			multisampling.alphaToOneEnable = VK_FALSE; // Optional
		}

		/*Color blending initialisation*/
		{
			colorBlendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttach.blendEnable = VK_FALSE;
			colorBlendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			colorBlendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttach.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			colorBlendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			colorBlendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttach.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

			colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendingInfo.logicOpEnable = VK_FALSE;
			colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
			colorBlendingInfo.attachmentCount = 1;
			colorBlendingInfo.pAttachments = &colorBlendAttach;
			colorBlendingInfo.blendConstants[0] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[1] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[2] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[3] = 0.0f; // Optional
		}

		/*Dynamic state initialisation*/
		{
			dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
			dynamicStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateInfos.dynamicStateCount = 2;
			dynamicStateInfos.pDynamicStates = dynamicStates;
		}

		/*Pipeline layout initialisation and creation*/
		{
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 0; // Optional
			pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
			pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
			pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

			if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
				throw runtime_error("Failed to create pipeline layout!");
		}

		/*Pipeline initialisation and creation*/
		{
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = shaderStages;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportStateInfo;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = NULL; // Optional
			pipelineInfo.pColorBlendState = &colorBlendingInfo;
			pipelineInfo.pDynamicState = &dynamicStateInfos;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.renderPass = renderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
			pipelineInfo.basePipelineIndex = -1; // Optional
		}

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
			throw runtime_error("Failed to create graphics pipeline!");

		vkDestroyShaderModule(device, fragShaderModule, NULL);
		vkDestroyShaderModule(device, vertShaderModule, NULL);
	}

	void	createRenderPass()
	{
		VkSubpassDependency			dependency = {};
		VkSubpassDescription		subpass = {};
		VkAttachmentReference		colorAttachRef = {};
		VkRenderPassCreateInfo		renderPassInfo = {};
		VkAttachmentDescription		colorAttachment = {};

		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		colorAttachRef.attachment = 0;
		colorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachRef;

		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			throw runtime_error("Failed to create render pass!");
	}

	void createFramebuffers()
	{
		VkFramebufferCreateInfo		framebufferInfo = {};

		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView	attachments[] = { swapChainImageViews[i] };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
				throw runtime_error("Failed to create framebuffer!");
		}
	}

	void	createCommandPool()
	{
		QueueFamilyIndices			queueFamilyIndices;
		VkCommandPoolCreateInfo		poolInfo = {};

		queueFamilyIndices = findQueueFamilies(physicalDevice);
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
		poolInfo.flags = 0; // Optional
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
			throw runtime_error("Failed to create command pool!");
	}

	void	createCommandBuffers()
	{
		VkRect2D						scissor = {};
		VkViewport						viewport = {};
		VkClearValue					clearColor;
		VkCommandBufferAllocateInfo		allocInfo = {};
		VkCommandBufferBeginInfo		beginInfo = {};
		VkRenderPassBeginInfo			renderPassInfo = {};

		commandBuffers.resize(swapChainFramebuffers.size());
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
			throw runtime_error("Failed to allocate command buffers!");

		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = NULL; // Optional

			vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;
			clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			{
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)swapChainExtent.width;
				viewport.height = (float)swapChainExtent.height;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				scissor.offset = { 0, 0 };
				scissor.extent = swapChainExtent;
			}

			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
				throw runtime_error("Failed to record command buffer!");
		}
	}

	void	createSemaphores()
	{
		VkSemaphoreCreateInfo	semaphoreInfo = {};

		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
			throw runtime_error("Failed to create semaphores!");
	}

	void cleanupSwapChain()
	{
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
			vkDestroyFramebuffer(device, swapChainFramebuffers[i], NULL);
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		//vkDestroyPipeline(device, graphicsPipeline, NULL);
		//vkDestroyPipelineLayout(device, pipelineLayout, NULL);
		//vkDestroyRenderPass(device, renderPass, NULL);
		for (size_t i = 0; i < swapChainImageViews.size(); i++)
			vkDestroyImageView(device, swapChainImageViews[i], NULL);
		vkDestroySwapchainKHR(device, swapChain, NULL);
	}

	void recreateSwapChain()
	{
		//vkDeviceWaitIdle(device);

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		//createRenderPass();
		//createGraphicPipeline();
		createFramebuffers();
		createCommandBuffers();
	}

	void	initVulkan()
	{
		//uint32_t	extensionCount = 0;
		//vector<VkExtensionProperties> extensions;

		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSemaphores();
		/*
		vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
		extensions.resize(extensionCount);
		vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions.data());
		cout << "Available extensions:" << endl;
		for (const VkExtensionProperties& extension : extensions)
		{
			cout << "\t" << extension.extensionName << endl;
		}*/
	}

	void drawFrame()
	{
		VkResult				result;
		uint32_t				imageIndex;
		VkSemaphore				waitSemaphores[1];
		VkSubmitInfo			submitInfo = {};
		VkSemaphore				signalSemaphores[1];
		VkPresentInfoKHR		presentInfo = {};
		VkPipelineStageFlags	waitStages[1];
		VkSwapchainKHR			swapChains[1];

		result = vkAcquireNextImageKHR(device, swapChain, numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			throw runtime_error("Failed to acquire swapchain image!");
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		waitSemaphores[0] = imageAvailableSemaphore;
		waitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		signalSemaphores[0] = renderFinishedSemaphore;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
			throw runtime_error("Failed to submit draw command buffer!");

		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		swapChains[0] = swapChain;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = NULL; // Optional

		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			recreateSwapChain();
		else if (result != VK_SUCCESS)
			throw runtime_error("Failed to present swap chain image!");
		vkQueueWaitIdle(presentQueue);
	}

	void	mainLoop()
	{
		int	fps;
		double	curentTime;
		double  lastTime;

		fps = 0;
		lastTime = glfwGetTime();
		while (!glfwWindowShouldClose(window))
		{
			curentTime = glfwGetTime();
			if (curentTime - lastTime > 1.0)
			{
				lastTime = curentTime;
				cout << fps << " FPS" << endl;
				fps = 0;
			}
			glfwPollEvents();
			drawFrame();
			fps++;
		}
		vkDeviceWaitIdle(device);
	}

	void	cleanup()
	{
		vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
		vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
		
		cleanupSwapChain();

		vkDestroyPipeline(device, graphicsPipeline, NULL);
		vkDestroyPipelineLayout(device, pipelineLayout, NULL);
		vkDestroyRenderPass(device, renderPass, NULL);

		vkDestroyCommandPool(device, commandPool, NULL);
		
		vkDestroyDevice(device, NULL);
		DestroyDebugReportCallbackEXT(instance, callback, NULL);
		vkDestroySurfaceKHR(instance, surface, NULL);
		vkDestroyInstance(instance, NULL);

		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

int		main()
{
	HelloTriangleApplication	app;

	try
	{
		app.run();
	}
	catch (const runtime_error& e)
	{
		cerr << e.what() << endl;
		system("PAUSE");
		return (1);
	}
	system("PAUSE");
	return (0);
}