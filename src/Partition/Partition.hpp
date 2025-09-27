#pragma once
#include "pch.hpp"
#include "TestUnityAPI/EntityJsonReciever.hpp"

class Partition
{
public:
    Partition();
    ~Partition();
    void Run();

private:
    std::unique_ptr<EntityJsonReciever> _reciever;
};