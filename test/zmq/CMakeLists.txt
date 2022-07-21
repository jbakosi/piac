# configure network tests with zmq

# daemon tests that exit immediately
add_test(NAME daemon_arg_help COMMAND ${DAEMON_EXECUTABLE} --help)
set_tests_properties(daemon_arg_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "Show help message"
                     LABELS "zmq")

add_test(NAME daemon_arg_version COMMAND ${DAEMON_EXECUTABLE} --version)
set_tests_properties(daemon_arg_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${DAEMON_EXECUTABLE} v"
                     LABELS "zmq")

add_test(NAME daemon_arg_bad COMMAND ${DAEMON_EXECUTABLE} asdf)
set_tests_properties(daemon_arg_bad PROPERTIES
                     PASS_REGULAR_EXPRESSION "invalid option"
                     LABELS "zmq")

# cli tests that don't require daemon
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.status.not_connected" _in)
add_test(NAME cli_zmq_status_not_connected
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_status_not_connected PROPERTIES
                     PASS_REGULAR_EXPRESSION "Not connected"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.version" _in)
add_test(NAME cli_zmq_version
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${CLI_EXECUTABLE} v"
                     LABELS "zmq")

add_test(NAME cli_zmq_arg_version COMMAND ${CLI_EXECUTABLE} --version)
set_tests_properties(cli_zmq_arg_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${CLI_EXECUTABLE} v"
                     LABELS "zmq")

add_test(NAME cli_zmq_arg_help COMMAND ${CLI_EXECUTABLE} --help)
set_tests_properties(cli_zmq_arg_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "Show help message"
                     LABELS "zmq")

add_test(NAME cli_zmq_arg_bad COMMAND ${CLI_EXECUTABLE} asdf)
set_tests_properties(cli_zmq_arg_bad PROPERTIES
                     PASS_REGULAR_EXPRESSION "invalid option"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.help" _in)
add_test(NAME cli_zmq_help
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "COMMANDS"
                     LABELS "zmq")

# start 3 daemons in background, run cli tests, kill daemons
# https://discourse.cmake.org/t/ctest-able-to-daemonize-fixtures/1012
add_test(NAME daemon_zmq_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 55091 --rpc-port 65091)
set_tests_properties(daemon_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon2_zmq_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 56091 --rpc-port 47091 --peer localhost:65091
         --log ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(daemon2_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon3_zmq_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 57091 --rpc-port 37091 --peer localhost:65091
         --log ${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(daemon3_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")

add_test(NAME wait4comm_zmq COMMAND sleep 1)
set_tests_properties(wait4comm_zmq PROPERTIES LABELS "zmq")

add_test(NAME daemon_zmq_grep COMMAND ${GREP} "Number of peers: 2"
         ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.log)
set_tests_properties(daemon_zmq_grep PROPERTIES
                     DEPENDS wait4comm_zmq LABELS "zmq")
add_test(NAME daemon2_zmq_grep COMMAND ${GREP} "Number of peers: 2"
        ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(daemon2_zmq_grep PROPERTIES
                     DEPENDS wait4comm_zmq LABELS "zmq")
add_test(NAME daemon3_zmq_grep COMMAND ${GREP} "Number of peers: 2"
         ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(daemon2_zmq_grep PROPERTIES
                     DEPENDS wait4comm_zmq LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.55091" _in)
add_test(NAME cli_zmq
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:47091|.*:37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.56091" _in)
add_test(NAME cli2_zmq
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli2_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.57091" _in)
add_test(NAME cli3_zmq
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli3_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*47091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.status.connected" _in)
add_test(NAME cli_zmq_status_connected
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_status_connected PROPERTIES
                     PASS_REGULAR_EXPRESSION "Connected"
                     LABELS "zmq")

add_test(NAME kill_daemon_zmq COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
set_tests_properties(kill_daemon_zmq PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")
add_test(NAME kill_daemon2_zmq
        COMMAND kill_daemon ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(kill_daemon2_zmq PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")
add_test(NAME kill_daemon3_zmq
        COMMAND kill_daemon ${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(kill_daemon3_zmq PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")

set_property(TEST daemon_zmq_detach daemon2_zmq_detach daemon3_zmq_detach
             PROPERTY FIXTURES_SETUP daemon_zmq)
set_tests_properties(daemon_zmq_grep daemon2_zmq_grep daemon3_zmq_grep
                     wait4comm_zmq cli_zmq cli2_zmq cli3_zmq
                     cli_zmq_status_connected
                     PROPERTIES FIXTURES_REQUIRED daemon_zmq)
set_property(TEST kill_daemon_zmq kill_daemon2_zmq kill_daemon3_zmq
             PROPERTY FIXTURES_CLEANUP daemon_zmq)
