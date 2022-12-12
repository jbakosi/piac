# piac testing

find_program(GREP grep REQUIRED)

function(softlink target link_name)
  set(LN_COMMAND "ln -sf ${target} ${link_name}")
  exec_program(${LN_COMMAND} OUTPUT_VARIABLE ln_output RETURN_VALUE ln_retval)
  if ("${ln_retval}" GREATER 0)
    message(FATAL_ERROR "Problem creating symbolic link from \"${target}\" to \"${link_name}\":\n${ln_output}")
    endif()
endfunction()

# configure helper executable to kill daemon
add_executable(kill_daemon kill_daemon.cpp)

# configure helper executable to generate a random json db entry
add_executable(rnd_json_entry rnd_json_entry.cpp)

add_subdirectory(db)
add_subdirectory(zmq)
add_subdirectory(curve-zmq)
add_subdirectory(p2p)
add_subdirectory(monero)
add_subdirectory(matrix)
