# configure monero tests

## start daemon in background, run cli tests, kill daemon
#add_test(NAME daemon_monero_detach COMMAND ${DAEMON_EXECUTABLE}
#         --detach --rpc-bind-port 45092)
#set_tests_properties(daemon_monero_detach PROPERTIES
#                     PASS_REGULAR_EXPRESSION "Forked PID"
#                     LABELS "monero")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.balance" _in)
add_test(NAME cli_monero_balance
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> < ${_in}")
set_tests_properties(cli_monero_balance PROPERTIES
                     PASS_REGULAR_EXPRESSION "Wallet sync height:"
                     LABELS "monero")

#add_test(NAME kill_daemon_monero COMMAND kill_daemon ${DAEMON_EXECUTABLE}.log)
#set_tests_properties(kill_daemon_monero PROPERTIES
#                     PASS_REGULAR_EXPRESSION "Killing PID"
#                     FAIL_REGULAR_EXPRESSION "No such process\|Cannot open file"
#                     LABELS "monero")
#
#set_property(TEST daemon_monero_detach PROPERTY FIXTURES_SETUP daemon_monero)
#set_tests_properties(cli_monero_balance
#                     PROPERTIES FIXTURES_REQUIRED daemon_monero)
#set_property(TEST kill_daemon_monero PROPERTY FIXTURES_CLEANUP daemon_monero)
