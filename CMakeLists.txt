# Build piac

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(piac C CXX)
include(ExternalProject)

# Set cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

# Set compiler var (will be exported to the source)
set(COMPILER "${CMAKE_CXX_COMPILER}")

# set version
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_PATCH "0")
set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# Set the requirement for the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Required language standard: C++${CMAKE_CXX_STANDARD}")

# Set build type (will be exported to the source)
set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
string(TOLOWER "${BUILD_TYPE}" BUILD_TYPE)

# Echo install prefix
message(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

# Echo number of PEs found
include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)
if(PROCESSOR_COUNT EQUAL 0)     # if ncpus could not be determined, assign 1
  set(PROCESSOR_COUNT "1")
endif()
message(STATUS "Found ${PROCESSOR_COUNT} PEs")

set(PIAC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

#### piac external libraries

# Set external libraries install directory
if (NOT TPL_DIR)
  set(REL_TPL_DIR ${PROJECT_SOURCE_DIR}/external/install/${COMP}-${ARCH})
  get_filename_component(ABS_TPL_DIR ${REL_TPL_DIR} ABSOLUTE)
  set(TPL_DIR ${ABS_TPL_DIR} CACHE
      STRING "Third-party (external) libraries install directory.")
endif()
message(STATUS "Third-party (external) libraries install directory: " ${TPL_DIR})

# Find monero
find_library(MONEROCPP_LIBRARIES libmonero-cpp.so ${TPL_DIR}/lib)

# Find threads
find_package(Threads REQUIRED)

# Find readline
find_package(Readline REQUIRED)

# Find zmqpp
set(zmqpp_dir ${TPL_DIR})
find_package(ZMQPP REQUIRED)

# Find Xapian
find_package(Xapian REQUIRED)

# Find Boost
find_package(Boost REQUIRED COMPONENTS filesystem serialization)

# Use monero's easylogging++
set(EASYLOGGINGPP_LIBRARIES ${TPL_DIR}/lib/libeasylogging.a
                            ${TPL_DIR}/lib/libepee.a
                            ${Boost_FILESYSTEM_LIBRARY})

# Find RapidJSON
find_package(RapidJSON REQUIRED)

# Find Crypto++
find_package(cryptopp REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

#### piac

add_library(db ${PIAC_SOURCE_DIR}/db.cpp
               ${PIAC_SOURCE_DIR}/jsonbase.cpp)
target_include_directories(db PUBLIC ${PIAC_SOURCE_DIR}
                                     ${TPL_DIR}/include
                                     ${RAPIDJSON_INCLUDE_DIRS}
                                     ${cryptopp_INCLUDE_DIRS})
set_target_properties(db PROPERTIES LIBRARY_OUTPUT_NAME piac_db)
install(TARGETS db
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

add_library(util ${PIAC_SOURCE_DIR}/util.cpp)
target_include_directories(util PUBLIC ${PIAC_SOURCE_DIR}
                                       ${TPL_DIR}/include
                                       ${cryptopp_INCLUDE_DIRS})
set_target_properties(util PROPERTIES LIBRARY_OUTPUT_NAME piac_util)
install(TARGETS util
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

# configure piac daemon executable
set(DAEMON_EXECUTABLE "piac-daemon")
add_executable(${DAEMON_EXECUTABLE}
               ${CMAKE_BINARY_DIR}/project_config.cpp
               ${PIAC_SOURCE_DIR}/daemon.cpp)
target_include_directories(${DAEMON_EXECUTABLE} PUBLIC
                           ${PIAC_SOURCE_DIR}
                           ${TPL_DIR}/include
                           ${ZMQPP_INCLUDE_DIRS}
                           ${PROJECT_BINARY_DIR})
target_link_libraries(${DAEMON_EXECUTABLE}
                      db util
                      ${XAPIAN_LIBRARIES}
                      ${ZMQPP_LIBRARIES}
                      ${EASYLOGGINGPP_LIBRARIES}
                      Threads::Threads
                      cryptopp::cryptopp)
install(TARGETS ${DAEMON_EXECUTABLE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
message(STATUS "Target ${DAEMON_EXECUTABLE} configured")

# configure piac cli executable
set(CLI_EXECUTABLE "piac-cli")
add_executable(${CLI_EXECUTABLE}
               ${CMAKE_BINARY_DIR}/project_config.cpp
               ${PIAC_SOURCE_DIR}/cli.cpp)
target_include_directories(${CLI_EXECUTABLE} PUBLIC
                           ${PIAC_SOURCE_DIR}
                           ${TPL_DIR}/include/utils
                           ${TPL_DIR}/include/common
                           ${TPL_DIR}/include/crypto
                           ${TPL_DIR}/include/storages
                           ${ZMQPP_INCLUDE_DIRS}
                           ${PROJECT_BINARY_DIR})
target_link_libraries(${CLI_EXECUTABLE} PRIVATE
                      util
                      ${ZMQPP_LIBRARIES}
                      ${Readline_LIBRARY}
                      ${Boost_SERIALIZATION_LIBRARY}
                      ${OPENSSL_CRYPTO_LIBRARIES}
                      ${EASYLOGGINGPP_LIBRARIES}
                      ${MONEROCPP_LIBRARIES}
                      cryptopp::cryptopp
                      Threads::Threads)
install(TARGETS ${CLI_EXECUTABLE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
message(STATUS "Target ${CLI_EXECUTABLE} configured")

#### Testing

option(ENABLE_TESTS "Enable tests" ON)
if (NOT ENABLE_TESTS)
  message(STATUS "Tests disabled")
else()
  enable_testing()
  add_subdirectory(${PROJECT_SOURCE_DIR}/test)
endif()

# Grab current date to stick into the executables
execute_process(COMMAND "date" "--utc" OUTPUT_VARIABLE BUILD_DATE)
string(REGEX REPLACE "[\r\n]" "" BUILD_DATE "${BUILD_DATE}")

# Configure cmake variables to pass to the build
configure_file( "${PIAC_SOURCE_DIR}/project_config.hpp.in"
                "${PROJECT_BINARY_DIR}/project_config.hpp" )
configure_file( "${PIAC_SOURCE_DIR}/project_config.cpp.in"
                "${PROJECT_BINARY_DIR}/project_config.cpp" )
