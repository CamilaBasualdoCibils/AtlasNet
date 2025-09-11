#include "KDNetServer.hpp"

void KDNetServer::Initialize(KDNetServer::InitializeProperties properties) {
    std::cout << "KDNet Initialize\n";
}

void KDNetServer::Update(std::span<KDEntity> entities, std::vector<KDEntity>& IncomingEntities,
		   std::vector<KDEntityID>& OutgoingEntities) {}