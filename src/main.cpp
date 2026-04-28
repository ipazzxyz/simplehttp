#include <iostream>
#include <server.hpp>
int main() {
  Server server(8080, 10, std::thread::hardware_concurrency());
  server.AddRoute("/hello", [](std::string_view request) {
    return "<h1>Hello, World!</h1>";
  });
  server.AddRoute("/time", [](std::string_view request) {
    time_t now = time(nullptr);
    return "<h1>Current Time: " + std::string(ctime(&now)) + "</h1>";
  });
  server.Loop();
  return 0;
}