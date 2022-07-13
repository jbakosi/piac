# - Try to find zmqpp
# Once done this will define
# ZMQPP_FOUND - System has zmqpp
# ZMQPP_INCLUDE_DIRS - The zmqpp include directories
# ZMQPP_LIBRARIES - The libraries needed to use zmqpp

find_path ( ZMQPP_INCLUDE_DIR zmqpp/zmqpp.hpp HINTS ${zmqpp_dir}/include )
find_library ( ZMQPP_LIBRARY NAMES zmqpp  HINTS ${zmqpp_dir}/lib )
find_library ( ZMQ_LIBRARY NAMES zmq )

set ( ZMQPP_LIBRARIES ${ZMQPP_LIBRARY} ${ZMQ_LIBRARY} )
set ( ZMQPP_INCLUDE_DIRS ${ZMQPP_INCLUDE_DIR} )

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set ZMQPP_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args ( ZMQPP DEFAULT_MSG ZMQPP_LIBRARIES ZMQPP_INCLUDE_DIRS )
