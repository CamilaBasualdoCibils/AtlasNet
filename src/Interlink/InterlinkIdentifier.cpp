#include "InterlinkIdentifier.hpp"
#include "InterlinkEnums.hpp"

InterLinkIdentifier InterLinkIdentifier::MakeIDGod()

{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGod;
    id.ID = "";
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDPartition(const std::string &_ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::ePartition;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameServer(const std::string &_ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameServer;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameClient(const std::string &_ID)
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
    id.ID = "";
    return id;
}

std::string InterLinkIdentifier::ToString() const
{
    return boost::describe::enum_to_string(Type, "UnknownUnknownType?") + std::string(" ") + (ID);
}

std::optional<InterLinkIdentifier> InterLinkIdentifier::FromString(const std::string &input)
{
    std::istringstream iss(input);
    std::string enumName;
    std::string idValue;

    // Read the enum name first
    if (!(iss >> enumName))
    {
        std::cerr << "unable to parse " << input << std::endl;
        return std::nullopt;
    }

    // Try to read a second token (optional)
    iss >> idValue;

    InterLinkIdentifier id;

    bool success = boost::describe::enum_from_string(enumName.c_str(), id.Type);
    if (!success)
    {
        std::cerr << "unable to parse " << input << std::endl;
        return std::nullopt;
    }

    // If only one token was present (like "eGod"), make ID empty
    if (idValue.empty())
    {
        id.ID.clear();
    }
    else
    {
        id.ID = idValue;
    }

    return id;
}

