#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "GL/Texture.hpp"
#include "Debug/Log.hpp"
class Cartograph : public Singleton<Cartograph>
{
    GLFWwindow *_glfwwindow;
    std::shared_ptr<Log> logger = std::make_shared<Log>("Cartograph");
    void Startup();
    void DrawBackground();

    std::string IP2God;
    void DrawConnectTo();

    void DrawMap();
    void DrawPartitionGrid();
    void DrawLog();

    void Update();
    void Render();
    static std::shared_ptr<Texture> LoadTextureFromCompressed(const void *data, size_t size);

public:
    Cartograph() {}
    void Run();
};