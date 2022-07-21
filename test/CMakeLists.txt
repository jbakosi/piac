# piac testing

find_program(GREP grep REQUIRED)

# configure helper executable to kill daemon
add_executable(kill_daemon kill_daemon.cpp)
# configure helper executable to generate a random json db entry
add_executable(rnd_json_entry rnd_json_entry.cpp)

add_subdirectory(db)
add_subdirectory(zmq)
add_subdirectory(p2p)
