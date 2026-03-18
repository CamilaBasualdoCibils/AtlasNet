#pragma once

#include <atomic>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Entities/Camera.hpp"
#include "EntityID.hpp"
#include "Global/Misc/Singleton.hpp"
class RTSClient : public Singleton<RTSClient>
{
	
	ClientID AtlasNetClientID;
	std::atomic_bool ShouldShutdown = false;
	Camera* camera;
	vec3 WorkerSelectedColor = vec3(0.6,0.2,0.4);
	std::unordered_set<EntityID> SelectedWorkers;
   public:
	void Run();

	

   private:
	void StartupGL();
	void Render(float DeltaTime);
	void Update(float DeltaTime);
};
