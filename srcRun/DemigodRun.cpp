#include "pch.hpp"
#include "Demigod/Demigod.hpp"

int main()
{
    // delay for now, so 
    std::this_thread::sleep_for(std::chrono::seconds(10));

    Demigod demigod;
    demigod.Init();
    demigod.Run();
    demigod.Shutdown();
    return 0;
}