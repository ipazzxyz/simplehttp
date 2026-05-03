#include <netdb.h>
#include <openssl/err.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <server.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <threadpool.hpp>
namespace simplehttp {
Server::Server(
    int port, int backlog, int thread_pool_size,
    std::map<std::string, std::function<std::string(std::string_view)>> routes)
    : port(port),
      backlog(backlog),
      thread_pool(thread_pool_size),
      routes(routes) {
  {
    addrinfo* res;
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int rv;
    if ((rv = getaddrinfo("localhost", std::to_string(port).c_str(), &hints,
                          &res)) != 0) {
      std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
      exit(1);
    }
    if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
      std::cerr << "socket: " << strerror(errno) << std::endl;
      exit(1);
    }
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      std::cerr << "setsockopt: " << strerror(errno) << std::endl;
      exit(1);
    }
    if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
      std::cerr << "bind: " << strerror(errno) << std::endl;
      exit(1);
    }
    freeaddrinfo(res);
  }
  if (listen(fd, backlog) < 0) {
    std::cerr << "listen: " << strerror(errno) << std::endl;
    exit(1);
  }
}
Server::~Server() {
  close(fd);
  SSL_CTX_free(ctx);
}
void Server::InitSSL(std::string_view cert_path, std::string_view key_path) {
  if (SSL_load_error_strings() == 0) {
    std::cerr << "SSL_load_error_strings: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(1);
  }
  if (OpenSSL_add_all_algorithms() == 0) {
    std::cerr << "OpenSSL_add_all_algorithms: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(1);
  }
  if (SSL_library_init() == 0) {
    std::cerr << "SSL_library_init: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(1);
  }
  if (!(ctx = SSL_CTX_new(TLS_server_method()))) {
    std::cerr << "SSL_CTX_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    exit(1);
  }
  if (SSL_CTX_use_certificate_file(ctx, cert_path.data(), SSL_FILETYPE_PEM) <=
      0) {
    std::cerr << "SSL_CTX_use_certificate_file: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(1);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, key_path.data(), SSL_FILETYPE_PEM) <=
      0) {
    std::cerr << "SSL_CTX_use_PrivateKey_file: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    exit(1);
  }
}
void Server::AddRoute(std::string path,
                      std::function<std::string(std::string_view)> handler) {
  routes[path] = handler;
}
void Server::Loop() {
  while (true) {
    int client_fd = accept(fd, nullptr, nullptr);
    if (client_fd < 0) continue;
    SSL* ssl = nullptr;
    if (ctx) {
      ssl = SSL_new(ctx);
      SSL_set_fd(ssl, client_fd);
      if (SSL_accept(ssl) <= 0) {
        SSL_free(ssl);
        close(client_fd);
        continue;
      }
    }
    thread_pool.enqueue(std::bind(&Server::HandleClient, this, ssl, client_fd));
  }
}
void Server::HandleClient(SSL* ssl, int client_fd) {
  std::string request = Read(ssl, client_fd);
  size_t first_space = request.find(' '),
         second_space = request.find(' ', first_space + 1);
  std::string path = "/";
  if (first_space != std::string::npos && second_space != std::string::npos) {
    path = request.substr(first_space + 1, second_space - first_space - 1);
  }
  std::string response_body;
  std::string status = "200 OK";
  if (routes.count(path)) {
    response_body = routes[path](request);
  } else {
    response_body = ReadFile("www" + path);
    if (response_body.empty()) {
      response_body = "<h1>404 Not Found</h1>";
      status = "404 Not Found";
    }
  }
  std::string header = "HTTP/1.1 " + status +
                       "\r\n"
                       "Content-Length: " +
                       std::to_string(response_body.length()) +
                       "\r\n"
                       "Connection: close\r\n\r\n";
  Send(ssl, client_fd, header + response_body);
  if (ssl) {
    SSL_shutdown(ssl);
  }
  close(client_fd);
  if (ssl) {
    SSL_free(ssl);
  }
}
std::string Server::Read(SSL* ssl, int fd) {
  char buffer[1024];
  ssize_t received;
  if (ssl) {
    received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
  } else {
    received = recv(fd, buffer, sizeof(buffer) - 1, 0);
  }
  if (received < 0) {
    std::cerr << "SSL_read: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return {};
  }
  buffer[received] = '\0';
  return std::string(buffer, received);
}
void Server::Send(SSL* ssl, int fd, std::string_view data) {
  size_t total_sent = 0;
  while (total_sent < data.size()) {
    ssize_t sent;
    if (ssl) {
      sent = SSL_write(ssl, data.data() + total_sent, data.size() - total_sent);
    } else {
      sent = send(fd, data.data() + total_sent, data.size() - total_sent, 0);
    }
    if (sent < 0) {
      std::cerr << "SSL_write: " << ERR_error_string(ERR_get_error(), nullptr)
                << std::endl;
      return;
    }
    total_sent += sent;
  }
}
std::string Server::ReadFile(std::string_view path) const {
  std::ifstream file(path.data());
  if (!file.is_open()) return {};
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}
}  // namespace simplehttp