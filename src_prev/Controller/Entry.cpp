
#include "AtlasNetController.hpp"
#include <chrono>
#include <thread>
int main(int argc, char** argv)
{

  AtlasNet::AtlasNetController controller;
  controller.Init();

  return 0;
}