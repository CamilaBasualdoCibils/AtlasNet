#include "InterlinkIdentifier.hpp"
#include "InterlinkEnums.hpp"

InterLinkIdentifier InterLinkIdentifier::MakeIDGod()

{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGod;
    id.ID = 0;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDPartition(uint32 _ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::ePartition;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameServer(uint32 _ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameServer;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameClient(uint32 _ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameClient;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGodView()
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameClient;
    id.ID = 0;
    return id;
}

std::string InterLinkIdentifier::ToString() const
{
    return boost::describe::enum_to_string(Type, "UnknownUnknownType?") + std::to_string(ID);
}

std::optional<InterLinkIdentifier> InterLinkIdentifier::FromString(const std::string& input)
{
     std::istringstream iss(input);
     std::string enumName;
     decltype(ID) idValue;
     if (!(iss >> enumName >> idValue))
     {
        return std::nullopt;
     }
    InterLinkIdentifier id;
    bool success = boost::describe::enum_from_string(enumName.c_str(),id.Type);
    if (!success)
    {
        return std::nullopt;
    }
    id.ID = idValue;
    return id;
}
