# Must be updated
aws_region = "<deployment-region>"

# This should match/coorelate to the name of the environment of the mpkhs publickey service.
environment = "demo-a"

# Location of the heartbeat prober source code zip.
heartbeat_source_zip = "../../../dist/publickey_heartbeat_pkg.zip"

# URL of the mpkhs public key endpoint url to probe.
# (e.g. https://publickeyservice.<domain>/v1alpha/publicKeys)
url_to_probe = "<mpkhs-publickey-endpoint-url>"

# Enable/Disable alarms
alarms_enabled = false

# Email used for notificaitons.
alarms_notification_email = "<notification email>"

# (Optional) Add any string to the custom_alarm_label to help filtering of alarms.
# Allowed chars (a-zA-Z_-) max 30 chars.
# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label = "<CustomAlarmLabel>"
