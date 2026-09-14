#pragma once


#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
namespace AtlasNet
{

class IServiceAdapter
{
public:
  IServiceAdapter(const std::string_view& serviceName,const std::string_view& imageName)
      : _serviceName(serviceName), _imageName(imageName)
  {
    if (_serviceName.empty())
    {
      throw std::invalid_argument("Service name cannot be empty");
    }
    if (_imageName.empty())
    {
      throw std::invalid_argument("Image name cannot be empty");
    }
  }
  virtual ~IServiceAdapter() = default;

  virtual void Create() = 0;
  virtual bool Exists() const = 0;
  virtual void Destroy() = 0;

  virtual void SetReplicaCount(uint32_t count) = 0;
  virtual uint32_t GetReplicaCount() const = 0;

protected:
  std::string _serviceName;
  std::string _imageName;
};
}; // namespace AtlasNet