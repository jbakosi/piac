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

macro (add_compiler_flag flag)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endmacro (add_compiler_flag)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

  add_compiler_flag("-fcolor-diagnostics")
  add_compiler_flag("-Weverything")
  add_compiler_flag("-Wno-c++98-compat")
  add_compiler_flag("-Wno-exit-time-destructors")
  add_compiler_flag("-Wno-global-constructors")
  add_compiler_flag("-Wno-ctad-maybe-unsupported")

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

  add_compiler_flag("-fdiagnostics-color")
  add_compiler_flag("-Wall")
  add_compiler_flag("-Wextra")

endif()

# Echo compiler flags
message(STATUS "C compiler flags: '${CMAKE_C_FLAGS}'")
message(STATUS "C++ compiler flags: '${CMAKE_CXX_FLAGS}'")
message(STATUS "Executable linker flags: '${CMAKE_EXE_LINKER_FLAGS}'")

#### piac external libraries

# Set external libraries install directory
if (NOT TPL_DIR)
  set(REL_TPL_DIR ${PROJECT_SOURCE_DIR}/external/install/${COMP}-${ARCH})
  get_filename_component(ABS_TPL_DIR ${REL_TPL_DIR} ABSOLUTE)
  set(TPL_DIR ${ABS_TPL_DIR} CACHE
      STRING "Third-party (external) libraries install directory.")
endif()
message(STATUS "Third-party (external) libraries install directory: " ${TPL_DIR})
# Add our TPL_DIR to modules directory
set(CMAKE_PREFIX_PATH ${TPL_DIR} ${CMAKE_PREFIX_PATH})

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

# Find Python: required for code coverage (fastcov) and doc (m.css)
find_package(PythonInterp 3.6)

# Attempt to find tools required for code coverage analysis
include(DetectCodeCoverage)

