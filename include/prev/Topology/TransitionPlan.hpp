#pragma once

#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
#include <queue>
namespace AtlasNet::Network::Topology
{
class TransitionPlan
{
  std::queue<TransitionStep> steps_;
  
  public:
  TransitionPlan() = default;
  virtual ~TransitionPlan() = default;
  TransitionStep NextStep()
  {
    if (steps_.empty())
    {
      return TransitionStep{};
    }
    auto step = steps_.front();
    steps_.pop();
    return step;
  }
  bool HasNextStep() const
  {
    return !steps_.empty();
  }
  size_t StepsRemaining() const
  {
    return steps_.size();
  }
  void PushStep(TransitionStep step)
  {
    steps_.push(std::move(step));
  }
};
} // namespace AtlasNet::Network::Topology