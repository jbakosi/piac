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
add_test(NAME cli_status_not_connected
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_status_not_connected PROPERTIES
                     PASS_REGULAR_EXPRESSION "Not connected"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.version" _in)
add_test(NAME cli_version
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${CLI_EXECUTABLE} v"
                     LABELS "zmq")

add_test(NAME cli_arg_version COMMAND ${CLI_EXECUTABLE} --version)
set_tests_properties(cli_arg_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${CLI_EXECUTABLE} v"
                     LABELS "zmq")

add_test(NAME cli_arg_help COMMAND ${CLI_EXECUTABLE} --help)
set_tests_properties(cli_arg_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "Show help message"
                     LABELS "zmq")

add_test(NAME cli_arg_bad COMMAND ${CLI_EXECUTABLE} asdf)
set_tests_properties(cli_arg_bad PROPERTIES
                     PASS_REGULAR_EXPRESSION "invalid option"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.help" _in)
add_test(NAME cli_help
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_help PROPERTIES
                     PASS_REGULAR_EXPRESSION "COMMANDS"
                     LABELS "zmq")

# configure helper executable to kill daemon
add_executable(kill_daemon kill_daemon.cpp)
# configure helper executable to grep logfile
add_executable(grep_log grep_log.cpp)

# start 2 daemons in background, run cli tests, kill daemons
# https://discourse.cmake.org/t/ctest-able-to-daemonize-fixtures/1012
add_test(NAME daemon_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 55091 --rpc-port 65091)
set_tests_properties(daemon_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon2_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 56091 --rpc-port 47091 --peer localhost:65091
         --log ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(daemon2_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon3_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --port 57091 --rpc-port 37091 --peer localhost:65091
         --log ${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(daemon3_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")

add_test(NAME daemon_grep
  COMMAND grep_log "Number of peers: 2" ${DAEMON_EXECUTABLE}.log)
set_tests_properties(daemon_grep PROPERTIES
                     PASS_REGULAR_EXPRESSION "Found:"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq")
add_test(NAME daemon2_grep
  COMMAND grep_log "Number of peers: 2" ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(daemon2_grep PROPERTIES
                     PASS_REGULAR_EXPRESSION "Found:"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq")
add_test(NAME daemon3_grep
  COMMAND grep_log "Number of peers: 2" ${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(daemon2_grep PROPERTIES
                     PASS_REGULAR_EXPRESSION "Found:"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.55091" _in)
add_test(NAME cli_connect
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_connect PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:47091|.*:37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.56091" _in)
add_test(NAME cli_connect2
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_connect2 PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.57091" _in)
add_test(NAME cli_connect3
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_connect3 PROPERTIES
  PASS_REGULAR_EXPRESSION
  "Connected.*[\r\n\t ].*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*47091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.status.connected" _in)
add_test(NAME cli_status_connected
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_status_connected PROPERTIES
                     PASS_REGULAR_EXPRESSION "Connected"
                     LABELS "zmq")

add_test(NAME kill_daemon_connect COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
set_tests_properties(kill_daemon_connect PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")
add_test(NAME kill_daemon2_connect
        COMMAND kill_daemon ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(kill_daemon2_connect PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")
add_test(NAME kill_daemon3_connect
        COMMAND kill_daemon ${DAEMON_EXECUTABLE}.37091.log)
set_tests_properties(kill_daemon3_connect PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "No such process|Cannot open file"
                     LABELS "zmq")

set_property(TEST daemon_detach daemon2_detach daemon3_detach PROPERTY
             FIXTURES_SETUP daemon_connect)
set_tests_properties(cli_connect cli_connect2 cli_connect3
                     daemon_grep daemon2_grep daemon3_grep cli_status_connected
                     PROPERTIES FIXTURES_REQUIRED daemon_connect)
set_property(TEST kill_daemon_connect kill_daemon2_connect kill_daemon3_connect
             PROPERTY FIXTURES_CLEANUP daemon_connect)
