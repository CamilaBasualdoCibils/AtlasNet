#pragma once

#include <cassert>
#include <functional>
#include <string>
#include <unordered_map>
namespace AtlasNet::Events
{
class IGlobalEvents
{
public:
  IGlobalEvents() = default;
  virtual ~IGlobalEvents() = default;
  using EventCallback = std::function<void(std::string)>;
  void OnEvent(const std::string& eventName,
               const std::function<void(std::string)>& callback)
  {
    assert(!eventName.empty());
    assert(callback != nullptr);
    assert(!eventCallbacks.contains(eventName));
    eventCallbacks[eventName] = callback;
    __SubscribeToEvent(eventName);
  }

  void Shutdown()
  {
    for (const auto& [eventName, callback] : eventCallbacks)
    {
      __UnsubscribeFromEvent(eventName);
    }
    eventCallbacks.clear();
  }

protected:
  void __DispatchEvent(const std::string& eventName,
                       const std::string& eventData)
  {
    assert(!eventName.empty());
    assert(eventCallbacks.contains(eventName));
    if (eventCallbacks.contains(eventName))
    {
      eventCallbacks[eventName](eventData);
    }
  }

private:
  virtual void __SubscribeToEvent(const std::string& eventName) = 0;
  virtual void __UnsubscribeFromEvent(const std::string& eventName) = 0;
  std::unordered_map<std::string, EventCallback> eventCallbacks;
};
} // namespace AtlasNet::Events