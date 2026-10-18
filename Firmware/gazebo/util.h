#include <filesystem>

std::filesystem::path getPluginPath() {
	Dl_info dl_info;
	if (dladdr((void*)&getPluginPath, &dl_info) == 0) {
		throw std::runtime_error("Unable to determine plugin path using dladdr.");
	}
	return std::filesystem::path(dl_info.dli_fname);
}
