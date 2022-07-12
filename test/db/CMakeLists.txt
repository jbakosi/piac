# configure database tests

# configure helper executable to generate a random json db entry
add_executable(rnd_json_entry rnd_json_entry.cpp)

# start daemon in background, run cli tests, kill daemon
add_test(NAME daemon_db_detach
         COMMAND ${DAEMON_EXECUTABLE} --detach --port 55092)
set_tests_properties(daemon_db_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.55092" _in)
add_test(NAME cli_db_connect
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_connect PROPERTIES
                     PASS_REGULAR_EXPRESSION "Connected")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/entry.json" _out)
add_test(NAME generate_json_entry COMMAND
  sh -c "$<TARGET_FILE:rnd_json_entry> ${CMAKE_CURRENT_SOURCE_DIR} > ${_out}")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.add_json" _in)
add_test(NAME cli_db_add_json_entry
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_add_json_entry PROPERTIES
                     PASS_REGULAR_EXPRESSION "Added 1 entries")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.query" _in)
add_test(NAME cli_db_query
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_query PROPERTIES
                     PASS_REGULAR_EXPRESSION "results found")

add_test(NAME kill_daemon_db COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
set_tests_properties(kill_daemon_db PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID")

set_property(TEST daemon_db_detach PROPERTY FIXTURES_SETUP daemon_db)
set_tests_properties(cli_db_connect PROPERTIES FIXTURES_REQUIRED daemon_db)
set_tests_properties(generate_json_entry PROPERTIES FIXTURES_REQUIRED daemon_db)
set_tests_properties(cli_db_add_json_entry
                     PROPERTIES FIXTURES_REQUIRED daemon_db
                     DEPENDS generate_json_entry)
set_tests_properties(cli_db_query
                     PROPERTIES FIXTURES_REQUIRED daemon_db
                     DEPENDS cli_db_add_json_entry)
set_property(TEST kill_daemon_db PROPERTY FIXTURES_CLEANUP daemon_db)
