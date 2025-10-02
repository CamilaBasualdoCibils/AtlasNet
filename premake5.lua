-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
    configurations { "DebugDocker", "DebugLocal", "Release" }
    startproject "God"
    includedirs { "src" }

    location "build"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"

    filter "system:windows"
        includedirs { "vcpkg_installed/x64-windows/include" }
        libdirs { "vcpkg_installed/x64-windows/lib" }

    filter "system:linux"
        includedirs { "vcpkg_installed/x64-linux/include" }
        libdirs { "vcpkg_installed/x64-linux/lib" }

    filter {}     -- clear filter so it doesn’t leak into other settings
    
    links { "boost_container", "curl", "GameNetworkingSockets", "GLEW", "glfw3", "glm", "imgui","implot","implot3d","GL" }

    filter "configurations:DebugDocker"
        symbols "On"
        defines { "_DOCKER" }

    filter "configurations:DebugLocal"
        defines { "_LOCAL",
        "_DISPLAY" }
        files { "lib/imgui/**.cpp" }
        symbols "On"
        includedirs { "lib/glfw/include", "lib/glew/include" }
        defines { "GLEW_STATIC" }
        links { "GLEW", "glfw3", "GL", "X11" }
        libdirs { "lib/glfw/lib", "lib/glew/lib" }
    filter "configurations:Release"
        defines { "_DOCKER" }
        optimize "On"

    project "AtlasNet"
        kind "StaticLib"
        language "C++"
        files { "src/**.cpp" }
        pchheader "src/pch.hpp"
        pchsource "src/pch.cpp"
    project "God"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GodRun.cpp" }
        defines "_GOD"
    project "GodView"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GodViewRun.cpp" }
        defines "_GODVIEW"
    project "Partition"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/PartitionRun.cpp" }
        defines "_PARTITION"

    project "SampleGame"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "examples/SampleGame/**.cpp" }
        defines { "_GAMECLIENT", "_GAMESERVER" }

-- Generic cleanup function
function customClean(dirsToRemove, filesToRemove)
    -- Remove specified directories
    for _, dir in ipairs(dirsToRemove or {}) do
        if os.isdir(dir) then
            os.rmdir(dir)
            os.execute('rm -rf "' .. dir .. '"')
            print("Removed directory: " .. dir)
        end
    end

    -- Remove specified files
    for _, file in ipairs(filesToRemove or {}) do
        if os.isfile(file) then
            os.remove(file)
            print("Removed file: " .. file)
        end
    end
end

function CleanBin()
    local dirs = { "bin", "obj", "build", "docker" };
    local files = {}
    customClean(dirs, files)
end

function CleanDeps()
    local dirs = { "vcpkg", "vcpkg_installed" };
    local files = {}
    customClean(dirs, files)
end

function CleanDocs()
    local dirs = { "docs" };
    local files = {}
    customClean(dirs, files)
end

-- Add the custom clean function to the clean action
newaction {
    trigger = "CleanBin",
    description = "Custom clean action",
    execute = function()
        CleanBin()
    end
}
newaction {
    trigger = "CleanDeps",
    description = "Custom clean action",
    execute = function()
        CleanDeps()
    end
}
newaction {
    trigger = "CleanDocs",
    description = "Custom clean action",
    execute = function()
        CleanDocs()
    end
}
newaction {
    trigger = "CleanAll",
    description = "Custom clean action",
    execute = function()
        CleanBin()
        CleanDeps()
        CleanDocs()
    end
}
newaction
{
    trigger = "setup",
    description = "Setups up dependencies",
    execute = function()
        local manifestDir = os.getcwd()
        local isWindows = package.config:sub(1, 1) == '\\' -- check if path separator is '\'

        os.execute("git clone https://github.com/microsoft/vcpkg.git")

        if isWindows then
            os.execute("vcpkg\\bootstrap-vcpkg.bat")
            os.execute("vcpkg\\vcpkg install")
        else
            os.execute("./vcpkg/bootstrap-vcpkg.sh")
            os.execute("./vcpkg/vcpkg install")
        end
    end

}
newaction
{
    trigger = "GenDocs",
    description = "generate documentation",
    execute = function()
        os.execute("doxygen Doxyfile")
    end

}
function BuildDockerImage(buildTarget, macros)
    -- Simple Dockerfile template with macros
    local BaseTemplateBuild = [[
FROM ubuntu:25.04

WORKDIR /app

RUN apt-get update && apt-get install git curl zip unzip coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev -y


RUN git clone https://github.com/microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_DEFAULT_TRIPLET=x64-linux
ENV VCPKG_FEATURE_FLAGS=manifests
COPY vcpkg.json /app

RUN ./vcpkg/vcpkg install
ENV LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux/lib

RUN git clone https://github.com/premake/premake-core.git
WORKDIR /app/premake-core
RUN make -f Bootstrap.mak linux

WORKDIR /app

# COPY premake5 /app
COPY premake5.lua /app
COPY vcpkg.json /app
RUN ./premake-core/bin/release/premake5 setup

COPY examples /app
COPY src /app/src
COPY srcRun /app/srcRun
RUN ./premake-core/bin/release/premake5 gmake
RUN make -C ./build ${BUILD_TARGET} config=${BUILD_CONFIG} -j $(nproc)



RUN ${BUILD_CMD}

CMD ["${RUN_CMD}"]
]]
local BaseTemplate = [[
FROM ubuntu:25.04

WORKDIR /app

RUN apt-get update && apt-get install git curl zip unzip tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev -y

ENV LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux/lib
COPY vcpkg_installed/ /app/vcpkg_installed/

${COPY_AND_RUN_CMDS}
]]

    
    -- Replace macros in template
    for key, value in pairs(macros) do
        BaseTemplateBuild = BaseTemplateBuild:gsub("%${" .. key .. "}", value)
    end

    local dockerFilePath = "docker/dockerfile." .. string.lower(buildTarget)
    -- Write to file
    local f = io.open(dockerFilePath, "w")
    if f then
        f:write(BaseTemplateBuild)
        f:close()
        print("Generated Dockerfile at " .. dockerFilePath)
    else
        print("Failed to open " .. dockerFilePath)
    end
    os.execute("docker build -f "..dockerFilePath.." -t ".. string.lower(buildTarget)..":latest .")
