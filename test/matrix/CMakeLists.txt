# configure matrix tests

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

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.matrix-login" _in)
add_test(NAME cli_matrix_login COMMAND sh -c
  "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file ${CLI_EXECUTABLE}.matrix-login.log < ${_in}")
set_tests_properties(cli_matrix_login PROPERTIES
  PASS_REGULAR_EXPRESSION "Connecting to matrix server as @alice:localhost"
  LABELS "matrix")

add_test(NAME cli_matrix_login_grep COMMAND ${GREP} "recv parsed_messages"
         ${CMAKE_CURRENT_BINARY_DIR}/${CLI_EXECUTABLE}.matrix-login.log)
set_tests_properties(cli_matrix_login_grep PROPERTIES LABELS "matrix")

add_test(NAME synapse_server_stop
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/synapse_server.sh stop)
set_tests_properties(synapse_server_stop PROPERTIES LABELS "matrix")

set_property(TEST synapse_server_start PROPERTY FIXTURES_SETUP synapse)
set_tests_properties(cli_matrix_login cli_matrix_login_grep
                     PROPERTIES FIXTURES_REQUIRED synapse)
set_property(TEST synapse_server_stop PROPERTY FIXTURES_CLEANUP synapse)
