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

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.version" _in)
add_test(NAME cli_zmq_version
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_version PROPERTIES
                     PASS_REGULAR_EXPRESSION "${CLI_EXECUTABLE} v"
                     LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.badcmd" _in)
add_test(NAME cli_zmq_badcmd
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_zmq_badcmd PROPERTIES
                     PASS_REGULAR_EXPRESSION "unknown cmd"
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
         --detach --rpc-bind-port 55091 --p2p-bind-port 65091)
set_tests_properties(daemon_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon2_zmq_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --rpc-bind-port 56091 --p2p-bind-port 47091 --peer localhost:65091
         --log-file ${DAEMON_EXECUTABLE}.47091.log)
set_tests_properties(daemon2_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq")
add_test(NAME daemon3_zmq_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --rpc-bind-port 57091 --p2p-bind-port 37091 --peer localhost:65091
         --log-file ${DAEMON_EXECUTABLE}.37091.log)
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
  ".*peers.*[\r\n\t ].*[\r\n\t ]|.*:47091|.*:37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.56091" _in)
add_test(NAME cli2_zmq
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli2_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION
  ".*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*37091"
  LABELS "zmq")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.connect.57091" _in)
add_test(NAME cli3_zmq
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli3_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION
  ".*peers.*[\r\n\t ].*[\r\n\t ]|.*:65091|.*47091"
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
                     PROPERTIES FIXTURES_REQUIRED daemon_zmq)
set_property(TEST kill_daemon_zmq kill_daemon2_zmq kill_daemon3_zmq
             PROPERTY FIXTURES_CLEANUP daemon_zmq)


# configure helper executable to test Lazy Pirate client
add_executable(pirate_server pirate_server.cpp)
target_include_directories(pirate_server PUBLIC ${PIAC_SOURCE_DIR}
                                                ${ZMQPP_INCLUDE_DIRS})
target_link_libraries(pirate_server
  PRIVATE logging_util ${ZMQPP_LIBRARIES} ${EASYLOGGINGPP_LIBRARIES})

# start a pirate server daemon in background, run cli tests, kill daemon
add_test(NAME pirate_server_zmq_detach COMMAND pirate_server --detach)
set_tests_properties(pirate_server_zmq_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq;pirate")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.pirate.5555" _in)
add_test(NAME cli_pirate_zmq COMMAND sh -c
  "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file piac-cli-pirate.log < ${_in}")
set_tests_properties(cli_pirate_zmq PROPERTIES
  PASS_REGULAR_EXPRESSION "No response from|peers" LABELS "zmq;pirate")

add_test(NAME pirate_cli_zmq_grep COMMAND
  ${GREP} "Destroyed socket" piac-cli-pirate.log)
set_tests_properties(pirate_cli_zmq_grep PROPERTIES
                     DEPENDS cli_pirate_zmq LABELS "zmq;pirate")

add_test(NAME pirate_server_zmq_grep
         COMMAND ${GREP} "Normal" pirate_server.log)
set_tests_properties(pirate_server_zmq_grep PROPERTIES
                     DEPENDS pirate_cli_zmq_grep LABELS "zmq;pirate")

add_test(NAME kill_pirate_server_zmq
         COMMAND kill_daemon pirate_server.log)
set_tests_properties(kill_pirate_server_zmq PROPERTIES
                     LABELS "zmq;pirate"
                     PASS_REGULAR_EXPRESSION "Killing PID|No such process"
                     FAIL_REGULAR_EXPRESSION "Cannot open file")
add_test(NAME pirate_zmq_clean_log COMMAND
  ${CMAKE_COMMAND} -E remove pirate_server.log piac-cli-pirate.log)
set_tests_properties(pirate_zmq_clean_log PROPERTIES
  DEPENDS "kill_pirate_server_zmq;pirate_server_zmq_grep;pirate_cli_zmq_grep"
  LABELS "zmq;pirate")

set_property(TEST pirate_server_zmq_detach
             PROPERTY FIXTURES_SETUP pirate_server_zmq)
set_tests_properties(cli_pirate_zmq pirate_server_zmq_grep pirate_cli_zmq_grep
                     PROPERTIES FIXTURES_REQUIRED pirate_server_zmq)
set_property(TEST kill_pirate_server_zmq pirate_zmq_clean_log
             PROPERTY FIXTURES_CLEANUP pirate_server_zmq)
