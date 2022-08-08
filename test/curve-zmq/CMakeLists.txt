# configure network tests with curve-zmq

# start grasslands daemon in background, run cli tests, kill daemon
add_test(NAME daemon_zmq_grasslands_detach
         COMMAND ${DAEMON_EXECUTABLE} --detach --rpc-bind-port 23191 --p2p-bind-port 24191  --log-file grasslands-daemon.log)
set_tests_properties(daemon_zmq_grasslands_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq;curve")

add_test(NAME daemon_zmq_grasslands_grep
         COMMAND ${GREP} "Connections to this server are not secure" grasslands-daemon.log)
set_tests_properties(daemon_zmq_grasslands_grep PROPERTIES
                     LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.23191" _in)
add_test(NAME cli_zmq_grasslands_2_grasslands
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.grasslands_2_grasslands.log < ${_in}")
set_tests_properties(cli_zmq_grasslands_2_grasslands PROPERTIES
  PASS_REGULAR_EXPRESSION "No peers"
  FAIL_REGULAR_EXPRESSION "No response"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.23191" _in)
add_test(NAME cli_zmq_stonehouse_2_grasslands_need
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_grasslands_need --rpc-secure < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_grasslands_need PROPERTIES
  PASS_REGULAR_EXPRESSION "Need"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.23191" _in)
add_test(NAME cli_zmq_stonehouse_2_grasslands_cmd
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_grasslands_cmd --rpc-secure --rpc-server-public-key 'PTxe(9bIzZI6zr#8j:N0wo]%l.ltI57q/hORO{8V' < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_grasslands_cmd PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.23191" _in)
add_test(NAME cli_zmq_stonehouse_2_grasslands_file
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_grasslands_file --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_grasslands_file PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.23191" _in)
add_test(NAME cli_zmq_ironhouse_2_grasslands_file
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.ironhouse_2_grasslands_file --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub --rpc-client-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client.pub --rpc-client-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client < ${_in}")
set_tests_properties(cli_zmq_ironhouse_2_grasslands_file PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

add_test(NAME kill_daemon_zmq_grasslands
         COMMAND kill_daemon ${CMAKE_CURRENT_BINARY_DIR}/grasslands-daemon.log)
set_tests_properties(kill_daemon_zmq_grasslands PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq;curve")

set_property(TEST daemon_zmq_grasslands_detach
             PROPERTY FIXTURES_SETUP zmq_grasslands)
set_tests_properties(daemon_zmq_grasslands_grep
                     cli_zmq_grasslands_2_grasslands
                     cli_zmq_stonehouse_2_grasslands_cmd
                     cli_zmq_stonehouse_2_grasslands_file
                     cli_zmq_ironhouse_2_grasslands_file
                     PROPERTIES FIXTURES_REQUIRED zmq_grasslands)
set_property(TEST kill_daemon_zmq_grasslands
             PROPERTY FIXTURES_CLEANUP zmq_grasslands)


# start stonehouse daemons in background, run cli tests, kill daemons
add_test(NAME daemon_zmq_stonehouse_genkey_detach
         COMMAND ${DAEMON_EXECUTABLE} --detach --rpc-bind-port 22191 --p2p-bind-port 25191 --log-file stonehouse-genkey-daemon.log --rpc-secure --rpc-server-save-public-key-file stonehouse-genkey.pub)
set_tests_properties(daemon_zmq_stonehouse_genkey_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq;curve")

add_test(NAME daemon_zmq_stonehouse_genkey_grep
         COMMAND ${GREP} "Connections to this server are secure but not auth" stonehouse-genkey-daemon.log)
set_tests_properties(daemon_zmq_stonehouse_genkey_grep PROPERTIES
                     LABELS "zmq;curve")

add_test(NAME daemon_zmq_stonehouse_extkey_detach
         COMMAND ${DAEMON_EXECUTABLE} --detach --rpc-bind-port 21192 --p2p-bind-port 25192 --log-file stonehouse-extkey-daemon.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/stonehouse.server.pub --rpc-server-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/stonehouse.server)
set_tests_properties(daemon_zmq_stonehouse_extkey_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq;curve")

add_test(NAME daemon_zmq_stonehouse_extkey_grep
         COMMAND ${GREP} "Connections to this server are secure but not auth" stonehouse-extkey-daemon.log)
set_tests_properties(daemon_zmq_stonehouse_extkey_grep PROPERTIES
                     LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.22191" _in)
add_test(NAME cli_zmq_grasslands_2_stonehouse
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.grasslands_2_stonehouse.log < ${_in}")
set_tests_properties(cli_zmq_grasslands_2_stonehouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21192" _in)
add_test(NAME cli_zmq_stonehouse_2_stonehouse_cmd
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_stonehouse_cmd --rpc-secure --rpc-server-public-key '6teL]lh(4rIWF+zK4-CypnEyJl/3W*cG/C}3/OI#' < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_stonehouse_cmd PROPERTIES
  PASS_REGULAR_EXPRESSION "No peers"
  FAIL_REGULAR_EXPRESSION "No response"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.22191" _in)
add_test(NAME cli_zmq_stonehouse_2_stonehouse_file
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_stonehouse_file --rpc-secure --rpc-server-public-key-file stonehouse-genkey.pub < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_stonehouse_file PROPERTIES
  PASS_REGULAR_EXPRESSION "No peers"
  FAIL_REGULAR_EXPRESSION "No response"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.22191" _in)
add_test(NAME cli_zmq_stonehouse_2_stonehouse_wrong_pubkey
         COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_stonehouse_wrong_pubkey --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/stonehouse.server.pub < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_stonehouse_wrong_pubkey PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.22191" _in)
add_test(NAME cli_zmq_ironhouse_2_stonehouse_file
        COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.ironhouse_2_stonehouse_file --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub --rpc-client-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client.pub --rpc-client-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client < ${_in}")
set_tests_properties(cli_zmq_ironhouse_2_stonehouse_file PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

add_test(NAME kill_daemon_zmq_stonehouse_genkey
         COMMAND kill_daemon ${CMAKE_CURRENT_BINARY_DIR}/stonehouse-genkey-daemon.log)
set_tests_properties(kill_daemon_zmq_stonehouse_genkey PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq;curve")

add_test(NAME kill_daemon_zmq_stonehouse_extkey
         COMMAND kill_daemon ${CMAKE_CURRENT_BINARY_DIR}/stonehouse-extkey-daemon.log)
set_tests_properties(kill_daemon_zmq_stonehouse_extkey PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq;curve")

set_property(TEST daemon_zmq_stonehouse_genkey_detach
                  daemon_zmq_stonehouse_extkey_detach
             PROPERTY FIXTURES_SETUP zmq_stonehouse)
set_tests_properties(daemon_zmq_stonehouse_genkey_grep
                     daemon_zmq_stonehouse_extkey_grep
                     cli_zmq_grasslands_2_stonehouse
                     cli_zmq_stonehouse_2_stonehouse_cmd
                     cli_zmq_stonehouse_2_stonehouse_file
                     cli_zmq_stonehouse_2_stonehouse_wrong_pubkey
                     cli_zmq_ironhouse_2_stonehouse_file
                     PROPERTIES FIXTURES_REQUIRED zmq_stonehouse)
set_property(TEST kill_daemon_zmq_stonehouse_genkey
                  kill_daemon_zmq_stonehouse_extkey
             PROPERTY FIXTURES_CLEANUP zmq_stonehouse)


# start ironhouse daemon in background, run cli tests, kill daemon
add_test(NAME daemon_zmq_ironhouse_detach COMMAND ${DAEMON_EXECUTABLE}
         --detach --rpc-bind-port 21191 --p2p-bind-port 26191
         --log-file ironhouse-daemon.log --rpc-secure
         --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub
         --rpc-server-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server
         --rpc-authorized-clients-file ${CMAKE_CURRENT_SOURCE_DIR}/authorized_clients)
set_tests_properties(daemon_zmq_ironhouse_detach PROPERTIES
                     PASS_REGULAR_EXPRESSION "Forked PID"
                     LABELS "zmq;curve")

add_test(NAME daemon_zmq_ironhouse_grep
         COMMAND ${GREP} "Only authenticated connections are accepted"
         ${CMAKE_CURRENT_BINARY_DIR}/ironhouse-daemon.log)
set_tests_properties(daemon_zmq_ironhouse_grep PROPERTIES
                     LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_grasslands_2_ironhouse COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.grasslands_2_ironhouse.log < ${_in}")
set_tests_properties(cli_zmq_grasslands_2_ironhouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_stonehouse_2_ironhouse COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_ironhouse.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_ironhouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_stonehouse_2_ironhouse_wrong_pubkey COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.stonehouse_2_ironhouse_wrong_pubkey.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client.pub < ${_in}")
set_tests_properties(cli_zmq_stonehouse_2_ironhouse_wrong_pubkey PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_ironhouse_noauth_2_ironhouse COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.ironhouse_noauth_2_ironhouse.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub --rpc-client-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client3.pub --rpc-client-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client3 < ${_in}")
set_tests_properties(cli_zmq_ironhouse_noauth_2_ironhouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No response"
  FAIL_REGULAR_EXPRESSION "No peers"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_ironhouse_2_ironhouse COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.ironhouse_2_ironhouse.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub --rpc-client-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client.pub --rpc-client-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client < ${_in}")
set_tests_properties(cli_zmq_ironhouse_2_ironhouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No peers"
  FAIL_REGULAR_EXPRESSION "No response"
  LABELS "zmq;curve")

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cli.peers.21191" _in)
add_test(NAME cli_zmq_ironhouse_client2_2_ironhouse COMMAND sh -c "$<TARGET_FILE:${CLI_EXECUTABLE}> --log-file cli.ironhouse_client2_2_ironhouse.log --rpc-secure --rpc-server-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/ironhouse.server.pub --rpc-client-public-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client2.pub --rpc-client-secret-key-file ${CMAKE_CURRENT_SOURCE_DIR}/client2 < ${_in}")
set_tests_properties(cli_zmq_ironhouse_client2_2_ironhouse PROPERTIES
  PASS_REGULAR_EXPRESSION "No peers"
  FAIL_REGULAR_EXPRESSION "No response"
  LABELS "zmq;curve")

add_test(NAME kill_daemon_zmq_ironhouse
         COMMAND kill_daemon ${CMAKE_CURRENT_BINARY_DIR}/ironhouse-daemon.log)
set_tests_properties(kill_daemon_zmq_ironhouse PROPERTIES
                     PASS_REGULAR_EXPRESSION "Killing PID"
                     FAIL_REGULAR_EXPRESSION "Cannot open file"
                     LABELS "zmq;curve")

set_property(TEST daemon_zmq_ironhouse_detach
             PROPERTY FIXTURES_SETUP zmq_ironhouse)
set_tests_properties(daemon_zmq_ironhouse_grep
                     cli_zmq_grasslands_2_ironhouse
                     cli_zmq_stonehouse_2_ironhouse
                     cli_zmq_stonehouse_2_ironhouse_wrong_pubkey
                     cli_zmq_ironhouse_noauth_2_ironhouse
                     cli_zmq_ironhouse_2_ironhouse
                     cli_zmq_ironhouse_client2_2_ironhouse
                     PROPERTIES FIXTURES_REQUIRED zmq_ironhouse)
set_property(TEST kill_daemon_zmq_ironhouse
             PROPERTY FIXTURES_CLEANUP zmq_ironhouse)
