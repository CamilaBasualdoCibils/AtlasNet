#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "Debug/Log.hpp"
class Bootstrap
{
	struct Task
	{
		using EnviromentVar = std::pair<std::string, std::string>;
		struct Build
		{
			std::string dockerfile;
		} build;
		struct Run
		{
			struct Port
			{
				uint16_t container = 0;
				std::string protocol;  // "tcp" / "udp"
			};

			std::filesystem::path workdir;
			std::string command;
			std::vector<std::string> args;
			std::vector<EnviromentVar> env;
			std::vector<Port> ports;
		} run;
		std::filesystem::path task_path;
	};
	Task game_server_task;

	std::string RegistryTagHeader = "";

	bool MultiNodeMode = false;
	std::string ManagerAdvertiseAddress;
	Log logger = Log("");
	void InitSwarm();
	void CreateTLS();
	void SendTLSToWorkers();
	void JoinWorkers();
	void SetupBuilder();
	void SetupNetwork();
	void SetupRegistry();
	void BuildImages();
	void BuildGameServer();
	void DeployStack();

	void BuildDockerImageLocally(const std::string& DockerFileContent,
								 const std::string& ImageName,std::filesystem::path workingDir = std::filesystem::current_path());
	void BuildDockerImageBuildX(const std::string& DockerFileContent, const std::string& ImageName,
								const std::unordered_set<std::string>& arches);

	static Task ParseTaskFile(std::filesystem::path file_path);

   public:
	void Run();
};