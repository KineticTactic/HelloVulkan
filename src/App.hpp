#pragma once
#include <GLFW/glfw3.h>

const uint32_t WINDOW_WIDTH = 800, WINDOW_HEIGHT = 600;

class App {
  private:
	GLFWwindow *window;
	VkInstance instance;

  public:
	App();
	~App();
	void run();

  private:
	void initVulkan();
	void createInstance();
	void cleanup();
};