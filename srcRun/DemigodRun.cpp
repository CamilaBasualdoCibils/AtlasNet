#include "pch.hpp"
#include "Demigod/Demigod.hpp"

int main()
{
    // delay for now, so 
    std::this_thread::sleep_for(std::chrono::seconds(5));

    Demigod demigod;
    demigod.Init();
    demigod.Run();
    demigod.Shutdown();

    const std::string DemigodServiceName = "demigod-service";
    std::string cmd = "docker service rm " + DemigodServiceName + " > /dev/null 2>&1";
    system(cmd.c_str());
    return 0;
}