#include "Log.hpp"
#include "App.hpp"

int main() {
	Log::init();

	App *app = new App();

	try {
		app->run();
	} catch (const std::exception &e) {
		LOG_ERROR("{}", e.what());
	}

	delete app;
	return 0;
}