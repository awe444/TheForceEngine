# Install script for directory: /home/runner/work/TheForceEngine/TheForceEngine/TheForceEngine

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_A11y/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Archive/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Asset/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Audio/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_DarkForces/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_ExternalData/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_FileSystem/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_FrontEndUI/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Game/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Input/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Jedi/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Memory/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Outlaws/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Polygon/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_PostProcess/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_RenderShared/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_RenderBackend/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Settings/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_System/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_Ui/cmake_install.cmake")
  include("/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/TFE_ForceScript/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/TheForceEngine/TheForceEngine/build/TheForceEngine/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
