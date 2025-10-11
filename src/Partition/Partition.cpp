#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ServerRegistry.hpp"
Partition::Partition()
{
}
Partition::~Partition()
{

	logger->Print("Goodbye from Partition!");
}
void Partition::Init()
{
	InterLinkIdentifier partitionIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
	Interlink::Get().Init(
		InterlinkProperties{
			.ThisID = partitionIdentifier,
			.logger = logger,
			.callbacks = {.acceptConnectionCallback = [](const Connection &c)
						  { return true; },
						  .OnConnectedCallback = [](const InterLinkIdentifier& Connection) {}}});
	IPAddress myAddress;
	myAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":"+std::to_string(_PORT_PARTITION));
	ServerRegistry::Get().RegisterSelf(partitionIdentifier, myAddress);

	while (!ShouldShutdown)
	{

		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	logger->Print("Shutting Down");
	ServerRegistry::Get().DeRegisterSelf(partitionIdentifier);
	Interlink::Get().Shutdown();
}