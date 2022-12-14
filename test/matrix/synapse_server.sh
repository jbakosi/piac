#!/bin/bash

# This is used to spin up a synapse matrix server for local (i.e., non-CI)
# testing. Usage: synapse_server.sh <start/stop>.

die () { echo >&2 "$@"; exit 1; }
[ "$#" -eq 1 ] || die "Exactly 1 argument required: start/stop"

SYNAPSE_IMAGE="matrixdotorg/synapse:v1.73.0"

if [[ "$1" == "start" ]]; then

  # start synapse
  mkdir -p data
  chmod 0777 data
  docker run -v `pwd`/data:/data --rm -e SYNAPSE_SERVER_NAME=localhost -e SYNAPSE_REPORT_STATS=no ${SYNAPSE_IMAGE} generate
  ./adjust-config.sh
  docker run -d \
         --name synapse \
         -p 443:8008 \
         -p 8448:8008 \
         -p 8008:8008 \
         -v `pwd`/data:/data ${SYNAPSE_IMAGE}
  echo Waiting for synapse to start...
  until curl -s -f -k https://localhost:443/_matrix/client/versions; do echo "Checking ..."; sleep 2; done
  echo Register alice
  docker exec synapse /bin/sh -c 'register_new_matrix_user --admin -u alice -p secret -c /data/homeserver.yaml https://localhost:8008'
  echo Register bob
  docker exec synapse /bin/sh -c 'register_new_matrix_user --admin -u bob -p secret -c /data/homeserver.yaml https://localhost:8008'
  echo Register carl
  docker exec synapse /bin/sh -c 'register_new_matrix_user --admin -u carl -p secret -c /data/homeserver.yaml https://localhost:8008'

elif [[ "$1" == "stop" ]]; then

  # stop any running instance of synapse
  rm -rf data
  docker rm -f synapse 2>&1>/dev/null

fi
