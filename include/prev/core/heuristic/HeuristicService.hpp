#pragma once
namespace AtlasNet
{
class HeuristicService
{
public:
  struct Config
  {
    // Add any necessary configuration parameters here
  };
  HeuristicService(const Config& config) : config_(config)
  {
    // Add any necessary initialization code here
  }

  

private:
  Config config_;
};
} // namespace AtlasNet