#pragma once
#include <GLFW/glfw3.h>
#include <cstdint>
#include <optional>

const uint32_t WINDOW_WIDTH = 800, WINDOW_HEIGHT = 600;

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

class App {
  private:
	GLFWwindow *window;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

  public:
	App();
	~App();
	void run();

  private:
	void initVulkan();
	bool checkValidationLayerSupport();
	void createInstance();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void cleanup();
};