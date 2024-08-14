#include "App.hpp"
#include "GLFW/glfw3.h"
#include "Log.hpp"
#include "vulkan/vulkan_core.h"
#include <fstream>
#include <stdexcept>
#include <set>
#include <limits>

/*
 !       :::      .::....    ::: :::      :::  .    :::.   :::.    :::.
 !       ';;,   ,;;;' ;;     ;;; ;;;      ;;; .;;,. ;;`;;  `;;;;,  `;;;
 !        \[[  .[[/  [['     [[[ [[[      [[[[[/'  ,[[ '[[,  [[[[[. '[[
 !         Y$c.$$"   $$      $$$ $$'     _$$$$,   c$$$cc$$$c $$$ "Y$c$$
 !          Y88P     88    .d888o88oo,.__"888"88o, 888   888,888    Y88
 !           MP       "YmmMMMM""""""YUMMM MMM "MMP"YMM   ""` MMM     YM
*/

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

App::App() {
	//* 1. Initialize the window
	initVulkan();
	//* 2. Create the Vulkan instance. This contains the information about the application as well
	//*    as the required extensions and validation layers.
	createInstance();
	//* 3. Creates the window surface. This is the connection between the Vulkan instance and the
	//*    window system. It is handled by GLFW.
	createSurface();
	//* 4. Select a physical device (GPU) that supports the features we need. It uses the function
	//*    isDeviceSuitable. isDeviceSuitable checks if the required queues (graphics and
	//*    presentation queues), extensions (swapchain) are available, and if the swapchain itself
	//*    is adequate.
	pickPhysicalDevice();
	//* 5. Create the logical device. The logical device is the connection between the application
	//*    and the physical device. Here we specify the queues, features and validation layers we
	//*    want to use.
	createLogicalDevice();
	//* 6. Create the swap chain. We have 3 functions, chooseSwapSurfaceFormat,
	//*    chooseSwapPresentMode, and chooseSwapExtent to choose the swapchain details we want from
	//*    the list of available choices.
	//*    After that we store the swapchain details and retrieve the swapchain images.
	createSwapChain();
	//* 7. Create the image views. Image views specify how to access the image (in this case
	//*    swapchain images) and which part of the image to access.
	createImageViews();
	//* 8. Create the Render pass. This tells vulkan about our framebuffer attachments, color and
	//*    depth buffers, etc.
	createRenderPass();
	//* 9. Create the graphics pipeline. This stores the complete sequence of operations that tell
	//*    Vulkan how to go from a set of vertex data to the final output on the screen
	createGraphicsPipeline();

	createFramebuffers();
	createCommandPool();
	createCommandBuffer();
	createSyncObjects();
	mainLoop();
}

App::~App() { cleanup(); }

void App::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

/*
 : ------------------------------------------------------------------------------------------------
 : MARK:									Window Creation
 : ------------------------------------------------------------------------------------------------
*/
void App::initVulkan() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan window", nullptr, nullptr);
	LOG_INFO("[VULKAN]: Window created");
}

//: Check if the requested validation layers are available. These are provided by LunarG

bool App::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char *layerName : validationLayers) {
		bool layerFound = false;
		for (const auto &layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;
	}
	return true;
}

/*
 : ------------------------------------------------------------------------------------------------
 : MARK:							Vulkan instance creation
 :  	General application information, along with the required extensions and validation layers
 : ------------------------------------------------------------------------------------------------
*/
void App::createInstance() {
	if (enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("Validation layers requested, but not available!");

	// (optional) basic information about our application
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// Vulkan instance creation details
	// Specifies which global extensions and validation layers we want to use
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// GLFW already has a function which returns a list of extensions it needs
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	// Enable validation layers
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else
		createInfo.enabledLayerCount = 0;

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create instance");

	LOG_INFO("[VULKAN]: Vulkan instance created");
}

/*
 : ------------------------------------------------------------------------------------------------
 : MARK: 							Surface creation
 :
 :  The surface is the connection between the Vulkan instance and the window system. GLFW handles
 :  this as it is platform specific.
 : ------------------------------------------------------------------------------------------------
*/
void App::createSurface() {
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface!");
}

/*
 : ------------------------------------------------------------------------------------------------
 : 	MARK:							Pick Physical Device
 :
 :  The physical device is the actual GPU. We need to select a physical device that supports the
 :  required features. We check if the device supports the required queue families, extensions,
 :  and swap chain.
 : ------------------------------------------------------------------------------------------------
*/
void App::pickPhysicalDevice() {
	physicalDevice = VK_NULL_HANDLE;

	//! This pattern of querying the number of devices and then querying the devices themselves is
	//! common in Vulkan.
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0)
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	LOG_TRACE("[VULKAN]: Number of physical devices: {}", deviceCount);

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	// Choose the first suitable device
	for (const auto &device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("failed to find a suitable GPU!");

	// Print the name of the GPU
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	LOG_INFO("[VULKAN]: Physical device selected");
	LOG_TRACE("[VULKAN]: Physical device: {}", deviceProperties.deviceName);
}

//: Check if the given physical device supports the required features.
bool App::isDeviceSuitable(VkPhysicalDevice device) {
	// Get the device properties and features
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// Check if the device supports the required queue families, extensions, and swap chain
	QueueFamilyIndices indices = findQueueFamilies(device);
	bool extensionsSupported = checkDeviceExtensionSupport(device);
	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate =
			!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

//: Find the queue families supported.
QueueFamilyIndices App::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto &queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphicsFamily = i;

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;
		i++;
	}
	return indices;
}

