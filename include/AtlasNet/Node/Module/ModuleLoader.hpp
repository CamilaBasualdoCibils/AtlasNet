#pragma once
#include "AtlasNet/Node/Module/ModuleAPI.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace AtlasNet::Module
{
class ModuleLoader
{
public:
  struct LoadedModule
  {
    std::string name;
    std::string version;
    Capability capabilities;
  };
  ModuleLoader() = default;
  ModuleLoader(const ModuleLoader&) = delete;
  ModuleLoader& operator=(const ModuleLoader&) = delete;
  ~ModuleLoader();
  const LoadedModule& Load(const std::filesystem::path& path,
                           ModuleRegistry& registry);
  [[nodiscard]] const std::vector<LoadedModule>& Modules() const noexcept
  {
    return modules;
  }

private:
  std::vector<void*> handles;
  std::vector<LoadedModule> modules;
};
} // namespace AtlasNet::Module
