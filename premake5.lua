workspace "HelloVulkan"
   location "build"
   configurations { "Debug", "Release" }

project "HelloVulkan"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"
   targetdir "bin/%{cfg.buildcfg}"
   files { "src/**.hpp", "src/**.cpp"}
   
   defines {"SPDLOG_COMPILED_LIB", "GLFW_INCLUDE_VULKAN", "GLM_FORCE_RADIANS", "GLM_FORCE_DEPTH_ZERO_TO_ONE"}
   includedirs { "external/include", "E:\\VulkanSDK\\1.3.290.0\\Include" }
   libdirs { "external/lib", "E:\\VulkanSDK\\1.3.290.0\\Lib"}
   links {"vulkan-1.lib", "glfw3", "spdlog" }
   linkoptions { "-g" }
   
   warnings "Extra"
   disablewarnings { "unused-variable", "unused-parameter", "unused-private-field" }

   filter "action:vs*"
      toolset "msc"

   filter "action:gmake*"
      toolset "gcc"
   
   filter "files:src/**.cpp"
      pchheader "src/pch.hpp"
   
   filter "system:windows"
      links {"gdi32", "user32", "shell32"}
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      optimize "Off"
   
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"


