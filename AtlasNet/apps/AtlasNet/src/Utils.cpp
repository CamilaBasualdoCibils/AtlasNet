#include "Utils.hpp" 
std::filesystem::path GetSDKPath() {
return GetEXEPath() / "../sdk";	 // relative to bin/
}
std::filesystem::path GetEXEPath()
{
	// Get the directory of the executable
	std::filesystem::path exe_dir =
		std::filesystem::read_symlink("/proc/self/exe").parent_path();	// Linux
	// On Windows, use GetModuleFileName()
	return exe_dir;	 // relative to bin/
}
