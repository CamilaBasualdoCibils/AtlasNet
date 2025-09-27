#include "Partition.hpp"
#include "Interlink/Interlink.hpp"

Partition::Partition()
{
Interlink::Get().Initialize(InterlinkProperties{.Type = InterlinkType::ePartition});
}
Partition::~Partition()
{
  std::cerr << "Goodbye from Partition!\n";
}
void Partition::Run()
{
  std::cerr << "Hello from Partition!\n";

  _reciever = std::make_unique<EntityJsonReciever>("0.0.0.0", 18080);
  bool state = _reciever->start();
  
  std::cerr << "[EntityJsonReciever] Success?: " << state << "\n";
}