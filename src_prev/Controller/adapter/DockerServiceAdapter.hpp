#pragma once

#include "adapter/IServiceAdapter.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <stdexcept>

namespace AtlasNet
{
class DockerServiceAdapter : public IServiceAdapter
{
public:
  DockerServiceAdapter(const std::string_view& DockerSocketPath, const std::string_view& serviceName,
                       const std::string_view& imageName,
                       std::vector<std::pair<Network::PortType, Network::PortType>> portMappings)
      : IServiceAdapter(serviceName, imageName),
        _DockerSocketPath(DockerSocketPath),
        _portMappings(std::move(portMappings))
  {
  }
  void Create() override;
  bool Exists() const override;
  void Destroy() override;
  void SetReplicaCount(uint32_t count) override;
  uint32_t GetReplicaCount() const override;

private:
  struct Response
  {
    int status;
    _Json body;
  };

  void EnsureConnected() const
  {
    if (m_socket.is_open())
    {
      return;
    }

    m_socket.connect(
        boost::asio::local::stream_protocol::endpoint(_DockerSocketPath));
  }

  void Disconnect() const
  {
    if (!m_socket.is_open())
    {
      return;
    }

    boost::system::error_code ec;
    m_socket.shutdown(
        boost::asio::local::stream_protocol::socket::shutdown_both, ec);
    m_socket.close(ec);
  }

  Response Request(const std::string& method, const std::string& target,
                   const std::string& body = "",
                   const std::string& contentType = "") const
  {
    namespace http = boost::beast::http;
    namespace beast = boost::beast;

    http::verb verb = http::string_to_verb(method);
    if (verb == http::verb::unknown)
    {
      throw std::runtime_error("Unsupported HTTP method: " + method);
    }

    std::function<Response(bool)> doRequest = [&](bool retry) -> Response
    {
      try
      {
        EnsureConnected();

        http::request<http::string_body> req{verb, target, 11};
        req.set(http::field::host, "localhost");
        req.set(http::field::user_agent, "AtlasNet");
        req.set(http::field::accept, "application/json");
        req.keep_alive(true);

        if (!contentType.empty())
        {
          req.set(http::field::content_type, contentType);
        }

        req.body() = body;
        req.prepare_payload();

        http::write(m_socket, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(m_socket, buffer, res);

        if (!res.keep_alive() || res.need_eof())
        {
          Disconnect();
        }

        _Json responseBody = _Json::object();

        if (!res.body().empty())
        {
          try
          {
            responseBody = _Json::parse(res.body());
          }
          catch (...)
          {
            responseBody = _Json{{"raw", res.body()}};
          }
        }

        return Response{static_cast<int>(res.result_int()),
                        std::move(responseBody)};
      }
      catch (const boost::system::system_error&)
      {
        Disconnect();

        if (!retry)
        {
          throw;
        }

        return doRequest(false);
      }
    };

    return doRequest(true);
  }

private:
  std::string _DockerSocketPath;
  // namespace asio = boost::asio;
  std::vector<std::pair<Network::PortType, Network::PortType>> _portMappings;
  mutable boost::asio::io_context m_io;
  mutable boost::asio::local::stream_protocol::socket m_socket{m_io};
};
} // namespace AtlasNet