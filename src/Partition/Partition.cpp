#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "Database/DatabaseRedis.hpp"
#include "pch.hpp"
Partition::Partition()
{
	Interlink::Get().Initialize(
		InterlinkProperties{.Type = InterlinkType::ePartition,
							.logger = logger,
							.bOpenListenSocket = true,
							.ListenSocketPort = CJ_LOCALHOST_PARTITION_PORT,
							.acceptConnectionFunc = [](const Connection &c) { return true; }});
	Run();
}
Partition::~Partition()
{
	logger->Print("Goodbye from Partition!");
}
void Partition::Run()
{
  try {
      // Connect to Redis started by Start.sh
      sw::redis::Redis redis("tcp://127.0.0.1:6379");

      redis.set("foo", "bar");
      auto val = redis.get("foo");
      if (val) {
          std::cout << "Redis says foo=" << *val << "\n";
      }
  } catch (const sw::redis::Error &e) {
      std::cerr << "Redis error: " << e.what() << std::endl;
  }

	while (true)
	{

		Interlink::Get().Tick();

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}