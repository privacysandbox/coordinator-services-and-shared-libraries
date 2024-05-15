#!/bin/bash

# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

################################################################################
# Certificate setup
################################################################################

# Create a cert for the backend service to be able to talk HTTPS.
# Google backends must use SSL when talking HTTP/2.
# This cert is not added for security, since SSL termination does not happen
# at the instance level, but just to be able to register an HTTP/2 server as
# a backend.
mkdir -p ${host_certificate_path};
openssl genrsa 2048 > ${host_certificate_path}/privatekey.pem;
openssl req -new -key ${host_certificate_path}/privatekey.pem -out ${host_certificate_path}/csr.pem -subj "/C=US/ST=WA/O=SCP/CN=scp.privacy-sandbox.com";
openssl x509 -req -days 7305 -in ${host_certificate_path}/csr.pem -signkey ${host_certificate_path}/privatekey.pem -out ${host_certificate_path}/public.crt;
rm ${host_certificate_path}/csr.pem;

################################################################################
# Logging setup
################################################################################

# Enables custom container log streams
# This ensures correct file formatting
logging_config_path="/etc/stackdriver/logging.config.d/fluentd-pbs-container-logs.conf"
echo "<source>" >> $logging_config_path
echo "  @type tail" >> $logging_config_path
echo "  <parse>" >> $logging_config_path
echo "    @type multi_format" >> $logging_config_path
echo "    <pattern>" >> $logging_config_path
echo "      format regexp" >> $logging_config_path
echo "      expression /^(?<time>[^ ]* {1,2}[^ ]* [^ ]*) (?<host>[^ ]*) (?<ident>[^ :\[]*)(?:\[(?<pid>[0-9]+)\])?(?:[^\:]*\:)? *(?<severity>(DEBUG|INFO|NOTICE|WARNING|ERROR))\|(?<cluster_name>[^ |]*)\|(?<machine_name>[^ |]*)\|(?<component_name>[^ |]*)\|(?<correlation_id>[^ |]*)\|(?<spanId>[^ |]*)\|(?<trace>[^ |]*)\|(?<sourceLocation>[^ \:|]*):(?<method_name>[^ \:]*):(?<line_number>[0-9]*)\|(?<message>.*)$/" >> $logging_config_path
echo "      time_format %b %d %H:%M:%S" >> $logging_config_path
echo "    </pattern>" >> $logging_config_path
echo "    <pattern>" >> $logging_config_path
echo "      format syslog" >> $logging_config_path
echo "    </pattern>" >> $logging_config_path
echo "  </parse>" >> $logging_config_path
echo "  path /var/log/pbs/syslog" >> $logging_config_path
echo "  pos_file /var/lib/google-fluentd/pos/pbs/syslog.pos" >> $logging_config_path
echo "  read_from_head true" >> $logging_config_path
echo "  tag pbs_syslog" >> $logging_config_path
echo "</source>" >> $logging_config_path
# Restart required to load updated config
docker restart stackdriver-logging-agent

# Increase the file descriptor soft limit to be equal to the hard limit
# The hard limit can be obtained by running
echo "Setting file descriptor soft limit to $(ulimit -Hn)"
ulimit -n $(ulimit -Hn)
