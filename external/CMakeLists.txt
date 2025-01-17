# Build piac external Libraries

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(piac-external C CXX)
include(ExternalProject)

# Set cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# Disallow in-source builds
include(DisallowInSourceBuilds)
# Set CMAKE_BUILD_TYPE
include(BuildType)
# Detect operating system type and version
include(DetectOS)
# Set BUILD_SHARED_LIBS
include(BuildShared)
# Sets CMAKE_INSTALL_{BIN,LIB}DIR needed for multilib support
include(GNUInstallDirs)
# Detect compiler version
include(DetectCompilerVersion)
# Get compiler flags
include(get_compiler_flags)
# Query target architecture
include(TargetArch)

# Set compiler id
string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" COMP)

# Set install directory based on compiler ID if was not given on command line
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install/${COMP}-${ARCH}"
      CACHE STRING "Third-party libraries install prefix." FORCE)
  message(STATUS "CMAKE_INSTALL_PREFIX not specified, setting to '${PROJECT_SOURCE_DIR}/install/${COMP}-${ARCH}'")
else()
  message(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})
endif()

# Echo number of PEs found
include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)
if(PROCESSOR_COUNT EQUAL 0)     # if ncpus could not be determined, assign 1
  set(PROCESSOR_COUNT "1")
endif()
message(STATUS "Found ${PROCESSOR_COUNT} PEs")

set(PIAC_CMAKE_PATH ${PROJECT_SOURCE_DIR}/../cmake)

# Configure build for libraries

# monero
set(MONERO_PATCH_COMMAND "patch;-p1;-t;-i;${PIAC_CMAKE_PATH}/monero-install-targets.patch")
ExternalProject_Add(
  monero
  PREFIX monero
  GIT_REPOSITORY https://github.com/monero-project/monero
  GIT_TAG v0.18.1.0
  GIT_SHALLOW 1
  PATCH_COMMAND ${MONERO_PATCH_COMMAND}
  CMAKE_ARGS
     -D CMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
     -D CMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
     -D CMAKE_BUILD_TYPE:PATH=${CMAKE_BUILD_TYPE}
     -D BUILD_SHARED_LIBS:BOOL=false
     -D CMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
     -D BUILD_GUI_DEPS:BOOL=true
     ${MONERO_ALPINE_DISABLE_STACK_TRACE}
  LOG_DOWNLOAD 1
  LOG_PATCH 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)

# monero-cpp
ExternalProject_Add(
  monero-cpp
  PREFIX monero-cpp
  DEPENDS monero
  GIT_REPOSITORY https://github.com/monero-ecosystem/monero-cpp
  GIT_TAG master
  GIT_SHALLOW 1
  CMAKE_ARGS
    -D CMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
    -D CMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
    -D CMAKE_BUILD_TYPE:PATH=${CMAKE_BUILD_TYPE}
    -D BUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -D CMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
    -D MONERO_PROJECT:STRING=${PROJECT_BINARY_DIR}/monero/src/monero
    -D MONERO_PROJECT_BUILD:STRING=${PROJECT_BINARY_DIR}/monero/src/monero-build
    -D BUILD_SCRATCHPAD:BOOL=off
    -D BUILD_SAMPLE:BOOL=off
  LOG_DOWNLOAD 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)

# zmqpp
ExternalProject_Add(
  zmqpp
  PREFIX zmqpp
  GIT_REPOSITORY https://github.com/zeromq/zmqpp.git
  GIT_TAG 4.2.0
  GIT_SHALLOW 1
  CMAKE_ARGS
     -D CMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
     -D CMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
     -D CMAKE_BUILD_TYPE:PATH=${CMAKE_BUILD_TYPE}
     -D BUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
     -D CMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  LOG_DOWNLOAD 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)

# mcss
ExternalProject_Add(
  mcss
  PREFIX mcss
  GIT_REPOSITORY https://github.com/mosra/m.css
  GIT_TAG 91ff035
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
                  ${PROJECT_BINARY_DIR}/mcss/src/mcss
                  ${CMAKE_INSTALL_PREFIX}/m.css
  LOG_DOWNLOAD 1
  LOG_INSTALL 1
)

# mtxclient
ExternalProject_Add(
  mtxclient
  PREFIX mtxclient
  GIT_REPOSITORY https://github.com/Nheko-Reborn/mtxclient.git
  GIT_TAG v0.8.2
  GIT_SHALLOW 1
  CMAKE_ARGS -D CMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
             -D CMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
             -D CMAKE_BUILD_TYPE:PATH=${CMAKE_BUILD_TYPE}
             -D BUILD_SHARED_LIBS:BOOL=false
             -D CMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
             -D BUILD_LIB_TESTS:BOOL=false
             -D USE_BUNDLED_COEURL=true
  LOG_DOWNLOAD 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)
