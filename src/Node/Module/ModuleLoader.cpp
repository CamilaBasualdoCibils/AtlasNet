#include "AtlasNet/Node/Module/ModuleLoader.hpp"
#include <dlfcn.h>
#include <stdexcept>
namespace
{
std::runtime_error LoadError(const std::filesystem::path& path,
                             const std::string& detail)
{
  return std::runtime_error("Failed to load AtlasNet module '" + path.string() +
                            "': " + detail);
}
} // namespace
AtlasNet::Module::ModuleLoader::~ModuleLoader()
{
  for (auto it = handles.rbegin(); it != handles.rend(); ++it)
    dlclose(*it);
}
const AtlasNet::Module::ModuleLoader::LoadedModule&
AtlasNet::Module::ModuleLoader::Load(const std::filesystem::path& path,
                                     ModuleRegistry& registry)
{
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle)
    throw LoadError(path, dlerror());
  try
  {
    dlerror();
    auto getModule =
        reinterpret_cast<GetModuleFunction>(dlsym(handle, ENTRY_POINT));
    if (const char* error = dlerror())
      throw LoadError(path,
                      std::string("missing ") + ENTRY_POINT + ": " + error);
    const ModuleDescriptor* descriptor = getModule();
    if (!descriptor)
      throw LoadError(path, "entry point returned a null descriptor");
    if (descriptor->structSize < sizeof(ModuleDescriptor))
      throw LoadError(path, "module descriptor is too small");
    if (descriptor->abiVersion != ABI_VERSION)
      throw LoadError(path, "incompatible ABI version " +
                                std::to_string(descriptor->abiVersion) +
                                " (node requires " +
                                std::to_string(ABI_VERSION) + ")");
    if (!descriptor->name || !*descriptor->name || !descriptor->version ||
        !*descriptor->version || !descriptor->Register)
      throw LoadError(path, "module descriptor is incomplete");
    for (const auto& loaded : modules)
      if (loaded.name == descriptor->name)
        throw LoadError(path, "duplicate module name '" + loaded.name + "'");
    ModuleRegistry staged;
    descriptor->Register(staged);
    if (staged.RegisteredCapabilities() !=
        static_cast<uint64_t>(descriptor->capabilities))
      throw LoadError(
          path, "registered capabilities do not match the module descriptor");
    registry.MergeFrom(std::move(staged));
    handles.push_back(handle);
    modules.push_back(
        {descriptor->name, descriptor->version, descriptor->capabilities});
    return modules.back();
  }
  catch (...)
  {
    dlclose(handle);
    throw;
  }
}