//: Check if the device supports the required extensions (swap chain).
bool App::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
										 availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto &extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

/*
 : ------------------------------------------------------------------------------------------------
 : 	MARK:							Create Logical Device
 :
 :  Think of physical device as the 'class', and the logical device as an instance of that class,
 :  with certain specified features and extensions enabled. Multiple apps have to share the
 :  physical device, but each app can have its own logical device with their own configuration of
 :  the physical device as they require.
 : ------------------------------------------------------------------------------------------------
*/
void App::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	// The graphics and presentation queues might be the same. So we use a set to ensure uniqueness.
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
											  indices.presentFamily.value()};

	// We have to supply a list of queues while creating the logical device.
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create logical device");
	LOG_INFO("[VULKAN]: Logical device created");

	// Gather queue handles
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

//: Query the swap chain support details.
SwapChainSupportDetails App::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
												  details.presentModes.data());
	}

	return details;
}

// Choose appropriate Swap surface format based on available formats.
VkSurfaceFormatKHR
App::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
	// VkSurfaceFormatKHR has format and colorSpace members.
	// Choose 32 bit sRGB format if available, otherwise choose the first available format.
	for (const auto &availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}
	return availableFormats[0];
}

// Choose appropriate Swap present mode based on available present modes.
// The present mode defines how Vulkan should display the rendered images on the swap surface.
VkPresentModeKHR
App::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
	// VK_PRESENT_MODE_IMMEDIATE_KHR: Images rendered are displayed right away.
	// VK_PRESENT_MODE_FIFO_KHR: Basically, Vsync.
	// VK_PRESENT_MODE_FIFO_RELAXED_KHR: Vsync, but if the queue is empty, render the image right
	// away.
	// VK_PRESENT_MODE_MAILBOX_KHR: Triple buffering.

	// If VK_PRESENT_MODE_MAILBOX_KHR is available, use it.
	for (const auto &availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return availablePresentMode;
	}

	// VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
	return VK_PRESENT_MODE_FIFO_KHR;
}

