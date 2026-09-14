#pragma once
#include <cereal/cereal.hpp>
#include <cereal/archives/xml.hpp>
namespace AtlasNet
{
class XMLSerializer
{
    public:
    XMLSerializer() : archive(std::cout) {

    }
    
    auto operator->() {
        return &archive;
    }

    template <typename... Args>
    void operator()(Args&&... args) {
        
        (archive(std::forward<Args>(args)), ...);
    }
private:
    cereal::XMLOutputArchive archive;
};
}