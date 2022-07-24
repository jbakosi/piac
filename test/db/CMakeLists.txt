# configure database tests

# start daemon in background, run cli tests, kill daemon
add_test(NAME daemon_db_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 55092 --rpc-port 65092)
set_tests_properties(daemon_db_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.55092" _in)
add_test(NAME cli_db_connect
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_connect PROPERTIES
                     PASS_REGULAR_EXPRESSION "Connected"
                     LABELS "db")

softlink( "${CMAKE_CURRENT_SOURCE_DIR}/docs.json" "${CMAKE_CURRENT_BINARY_DIR}" )
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.add_docs_json" _in)
add_test(NAME cli_db_add_docs_json
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_add_docs_json PROPERTIES
                     PASS_REGULAR_EXPRESSION "Added [02] entries"
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/entry.json" _out)
add_test(NAME generate_rnd_json_entry COMMAND sh -c
  "$<TARGET_FILE:rnd_json_entry> ${CMAKE_CURRENT_SOURCE_DIR} > ${_out}")
set_tests_properties(generate_rnd_json_entry PROPERTIES
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.add_rnd_json_entry" _in)
add_test(NAME cli_db_add_rnd_json_entry
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_add_rnd_json_entry PROPERTIES
                     PASS_REGULAR_EXPRESSION "Added 1 entries"
                     DEPENDS generate_rnd_json_entry
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.query" _in)
add_test(NAME cli_db_query
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_query PROPERTIES
                     PASS_REGULAR_EXPRESSION "results found"
                     DEPENDS cli_db_add_docs_json
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.hash" _in)
add_test(NAME cli_db_list_hash
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_list_hash PROPERTIES PASS_REGULAR_EXPRESSION
  "D0FA65B8B83584F8159708CCBF112E922C01353F9D5E6D8D7A09B513EC5BEA33.*[\r\n\t ]4B09CAD8AE7483A3173A0C8DBD38E4B03652881100DBD9C17C112DD55DA3C324"
  DEPENDS cli_db_add_docs_json
  LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.rm1" _in)
add_test(NAME cli_db_rm1
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_rm1 PROPERTIES
                     PASS_REGULAR_EXPRESSION "Removed 1 entries"
                     DEPENDS "cli_db_add_docs_json;cli_db_list_hash"
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.add_docs_json" _in)
add_test(NAME cli_db_add_back_docs_json
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_add_back_docs_json PROPERTIES
                     PASS_REGULAR_EXPRESSION "Added 1 entries"
                     DEPENDS "cli_db_add_docs_json;cli_db_rm1"
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.rm2" _in)
add_test(NAME cli_db_rm2
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_rm2 PROPERTIES
                     PASS_REGULAR_EXPRESSION "Removed 2 entries"
                     DEPENDS "cli_db_add_back_docs_json;cli_db_list_hash"
                     LABELS "db")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.rm1_noauth" _in)
add_test(NAME cli_db_rm1_noauth
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_rm1_noauth PROPERTIES
                     FAIL_REGULAR_EXPRESSION "Removed [^0] entries"
                     DEPENDS cli_db_add_docs_json
                     LABELS "db")

add_test(NAME kill_daemon_db COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
set_tests_properties(kill_daemon_db PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process\|Cannot open file"
                     LABELS "db")

set_property(TEST daemon_db_detach PROPERTY FIXTURES_SETUP daemon_db)
set_tests_properties(cli_db_connect
                     cli_db_add_docs_json
                     generate_rnd_json_entry
                     cli_db_add_rnd_json_entry
                     cli_db_query
                     cli_db_list_hash
                     cli_db_rm1
                     cli_db_rm2
                     cli_db_rm1_noauth
                     cli_db_add_back_docs_json
                     PROPERTIES FIXTURES_REQUIRED daemon_db)
set_property(TEST kill_daemon_db PROPERTY FIXTURES_CLEANUP daemon_db)
