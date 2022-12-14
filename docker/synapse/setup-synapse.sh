#!/bin/sh

set -e

export DEBIAN_FRONTEND=noninteractive

apt-get update && apt-get -y install --no-install-recommends runit openssl

mkdir -p /data

SYNAPSE_SERVER_NAME=localhost SYNAPSE_REPORT_STATS=no /start.py generate

# yes, the empty line is needed
cat <<EOF >> /data/homeserver.yaml


enable_registration: true
enable_registration_without_verification: true

tls_certificate_path: "/data/synapse.tls.crt"
tls_private_key_path: "/data/synapse.tls.key"

rc_message:
  per_second: 10000
  burst_count: 100000

rc_registration:
  per_second: 10000
  burst_count: 30000

rc_login:
  address:
    per_second: 10000
    burst_count: 30000
  account:
    per_second: 10000
    burst_count: 30000
  failed_attempts:
    per_second: 10000
    burst_count: 30000

rc_admin_redaction:
  per_second: 1000
  burst_count: 5000

rc_joins:
  local:
    per_second: 10000
    burst_count: 100000
  remote:
    per_second: 10000
    burst_count: 100000

experimental_features:
  msc3266_enabled: true
EOF

sed -i 's/tls: false/tls: true/g;' data/homeserver.yaml

openssl req -x509 -newkey rsa:4096 -keyout data/synapse.tls.key -out data/synapse.tls.crt -days 365 -subj '/CN=synapse' -nodes
chmod 0777 data/synapse.tls.crt
chmod 0777 data/synapse.tls.key

# start synapse and create users
/start.py &

echo Waiting for synapse to start...
until curl -s -f -k https://localhost:8008/_matrix/client/versions; do echo "Checking ..."; sleep 2; done
echo Register alice
register_new_matrix_user --admin -u alice -p secret -c /data/homeserver.yaml https://localhost:8008
echo Register bob
register_new_matrix_user --admin -u bob -p secret -c /data/homeserver.yaml https://localhost:8008
echo Register carl
register_new_matrix_user --admin -u carl -p secret -c /data/homeserver.yaml https://localhost:8008

exit 0