//: The swap extent is the size (resolution) of the swap surface images.
VkExtent2D App::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
	/*
	 * Some window managers allow us to specify the resolution of the swap chain images.
	 * They do this by setting the width and height to the max value of uint32_t
	 * If that is the case, then we have to specify the width and height ourselves. Otherwise, we
	 * use the width and height specified by the window manager.
	 */
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	VkExtent2D actualExtent = {
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
	};

	actualExtent.width = std::max(capabilities.minImageExtent.width,
								  std::min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height =
		std::max(capabilities.minImageExtent.height,
				 std::min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

/*
 : ------------------------------------------------------------------------------------------------
 :	 MARK:                               Create Swap Chain
 :
 :  Swapchain is a sequence of images owned by the GPU. We request images from the swap chain to be
 :  used as render targets, and after rendering, we return them back to the swap chain for display.
 : ------------------------------------------------------------------------------------------------
*/
void App::createSwapChain() {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	// 0 is a special value which means there is no maximum limit.
	if (swapChainSupport.capabilities.maxImageCount > 0 &&
		imageCount > swapChainSupport.capabilities.maxImageCount)
		imageCount = swapChainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	// If we want to do post processing, we can use VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use
	// a memory operation to transfer the rendered image to a swap chain image
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	// Specify how the images will be used across multiple queue families.
	// If the graphics and present queues are different, we use concurrent sharing mode.
	// Otherwise, we use exclusive sharing mode.
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;	  // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create swap chain");
	LOG_INFO("[VULKAN]: Swap chain created");
	LOG_TRACE("[VULKAN]: Swap chain image count: {}", imageCount);

	// Store the swap chain format, and extent
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// Retrieve the swap chain images
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Create Image View
 :
 :  VkImageView object helps select only part (array or mip) of the VkImage. We have to create image
 :  views for the swapchain images.
 : ------------------------------------------------------------------------------------------------
*/
void App::createImageViews() {
	swapChainImageViews.resize(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
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

		VkResult result = vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to create image views");
	}
	LOG_INFO("[VULKAN]: Image views created");
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Create Render Pass
 :
 :  The render pass tells Vulkan about our framebuffer attachments, color and depth buffers, number
 :  of samples, and how to handle the content throughout rendering operations.
 : ------------------------------------------------------------------------------------------------
*/
void App::createRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Single color buffer attachment
	// Clear color and depth buffers before rendering
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// Store the contents after rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// Dont care about stencil buffers
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// Layout transition:
	// LAYOUT_UNDEFINED (before rendering) -> LAYOUT_PRESENT_SRC_KHR (after rendering)
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	/*
	 * Subpasses: A render pass can have multiple subpasses. Each subpass references one or more of
	 * the attachments that we've described in the render pass. It also describes the layout
	 * transitions that need to take place during the subpass.
	 */

	// We just create 1 subpass which uses the color attachment
	VkAttachmentReference colorAttachmentRef{};
	// We only have 1 color attachment (index 0) so we reference that
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	// Finally create the render pass
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	/*
	 * Subpass Dependencies:
	 * Too big to explain. read from here
	 * https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Subpass-dependencies
	 */
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass");
	LOG_INFO("[VULKAN]: Render pass created");
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Graphics Pipeline!
 :
 : The graphics pipeline is the sequence of operations that take the
 : vertices and textures of your meshes all the way to the pixels in the render targets.
 :
 : Here we configure these:
 : 1. Shader pipeline
 : 2. Vertex layout info (glVertexAttribPointer)
 : 3. Vertex assembly (Triangles, Triangle strips, Lines)
 : 4. Viewport and Scissor
 : 5. Rasterizer (render as FILL, LINES, POINTS), lineWidth, depth bias, etc.
 : 6. Multisampling (for Anti-Aliasing)
 : 7. Color attachments (For global state as well as per framebuffer)
 : 8. Pipeline creation (Phew, finally) :
 : ------------------------------------------------------------------------------------------------
*/
void App::createGraphicsPipeline() {
	//* Shader stuff:
	// Read the SPIR-V bytecode from the files
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	// Create shader modules from the bytecode
	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	// Information about the 2 shader modules (we'll use them as vertex and fragment shaders)
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	//* Vertex data format
	// The format of the vertex data. This is equivalent to OpenGL's glVertexAttribPointer
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	//* Vertex Assembler
	// How to assemble the primitives (polygons, lines for wireframe, points)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	//* Viewport and Scissor
	// Viewport defines the region of the framebuffer we will render to.
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// "While viewports define the transformation from the image to the framebuffer, scissor
	// rectangles define in which regions pixels will actually be stored. Any pixels outside the
	// scissor rectangles will be discarded by the rasterizer."
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapChainExtent;

	// Here we are setting viewport and scissor statically during pipeline creation. We also
	// could've specified them dynamically during render time.
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	//* Rasterizer
	// The rasterizer is responsible for turning the geometry into fragments to be filled
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; // This could be useful in shadow mapping
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	// You can set the following as: VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE,
	// VK_POLYGON_MODE_POINT
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	//* Multisampling
	// Multisampling (for AA). Disabling it for now.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//* Color blending:
	// The first struct, VkPipelineColorBlendAttachmentState contains the configuration per
	// attached framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
										  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	// and the second struct, VkPipelineColorBlendStateCreateInfo contains the global
	// color blending settings
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	//* Pipeline layout creation info
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;	  // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional

	VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline layout");
	LOG_INFO("[VULKAN]: Pipeline layout created");

	//* Finally, the pipeline creation info
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	// pipelineInfo.pDynamicState = &dynamicState; // If dynamic state for viewport and scissor used
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1;			  // Optional

	result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
									   &graphicsPipeline);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline");
	LOG_INFO("[VULKAN]: Graphics pipeline created");

	// Delete the temporary shader module objects
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

//: Create a shader module from the shader code.
VkShaderModule App::createShaderModule(const std::vector<char> &code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module");
	LOG_INFO("[VULKAN]: Shader module created");
	return shaderModule;
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Framebuffer creation
 :
 : The attachments specified during render pass creation are bound by wrapping them into a
 : VkFramebuffer object. A framebuffer object references all of the VkImageView objects that
 : represent the attachments.
 :
 : we have to create a framebuffer for all of the images in the swap chain and use the one that
 : corresponds to the retrieved image at drawing time.
 : ------------------------------------------------------------------------------------------------
*/
void App::createFramebuffers() {
	swapChainFramebuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		VkImageView attachments[] = {swapChainImageViews[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		VkResult result =
			vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to create framebuffer");
	}

	LOG_INFO("[VULKAN]: Framebuffers created");
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Command Pool
 :
 : Command pools are used to allocate command buffers. Idk what else
 : ------------------------------------------------------------------------------------------------
*/
void App::createCommandPool() {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create command pool");
	LOG_INFO("[VULKAN]: Command pool created");
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Command Buffer
 :
 :  Command buffers are objects used to record commands which can be submitted to a queue for
 :  execution.
 : ------------------------------------------------------------------------------------------------
*/
void App::createCommandBuffer() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	// Primary buffers are submitted to queues for execution.
	// Secondary buffers are meant to be called from primary buffers.
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffer");
	LOG_INFO("[VULKAN]: Command buffer created");
}

//: MARK: Record commands

void App::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	// Begin recording
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;				  // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to begin recording command buffer");
	LOG_INFO("[VULKAN]: Command buffer recording started");

	// Render pass information
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapChainExtent;

	// Setting the clear color
	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	// Begin the render pass
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	//* Drawing commands from here:
	// Bind our graphics pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Dynamic viewport and scissor here if needed

	// Actual draw command (underwhelming, I know)
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	// End the render pass
	vkCmdEndRenderPass(commandBuffer);

	// End recording
	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to record command buffer");
	LOG_INFO("[VULKAN]: Command buffer recording ended");
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Sync Objects
 :
 :  Semaphores are used to synchronize GPU operations between the various queues. Fences are used to
 :  synchronize CPU operations with GPU operations. "Signalling" a semaphore or a fence means that
 :  the GPU has finished executing the command buffer.
 :
 :  Here we use semaphores to:
 :  1. Signal that an image is available for rendering (after which we can start drawing).
 :  2. Signal that rendering has finished (after which we can present the image to the window).
 :
 :  We use fences to wait for the frame to finish before starting to render the next one.
 : ------------------------------------------------------------------------------------------------
*/
void App::createSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Since we are using the fence to wait for the frame to finish, we start it in the signaled
	// state. Otherwise the fence will be in the unsignaled state and the vkWaitForFences function
	// will wait forever.
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) !=
			VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) !=
			VK_SUCCESS ||
		vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
		throw std::runtime_error("Failed to create synchronization objects");
	LOG_INFO("[VULKAN]: Synchronization objects created");
}

//: MARK: Main loop

void App::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		drawFrame();
	}

	// Wait for the device to finish before cleaning up
	vkDeviceWaitIdle(device);
}

/*
 : ------------------------------------------------------------------------------------------------
 :  MARK:								Draw Frame
 :
 :  At a high level, rendering a frame in Vulkan consists of a common set of steps:
 :  1. Wait for the previous frame to finish
 :  2. Acquire an image from the swap chain
 :  3. Record a command buffer which draws the scene onto that image
 :  4. Submit the recorded command buffer
 :  5. Present the swap chain image
 : ------------------------------------------------------------------------------------------------
*/
void App::drawFrame() {
	// 1. Wait for the previous frame to finish
	vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &inFlightFence);

	// 2. Acquire an image from the swap chain
	uint32_t imageIndex;
	// The imageAvailableSemaphore is signalled when the image is available for rendering
	vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE,
						  &imageIndex);

	// 3. Record a command buffer which draws the scene onto that image
	vkResetCommandBuffer(commandBuffer, 0);
	// The command buffer contains our rendering code. We submit it to the graphics queue next.
	recordCommandBuffer(commandBuffer, imageIndex);

	// 4. Submit the recorded command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
	// The waitStages array specifies the stages at which the semaphore waits. We want to wait at
	// the color attachment stage before executing the command buffer. This means that the vertex
	// shader and other early stages can already start executing while the image is still not
	// available.
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	// Wait until imageAvailableSemaphore is signalled. We can start drawing after that.
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Once the rendering is done, we signal the renderFinishedSemaphore. After that, we can
	// present the image to the window.
	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// So to summarize, the queue will wait for the imageAvailableSemaphore to be signalled before
	// executing the rendering commands. Once the rendering is done, it will signal the
	// renderFinishedSemaphore.
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to submit draw command buffer");
	LOG_INFO("[VULKAN]: Command buffer submitted");

	// 5. Present the swap chain image
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// Wait for the renderFinishedSemaphore to be signalled before presenting the image.
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	// List of swap chains to present images to. We only have 1 swapchain.
	VkSwapchainKHR swapChains[] = {swapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image");
	LOG_INFO("[VULKAN]: Swap chain image presented");
}

//: MARK: Cleanup
void App::cleanup() {
	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroyFence(device, inFlightFence, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	for (auto framebuffer : swapChainFramebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (auto imageView : swapChainImageViews)
		vkDestroyImageView(device, imageView, nullptr);
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
}

std::vector<char> App::readFile(const std::string &filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}
