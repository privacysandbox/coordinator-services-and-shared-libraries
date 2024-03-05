# Must be updated
aws_region = "<deployment-region>"

# This should match/coorelate to the name of the environment of the distributed pbs.
environment = "demo-a"

# Location of the heartbeat prober source code zip.
heartbeat_source_zip = "../../../dist/pbs_heartbeat_pkg.zip"

# URL of the distributed pbs endpoint url to probe.
# (e.g. https://mp-pbs-<environment>.<domain>/v1/transactions)
url_to_probe = "<pbs-endpoint-url>"

# Enable/Disable alarms
alarms_enabled = false

# Email used for notificaitons.
alarms_notification_email = "<notification email>"

# (Optional) Add any string to the custom_alarm_label to help filtering of alarms.
# Allowed chars (a-zA-Z_-) max 30 chars.
# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label = "<CustomAlarmLabel>"
