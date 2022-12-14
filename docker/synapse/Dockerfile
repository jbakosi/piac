# vim: filetype=dockerfile:
#
# Generate synapse matrix server service docker image for CI
# See also .woodpecker.yml
#
# Usage:
#
# docker build -f Dockerfile -t piac-synapse .
# docker-squash --cleanup piac-synapse -t jbakosi/piac-synapse
# docker push jbakosi/piac-synapse

FROM matrixdotorg/synapse:v1.73.0

COPY setup-synapse.sh /setup-synapse.sh
COPY entrypoint.sh /entrypoint.sh
COPY service /service

RUN /setup-synapse.sh

ENTRYPOINT ["/entrypoint.sh"]

EXPOSE 8008
