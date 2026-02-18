#pragma once
#include "Network/NetworkIdentity.hpp"
#include "Global/Misc/Singleton.hpp"
class EventSystem : public Singleton<EventSystem>
{
	NetworkIdentity ID;

   public:
	EventSystem() = default;

	void Init(const NetworkIdentity& _ID) { 
        
        ID = _ID; 
    
    }
};