#include "GameCoordinator/GameCoordinator.hpp"
#include "pch.hpp"

int main()
{
    if (!GameCoordinator::Get().Init())
    {
        std::cerr << "[Main] Failed to initialize GameCoordinator!" << std::endl;
        return -1;
    }

    GameCoordinator::Get().Run();
    return 0;
}