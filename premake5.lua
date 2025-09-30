-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
    configurations { "DebugDocker","DebugLocal", "Release" }
    startproject "God"
    includedirs {"src"}
    pchheader "src/pch.hpp"
    pchsource "src/pch.cpp"
    location "build"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"
    --includedirs{"lib/GameNetworkingSockets/include","lib/glm","lib/json/single_include","lib"}
    --links{"GameNetworkingSockets_s","protobuf","crypto","ssl","curl"}
    --libdirs{"lib/GameNetworkingSockets/lib"}
    includedirs{"vcpkg_installed/x64-linux/include"}
    libdirs "vcpkg_installed/x64-linux/lib"
    links{"boost_container","curl","GameNetworkingSockets","GLEW","glfw3","glm","imgui"}
    
    filter "configurations:DebugDocker"
        symbols "On"
        defines {"_DOCKER"}

    filter "configurations:DebugLocal"
        defines {"_LOCAL",
        "_DISPLAY"}
        files { "lib/imgui/**.cpp" }
        --files{"lib/imgui/*.cpp"}
        --includedirs{"lib/imgui"}
        --files{"lib/imgui/backends/*.cpp"}
        symbols "On"        
        includedirs{"lib/glfw/include","lib/glew/include"}
        defines {"GLEW_STATIC"}
        links{"GLEW","glfw3","GL","X11"}
        libdirs{"lib/glfw/lib","lib/glew/lib"}
    filter "configurations:Release"
        defines {"_DOCKER"}
        optimize "On"

    project "KDNet"
        kind "SharedLib"
        language "C++"
        files { "src/**.cpp" }
    project "God"
        dependson "KDNet"
        links "KDNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/GodRun.cpp" }
        defines "_GOD"
   project "GodView"
        dependson "KDNet"
        links "KDNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/GodViewRun.cpp" }
        defines "_GODVIEW"
    project "Partition"
        dependson "KDNet"
        links "KDNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/PartitionRun.cpp" }
        defines "_PARTITION"

    project "SampleGame"
        dependson "KDNet"
        links "KDNet"
        kind "ConsoleApp"
        language "C++"
        files { "examples/SampleGame/**.cpp" }
        defines {"_GAMECLIENT","_GAMESERVER"}
function customClean()
    -- Specify the directories or files to be cleaned
    local dirsToRemove = {
        "bin",
        "obj",
        "Intermediate",
        ".cache",
        "build",
        "docker",
        "vcpkg",
        "vcpkg_installed",
        "docs"
        
    }
    local filesToRemove = {
        "Makefile",
        "imgui.ini",
        "compile_commands.json"
    }

    local extensionsToRemove = {
--        ".make",
    }
    -- Remove specified directories
    for _, dir in ipairs(dirsToRemove) do
        if os.isdir(dir) then
            os.rmdir(dir,{recursive = true})
            os.execute('rm -rf "' .. dir .. '"')
            print("Removed directory: " .. dir)
        end
    end

    -- Remove specified files
    for _, file in ipairs(filesToRemove) do
        if os.isfile(file) then
            os.remove(file)
            print("Removed file: " .. file)
        end
    end
        local rootFiles = os.matchfiles("*") -- only root files
    for _, file in ipairs(rootFiles) do
        for _, ext in ipairs(extensionsToRemove) do
            if file:sub(-#ext) == ext then
                os.remove(file)
                print("Removed file by extension: " .. file)
            end
        end
    end
end

-- Add the custom clean function to the clean action
newaction {
    trigger = "clean",
    description = "Custom clean action",
    execute = function()
        customClean()
    end
}
newaction 
{
    trigger = "setup",
    description = "Setups up dependencies",
    execute = function ()
        --local vcpkgDir = path.join(os.getenv("HOME") or ".", "vcpkg")
        local manifestDir = os.getcwd()
         os.execute("git clone https://github.com/microsoft/vcpkg.git ")
         os.execute("./vcpkg/bootstrap-vcpkg.sh")
         os.execute("./vcpkg/vcpkg install")
    end
}
newaction
{
    trigger = "GenDocs",
    description = "generate documentation",
    execute = function ()
        os.execute("doxygen Doxyfile")
    end

}