end

function BuildDockerImageFromTarget(target,build_config,app_bundle)
     
    if target == "God" or target == "Demigod"or target == "GameCoordinator" then
         BuildF(string.lower(build_config),target)
    BuildDockerImage(target, {
                    BUILD_TARGET = target,
            BUILD_CONFIG = string.lower(build_config),
        COPY_AND_RUN_CMDS = [[COPY bin/]] .. build_config .."/".. target.."/"..target.." /app\n" ..
        "ENTRYPOINT /app/"..target
    })
    elseif target == "Partition" then
      
        BuildF(string.lower(build_config),target .." ".. app_bundle)
        BuildDockerImage(target, {
            BUILD_TARGET = target,
            BUILD_CONFIG = string.lower(build_config),
        COPY_AND_RUN_CMDS = "COPY bin/" .. build_config .."/".. target.."/"..target.." /app\n" ..
        "COPY bin/" .. build_config .."/".. target.."/"..target.." /app\n" ..
        "ENTRYPOINT /app/"..target
    })
    end
end
newaction {
    trigger = "build-docker-image",
    description = "generate documentation",
    execute = function()
        local target = _OPTIONS["target"] or "INVALID"
        local build_config = _OPTIONS["build_config"] or "Release"
        local app_bundle = _OPTIONS["app-bundle"] or "INVALID"
        BuildDockerImageFromTarget(target,build_config,app_bundle)
    end
}
newaction {
    trigger = "run-docker-image",
    description = "generate documentation",
    execute = function()
        local target = _OPTIONS["target"] or "INVALID"
        local build_config = _OPTIONS["build_config"] or "Release"
        local app_bundle = _OPTIONS["app-bundle"] or "INVALID"
        BuildDockerImageFromTarget(target,build_config,app_bundle)
        os.execute("docker rm -f " .. target .. " >/dev/null 2>&1; ")
        local ok, _, code = os.execute("docker run --init -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name "..target.." -d -p 1234:1234 "..string.lower(target))
        if not ok or code ~= 0 then
            error("(exit code: " .. tostring(code) .. ")", 2)
        end
    end
}
function BuildF(build_config,target)
    local pok, _, pcode = os.execute("./premake5 gmake")
    if not pok or pcode ~= 0 then
        error("(exit code: " .. tostring(pcode) .. ")", 2)
    end
    local handle = io.popen("nproc --all")
    local result = handle:read("*a")
    handle:close()
    local cores = tonumber(result:match("%d+"))
    print("Compiling in " .. cores .. " cores")
    local mok, _, mcode = os.execute("make -C build config=" .. (build_config) .." ".. (target) .. " -j "..cores)
    if not mok or mcode ~= 0 then
        error("(exit code: " .. tostring(mcode) .. ")", 2)
    end
end
newaction {
    trigger = "Build",
    description = "build",
    execute = function()
         BuildF(string.lower(_OPTIONS["build_config"] or "Release"),(_OPTIONS["target"] or ""))
    end
}
newoption {
    trigger     = "target",
    value       = "NAME",
    description = "Specify which target to generate for"
}
newoption {
    trigger     = "build_config",
    value       = "Release",
    description = "Specify which build to generate for"
}
newoption {
    trigger = "app-bundle",
    value = "Invalid",
    description = "Specify the app to bundle with partition"
}