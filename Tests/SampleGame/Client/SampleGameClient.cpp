#include "SampleGameClient.hpp"

std::string ExePath;
void SampleGameClient::Run()
{
 std::cerr << "SampleGame Starting" << std::endl;
    AtlasNetClient::InitializeProperties InitProperties;
    AtlasNetClient::Get().Initialize(InitProperties);
    InitProperties.ExePath = ExePath;

     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    while (true)
    {
        
        AtlasNetClient::Get().Tick();
    }
}

int main(int argc, char **argv)
{
    ExePath = argv[0];
    SampleGameClient client;
    client.Run();
}