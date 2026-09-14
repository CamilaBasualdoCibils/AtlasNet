#pragma once

#include "atlasnet/core/events/IEvent.hpp"
#include "atlasnet/core/tasks/TaskHandle.hpp"
#include "atlasnet/core/tasks/TaskSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include <functional>
#include <string_view>
#include <typeindex>
namespace AtlasNet
{
class IEventSystem
{
public:
  virtual ~IEventSystem() = default;

  template <typename EventType>
    requires(std::derived_from<EventType, IEvent>)
  void On(std::function<void(const EventType&)> cb)
  {
    On(EventType::ID,
       [c = std::move(cb)](const std::string_view& data)
       {
         EventType event;
         ByteReader reader(data);
         event.Deserialize(reader);
         c(event);
       });
    
  }

  template <typename EventType>
  TaskHandle<> Emit(const EventType& event)
    requires(std::derived_from<EventType, IEvent>)
  {
    ByteWriter writer;
    event.Serialize(writer);
    return Emit(EventType::ID, writer.as_string_view());
  }

  void On(EventID eventID, std::function<void(const std::string_view&)> cb) {
    impl_On(eventID, std::move(cb));
  }
  TaskHandle<> Emit(EventID eventID, const std::string_view data)
  {
    return impl_Emit(eventID, data);
  }

protected:
  virtual void impl_On(EventID eventID,
                       std::function<void(const std::string_view&)> cb) = 0;
  virtual TaskHandle<> impl_Emit(EventID eventID, const std::string_view& data) = 0;

private:
  template <typename EventType>
  static std::function<void(const IEvent*)>
  wrap(std::function<void(const EventType&)> cb)
  {
    return [cb = std::move(cb)](const IEvent* e)
    { cb(*static_cast<const EventType*>(e)); };
  }
};
} // namespace AtlasNet