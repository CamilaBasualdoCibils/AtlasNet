#pragma once

#include "IServiceAdapter.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace AtlasNet
{

class KubernetesServiceAdapter : public IServiceAdapter
{
public:
  enum ImagePullPolicy
  {
    Always,
    IfNotPresent,
    Never
  };
  KubernetesServiceAdapter(
      const std::string_view& serviceName, const std::string_view& imageName,
      std::vector<std::pair<Network::PortType, Network::PortType>> portMappings, //internal/external
      ImagePullPolicy imagePullPolicy, const std::string& namespaceName = "");

  void Create() override;

  bool Exists() const override;

  void Destroy() override;

  void SetReplicaCount(uint32_t count) override;

  uint32_t GetReplicaCount() const override;

private:
  struct Response
  {
    int status;
    std::string body;
  };

  std::string _namespace;

  boost::asio::io_context _ioc;
  boost::asio::ssl::context _ctx;
  mutable boost::asio::ip::tcp::resolver _resolver;
  mutable boost::beast::ssl_stream<boost::beast::tcp_stream> _stream;

  std::string _token;
  ImagePullPolicy _imagePullPolicy;
std::vector<std::pair<Network::PortType, Network::PortType>> portMappings;
std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("K8sServiceAdapter");
private:
  std::string DetectNamespace();

  void LoadToken();

  void ConfigureTLS();

  Response Request(const std::string& method, const std::string& target,
                   const std::string& body,
                   const std::string& contentType = "application/json") const;

  _Json BuildStatefulSetJSON() const;
};

} // namespace AtlasNet