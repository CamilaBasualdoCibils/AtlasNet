#include "KDNetServer.hpp"

#include "Interlink/Interlink.hpp"
void KDNetServer::Initialize(KDNetServer::InitializeProperties properties) {
    std::cerr << "KDNet Initialize\n";
    Interlink::Check();

    Interlink::Get().Initialize(InterlinkProperties{.Type = InterlinkType::eGameServer});
}

void KDNetServer::Update(std::span<KDEntity> entities, std::vector<KDEntity>& IncomingEntities,
			 std::vector<KDEntityID>& OutgoingEntities) {}
