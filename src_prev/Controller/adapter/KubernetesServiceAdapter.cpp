#include "KubernetesServiceAdapter.hpp"
#include "atlasnet/core/CoreDefs.hpp"

void AtlasNet::KubernetesServiceAdapter::Create()
{
  auto body = BuildStatefulSetJSON();

  auto res = Request("POST",
                     "/apis/apps/v1/namespaces/" + _namespace + "/statefulsets",
                     body.dump());
  logger->info("Create service adapter response: {} - {}", res.status,
               res.body);

  if (res.status != 201)
  {
    logger->error("Failed to create service adapter for {} in namespace {}. "
                  "Response: {} - {}",
                  _serviceName, _namespace, res.status, res.body);
  }
}
AtlasNet::KubernetesServiceAdapter::KubernetesServiceAdapter(
    const std::string_view& serviceName, const std::string_view& imageName,
    std::vector<std::pair<Network::PortType, Network::PortType>> portMappings,
    ImagePullPolicy imagePullPolicy, const std::string& namespaceName)
    : IServiceAdapter(serviceName, imageName),
      portMappings(std::move(portMappings)),
      _namespace(namespaceName.empty() ? DetectNamespace() : namespaceName),
      _ctx(boost::asio::ssl::context::tlsv12_client), _resolver(_ioc),
      _stream(_ioc, _ctx), _imagePullPolicy(imagePullPolicy)
{
  LoadToken();
  ConfigureTLS();
}
bool AtlasNet::KubernetesServiceAdapter::Exists() const
{
  auto res = Request("GET",
                     "/apis/apps/v1/namespaces/" + _namespace +
                         "/statefulsets/" + _serviceName,
                     "");
  logger->info("Check service adapter existence response: {} - {}", res.status,
               res.body);
  if (res.status == 404)
  {
    logger->info("Service adapter {} does not exist in namespace {}",
                 _serviceName, _namespace);
  }
  else if (res.status != 200)
  {
    logger->error("Failed to check existence of service adapter for {} in "
                  "namespace {}. Assuming it does not exist.",
                  _serviceName, _namespace);
  }
  return res.status == 200;
}
void AtlasNet::KubernetesServiceAdapter::Destroy()
{
  auto res = Request("DELETE",
                     "/apis/apps/v1/namespaces/" + _namespace +
                         "/statefulsets/" + _serviceName,
                     "");

  logger->info("Destroy service adapter response: {} - {}", res.status,
               res.body);
  if (res.status != 200 && res.status != 204)
  {
    logger->error("Failed to destroy service adapter for {} in namespace {}",
                  _serviceName, _namespace);
  }
  (void)res;
}
void AtlasNet::KubernetesServiceAdapter::SetReplicaCount(uint32_t count)
{
  _Json patch = {{"spec", {{"replicas", count}}}};

  auto res = Request("PATCH",
                     "/apis/apps/v1/namespaces/" + _namespace +
                         "/statefulsets/" + _serviceName,
                     patch.dump(), "application/merge-patch+json");
  logger->info("SetReplicaCount response: {} - {}", res.status, res.body);
  if (res.status != 200)
  {
    logger->error(
        "Failed to set replica count for service adapter {} in namespace {}",
        _serviceName, _namespace);
  }
  (void)res;
}
uint32_t AtlasNet::KubernetesServiceAdapter::GetReplicaCount() const
{
  auto res = Request("GET",
                     "/apis/apps/v1/namespaces/" + _namespace +
                         "/statefulsets/" + _serviceName,
                     "");
  logger->info("GetReplicaCount response: {} - {}", res.status, res.body);
  if (res.status != 200)
    return 0;

  _Json j = _Json::parse(res.body);
  return j["spec"]["replicas"].get<uint32_t>();
}
std::string AtlasNet::KubernetesServiceAdapter::DetectNamespace()
{
  std::ifstream f("/var/run/secrets/kubernetes.io/serviceaccount/namespace");

  std::string ns;

  if (f.good())
    std::getline(f, ns);

  logger->info("Detected namespace: {}", ns);
  if (ns.empty())
    return "default";

  return ns;
}
void AtlasNet::KubernetesServiceAdapter::LoadToken()
{
  std::ifstream f("/var/run/secrets/kubernetes.io/serviceaccount/token");

  if (f.good())
    std::getline(f, _token);
  if (_token.empty())
  {
    logger->error("Failed to load Kubernetes token from file, service adapter "
                  "will not be able to authenticate with the Kubernetes API");
  }
  else
  {
    logger->info("Successfully loaded Kubernetes token from file");
  }
}
void AtlasNet::KubernetesServiceAdapter::ConfigureTLS()
{
  _ctx.set_verify_mode(boost::asio::ssl::verify_none); // replace in prod
}
AtlasNet::KubernetesServiceAdapter::Response
AtlasNet::KubernetesServiceAdapter::Request(
    const std::string& method, const std::string& target,
    const std::string& body, const std::string& contentType) const
{
  try
  {
    const std::string host = "kubernetes.default.svc";
    const std::string port = "443";

    auto const results = _resolver.resolve(host, port);

    boost::beast::get_lowest_layer(_stream).connect(results);

    _stream.handshake(boost::asio::ssl::stream_base::client);

    boost::beast::http::request<boost::beast::http::string_body> req;

    req.method_string(method);
    req.target(target);
    req.version(11);

    req.set(boost::beast::http::field::host, host);

    req.set(boost::beast::http::field::authorization, "Bearer " + _token);

    req.set(boost::beast::http::field::content_type, contentType);

    if (!body.empty())
      req.body() = body;

    req.prepare_payload();

    boost::beast::http::write(_stream, req);

    boost::beast::flat_buffer buffer;

    boost::beast::http::response<boost::beast::http::string_body> res;

    boost::beast::http::read(_stream, buffer, res);

    boost::beast::error_code ec;
    _stream.shutdown(ec);

    return {(int)res.result_int(), res.body()};
  }
  catch (const std::exception&)
  {
    return {0, ""};
  }
}
AtlasNet::_Json AtlasNet::KubernetesServiceAdapter::BuildStatefulSetJSON() const
{
  return {
      {"apiVersion", "apps/v1"},
      {"kind", "StatefulSet"},
      {"metadata",
       {
           {"name", _serviceName},
           {"namespace", _namespace},
       }},
      {"spec",
       {
           {"serviceName", _serviceName},
           {"replicas", 1},
           {"selector",
            {
                {"matchLabels",
                 {
                     {"app", _serviceName},
                 }},
            }},
           {"template",
            {
                {"metadata",
                 {
                     {"labels",
                      {
                          {"app", _serviceName},
                      }},
                 }},
                {"spec",
                 {
                     {"containers",
                      {{
                          {"name", _serviceName},
                          {"image", _imageName},
                          {"imagePullPolicy",
                           _imagePullPolicy == Always         ? "Always"
                           : _imagePullPolicy == IfNotPresent ? "IfNotPresent"
                                                              : "Never"},
                          {"env", _Json::array({{{"name", "NODE_IP"},
                                                 {"valueFrom",
                                                  {{"fieldRef",
                                                    {{"fieldPath",
                                                      "status.podIP"}}}}}}})}
                          //{"ports",
                          // {{{"containerPort", 7777}}}},
                      }}},
                 }},
            }},
       }}};
}
