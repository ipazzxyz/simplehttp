#pragma once
#include <openssl/ssl.h>

#include <map>
#include <string>
#include <string_view>
class ThreadPool;
namespace simplehttp {
class Server {
 public:
  Server(int port, int backlog,
         int thread_pool_size = std::thread::hardware_concurrency(),
         std::map<std::string, std::function<std::string(std::string_view)>>
             routes = {});
  ~Server();
  void InitSSL(std::string_view cert_path, std::string_view key_path);
  void AddRoute(std::string path,
                std::function<std::string(std::string_view)> handler);
  void Loop();

 private:
  int port;
  int backlog;
  int fd;
  SSL_CTX* ctx;
  ThreadPool thread_pool;
  std::map<std::string, std::function<std::string(std::string_view)>> routes;
  void HandleClient(SSL* ssl, int client_fd);
  std::string Read(SSL* ssl, int fd);
  void Send(SSL* ssl, int fd, std::string_view data);
  std::string ReadFile(std::string_view path) const;
};
}  // namespace simplehttp