# configure matrix tests

if (NOT DEFINED ENV{CI})
  # configure synapse target for matrix tests
  find_program(DOCKER docker REQUIRED)
  find_program(PERL perl REQUIRED)
  softlink( "${CMAKE_CURRENT_SOURCE_DIR}/adjust-config.sh"
            "${CMAKE_CURRENT_BINARY_DIR}" )
  # start matrix server in background, run cli tests, kill daemon
  add_test(NAME synapse_server_start
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/synapse_server.sh start)
  set_tests_properties(synapse_server_start PROPERTIES
                       PASS_REGULAR_EXPRESSION "Success!"
                       LABELS "matrix")
  set(SYNAPSE_SERVER_HOSTNAME "localhost")
else()
  set(SYNAPSE_SERVER_HOSTNAME "synapse")
endif()


configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cli.matrix-login"
                "${CMAKE_CURRENT_BINARY_DIR}/cli.matrix-login" )

file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/cli.matrix-login" _in)
add_test(NAME cli_matrix_login COMMAND sh -c
  "$<TARGET_FILE:${CLI_EXECUTABLE}> --matrix-sync-timeout 1000 --log-file ${CLI_EXECUTABLE}.matrix-login.log < ${_in}")
set_tests_properties(cli_matrix_login PROPERTIES
  PASS_REGULAR_EXPRESSION "Connecting to matrix server as @alice:${SYNAPSE_SERVER_HOSTNAME}"
  LABELS "matrix")

add_test(NAME cli_matrix_login_grep COMMAND ${GREP} "recv parsed_messages"
         ${CMAKE_CURRENT_BINARY_DIR}/${CLI_EXECUTABLE}.matrix-login.log)
set_tests_properties(cli_matrix_login_grep PROPERTIES
                     DEPENDS cli_matrix_login LABELS "matrix")


configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cli.invite_room"
                "${CMAKE_CURRENT_BINARY_DIR}/cli.invite_room" )

file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/cli.invite_room" _in)
add_test(NAME cli_matrix_invite_room COMMAND sh -c
  "$<TARGET_FILE:${CLI_EXECUTABLE}> --matrix-sync-timeout 1000 --log-file ${CLI_EXECUTABLE}.invite_room.log < ${_in}")
set_tests_properties(cli_matrix_invite_room PROPERTIES
  PASS_REGULAR_EXPRESSION "Connecting to matrix server as @alice:${SYNAPSE_SERVER_HOSTNAME}"
  LABELS "matrix")

add_test(NAME cli_matrix_invite_room_grep COMMAND ${GREP} "alice created room"
         ${CMAKE_CURRENT_BINARY_DIR}/${CLI_EXECUTABLE}.invite_room.log)
set_tests_properties(cli_matrix_invite_room_grep PROPERTIES
                     DEPENDS cli_matrix_invite_room LABELS "matrix")


configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cli.join_room"
                "${CMAKE_CURRENT_BINARY_DIR}/cli.join_room" )

file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/cli.join_room" _in)
add_test(NAME cli_matrix_join_room COMMAND sh -c
  "$<TARGET_FILE:${CLI_EXECUTABLE}> --matrix-sync-timeout 1000 --log-file ${CLI_EXECUTABLE}.join_room.log < ${_in}")
set_tests_properties(cli_matrix_join_room PROPERTIES
  PASS_REGULAR_EXPRESSION "Connecting to matrix server as @bob:${SYNAPSE_SERVER_HOSTNAME}"
  DEPENDS cli_matrix_invite_room LABELS "matrix")

add_test(NAME cli_matrix_join_room_grep COMMAND ${GREP} "joining room"
         ${CMAKE_CURRENT_BINARY_DIR}/${CLI_EXECUTABLE}.join_room.log)
set_tests_properties(cli_matrix_join_room_grep PROPERTIES
                     DEPENDS cli_matrix_join_room LABELS "matrix")


if (NOT DEFINED ENV{CI})
  add_test(NAME synapse_server_stop
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/synapse_server.sh stop)
  set_tests_properties(synapse_server_stop PROPERTIES LABELS "matrix")
  set_property(TEST synapse_server_start PROPERTY FIXTURES_SETUP synapse)
  set_tests_properties(cli_matrix_login cli_matrix_login_grep
                       cli_matrix_invite_room cli_matrix_invite_room_grep
                       cli_matrix_join_room cli_matrix_join_room_grep
                       PROPERTIES FIXTURES_REQUIRED synapse)
  set_property(TEST synapse_server_stop PROPERTY FIXTURES_CLEANUP synapse)
endif()
