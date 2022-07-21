# configure p2p tests

# daemon tests that exit immediately
add_test(NAME daemon_arg_help COMMAND ${DAEMON_EXECUTABLE} --help)
set_tests_properties(daemon_arg_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "Show help message"
                     LABELS "p2p")

add_test(NAME daemon_arg_version COMMAND ${DAEMON_EXECUTABLE} --version)
set_tests_properties(daemon_arg_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${DAEMON_EXECUTABLE} v"
                     LABELS "p2p")

add_test(NAME daemon_arg_bad COMMAND ${DAEMON_EXECUTABLE} asdf)
set_tests_properties(daemon_arg_bad PROPERTIES
                     PASS_REGULAR_EXPRESSION "invalid option"
                     LABELS "p2p")

# start 3 daemons in background, run cli tests, kill daemons
# https://discourse.cmake.org/t/ctest-able-to-daemonize-fixtures/1012
add_test(NAME daemon_p2p_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 36091 --rpc-port 35091)
set_tests_properties(daemon_p2p_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "p2p")
add_test(NAME daemon2_p2p_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 33091 --rpc-port 27091 --peer localhost:35091
         --log ${DAEMON_EXECUTABLE}.27091.log --db daemon2)
set_tests_properties(daemon2_p2p_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "p2p")
add_test(NAME daemon3_p2p_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 23091 --rpc-port 17091 --peer localhost:35091
         --log ${DAEMON_EXECUTABLE}.17091.log --db daemon3)
set_tests_properties(daemon3_p2p_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "p2p")


file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/entry.json" _out)
add_test(NAME generate_rnd_json_entry_p2p COMMAND sh -c
  "$<TARGET_FILE:rnd_json_entry> ${CMAKE_CURRENT_SOURCE_DIR}/../db > ${_out}")
set_tests_properties(generate_rnd_json_entry_p2p PROPERTIES
                     LABELS "p2p")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.add_rnd_json_entry" _in)
add_test(NAME cli_db_add_rnd_json_entry_p2p
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_db_add_rnd_json_entry_p2p PROPERTIES
                     PASS_REGULAR_EXPRESSION "Added 1 entries"
                     DEPENDS generate_rnd_json_entry_p2p
                     LABELS "p2p")

add_test(NAME wait4comm_p2p COMMAND sleep 1)
set_tests_properties(wait4comm_p2p PROPERTIES
                     DEPENDS cli_db_add_rnd_json_entry_p2p
                     LABELS "p2p")

add_test(NAME daemon_p2p_grep COMMAND ${GREP} "\[Nn\]umber of documents: [1-9]"
         ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.log)
set_tests_properties(daemon_p2p_grep PROPERTIES
                     DEPENDS wait4comm_p2p
                     LABELS "p2p")
add_test(NAME daemon2_p2p_grep COMMAND ${GREP} "[Nn]umber of documents: [1-9]"
        ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.27091.log)
set_tests_properties(daemon2_p2p_grep PROPERTIES
                     DEPENDS wait4comm_p2p
                     LABELS "p2p")
add_test(NAME daemon3_p2p_grep COMMAND ${GREP} "[Nn]umber of documents: [1-9]"
         ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.17091.log)
set_tests_properties(daemon3_p2p_grep PROPERTIES
                     DEPENDS wait4comm_p2p
                     LABELS "p2p")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.status.connected" _in)
add_test(NAME cli_status_connected
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_status_connected PROPERTIES
                     PASS_REGULAR_EXPRESSION "Connected"
                     LABELS "p2p")

add_test(NAME kill_daemon_p2p COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
set_tests_properties(kill_daemon_p2p PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "p2p")
add_test(NAME kill_daemon2_p2p COMMAND kill_daemon
         ${DAEMON_EXECUTABLE}.27091.log)
set_tests_properties(kill_daemon2_p2p PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "p2p")
add_test(NAME kill_daemon3_p2p
        COMMAND kill_daemon ${DAEMON_EXECUTABLE}.17091.log)
set_tests_properties(kill_daemon3_p2p PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "p2p")

set_property(TEST daemon_p2p_detach daemon2_p2p_detach daemon3_p2p_detach
             PROPERTY FIXTURES_SETUP daemon_p2p)
set_tests_properties(generate_rnd_json_entry_p2p cli_db_add_rnd_json_entry_p2p
                     daemon_p2p_grep daemon2_p2p_grep daemon3_p2p_grep
                     PROPERTIES FIXTURES_REQUIRED daemon_p2p)
set_property(TEST kill_daemon_p2p kill_daemon2_p2p kill_daemon3_p2p
             PROPERTY FIXTURES_CLEANUP daemon_p2p)
