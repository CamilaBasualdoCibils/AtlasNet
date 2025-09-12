#include "Connection.hpp"
#include "Singleton.hpp"
#include "pch.hpp"

enum class InterlinkType {
    eInvalid = -1,
    ePartition = 1,   // partition Server
    eGod = 2,	      // God
    eGodView = 3,     // God Debug Tool
    eGameClient = 4,  // Game Client / Player
    eGameServer = 5   // Game Server
};
struct InterlinkProperties {
    InterlinkType Type = InterlinkType::eInvalid;
};
class Interlink : public Singleton<Interlink> {
    std::unordered_map<InterlinkType, Connection> OpenConnections;
    ISteamNetworkingSockets* networkInterface;
    InterlinkType ThisType;
    private:
    void ConnectToGod();
   public:
    Interlink() {};
    void Initialize(const InterlinkProperties& properties);
    std::weak_ptr<Connection> OpenConnection()
};