# Set compiler flags to perform code coverage analysis
if(CODE_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage ")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

#### piac

set(EXECUTABLES)

add_library(db ${PIAC_SOURCE_DIR}/db.cpp)
target_include_directories(db PUBLIC ${PIAC_SOURCE_DIR}
                                     ${TPL_DIR}/include
                                     ${RAPIDJSON_INCLUDE_DIRS}
                                     ${cryptopp_INCLUDE_DIRS})
set_target_properties(db PROPERTIES LIBRARY_OUTPUT_NAME piac_db)
install(TARGETS db
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

add_library(document ${PIAC_SOURCE_DIR}/document.cpp
                     ${PIAC_SOURCE_DIR}/jsonbase.cpp)
target_include_directories(document PUBLIC ${PIAC_SOURCE_DIR}
                                           ${TPL_DIR}/include
                                           ${RAPIDJSON_INCLUDE_DIRS}
                                           ${cryptopp_INCLUDE_DIRS})
set_target_properties(document PROPERTIES LIBRARY_OUTPUT_NAME piac_document)
install(TARGETS document
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

add_library(logging_util ${PIAC_SOURCE_DIR}/logging_util.cpp)
target_include_directories(logging_util PUBLIC ${PIAC_SOURCE_DIR}
                                               ${TPL_DIR}/include)
set_target_properties(logging_util
                      PROPERTIES LIBRARY_OUTPUT_NAME piac_logging_util)
install(TARGETS logging_util
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

add_library(crypto_util ${PIAC_SOURCE_DIR}/crypto_util.cpp)
target_include_directories(crypto_util PUBLIC ${PIAC_SOURCE_DIR}
                                              ${cryptopp_INCLUDE_DIRS})
set_target_properties(crypto_util
                      PROPERTIES LIBRARY_OUTPUT_NAME piac_crypto_util)
install(TARGETS crypto_util
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)

add_library(string_util ${PIAC_SOURCE_DIR}/string_util.cpp)
target_include_directories(string_util PUBLIC ${PIAC_SOURCE_DIR})
set_target_properties(string_util
                      PROPERTIES LIBRARY_OUTPUT_NAME piac_string_util)
install(TARGETS string_util
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
                      db
                      document
                      logging_util
                      string_util
                      crypto_util
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
list(APPEND EXECUTABLES ${DAEMON_EXECUTABLE})

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
                      logging_util
                      crypto_util
                      string_util
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
list(APPEND EXECUTABLES ${CLI_EXECUTABLE})

#### Testing

option(ENABLE_TESTS "Enable tests" ON)
if (NOT ENABLE_TESTS)
  message(STATUS "Tests disabled")
else()
  enable_testing()
  add_subdirectory(${PROJECT_SOURCE_DIR}/test)
  list(APPEND EXECUTABLES kill_daemon rnd_json_entry pirate_server)
endif()

# Grab current date to stick into the executables
execute_process(COMMAND "date" "--utc" OUTPUT_VARIABLE BUILD_DATE)
string(REGEX REPLACE "[\r\n]" "" BUILD_DATE "${BUILD_DATE}")

execute_process(COMMAND "date" "+%A, %b %d, %Y"
                OUTPUT_VARIABLE BUILD_DATE_SIMPLE)
string(REGEX REPLACE "[\r\n]" "" BUILD_DATE_SIMPLE "${BUILD_DATE_SIMPLE}")

# Configure cmake variables to pass to the build
configure_file( "${PIAC_SOURCE_DIR}/project_config.hpp.in"
                "${PROJECT_BINARY_DIR}/project_config.hpp" )
configure_file( "${PIAC_SOURCE_DIR}/project_config.cpp.in"
                "${PROJECT_BINARY_DIR}/project_config.cpp" )

# configure zmq curve certificate generator executable
add_executable(curve_keygen ${PIAC_SOURCE_DIR}/curve_keygen.cpp)
list(APPEND EXECUTABLES curve_keygen)
target_include_directories(curve_keygen PUBLIC ${ZMQPP_INCLUDE_DIRS})
target_link_libraries(curve_keygen PRIVATE ${ZMQPP_LIBRARIES})

# Configure building documentation and web page
find_package(Doxygen 1.8.15)
find_package(MCSS)
if (DOXYGEN_FOUND AND MCSS_FOUND AND PYTHONINTERP_FOUND AND PYGMENTS_FOUND AND
    JINJA2_FOUND)

  find_program( PERL perl )
  find_program( AWK awk )
  find_program( XARGS xargs )
  find_program( HEAD head )

  if (PERL AND AWK AND XARGS AND HEAD)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/piac.doxy
                   ${CMAKE_CURRENT_BINARY_DIR}/doc/piac.doxy @ONLY)

    file(GLOB doc_pages CONFIGURE_DEPENDS
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "doc/pages/*.dox")
    foreach(page ${doc_pages})
      configure_file(${page} ${CMAKE_CURRENT_BINARY_DIR}/${page} @ONLY)
    endforeach()

    file(COPY doc/images DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/doc/html)

    add_custom_command(OUTPUT xml
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc
                       DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/piac.doxy
                       COMMAND ${DOXYGEN_EXECUTABLE} piac.doxy
                       COMMAND ${MCSS_DOX2HTML5} --no-doxygen piac.doxy
                       VERBATIM USES_TERMINAL)

    add_custom_target(doc DEPENDS xml
                      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc
                      COMMENT "Documentation")

    message(STATUS "Add target 'doc' to generate documentation to ./doc/html/index.html")

  endif()

endif()

# Setup code coverage targets
if(CODE_COVERAGE)

  # Determine number of CPUs available
  if (DEFINED ENV{SLURM_NPROCS})
    set(PROCESSOR_COUNT $ENV{SLURM_NPROCS})
  else()
    include(ProcessorCount)
    ProcessorCount(PROCESSOR_COUNT)
    if(PROCESSOR_COUNT EQUAL 0)     # if ncpus could not be determined, assign 1
      set(PROCESSOR_COUNT "1")
    endif()
  endif()
  message(STATUS "Found ${PROCESSOR_COUNT} PEs")

  include(CppCheck)
  include(CodeCoverage)
  setup_target_for_coverage(doc/html/${CMAKE_BUILD_TYPE} test_coverage
                            DEPENDS ${EXECUTABLES})

endif()
