#include "pch.hpp"
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
}