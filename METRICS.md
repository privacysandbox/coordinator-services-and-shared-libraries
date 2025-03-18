# PBS - Coordinator Metrics Documentation

This document provides a detailed list of all metrics exported for the PBS -
Coordinator service, including each metric's name, category, class, type, unit (
if available), description, and labels. It also provides details on how to
enable them.

---

## Enabling Metrics for PBS

To enable metrics for the PBS service, ensure the following environment
variables are set. These configurations control the activation and behavior of
metric exports, including the export interval and timeout settings.

### Configuration Variables

Add the following environment variables to
the `pbs_application_environment_variables`:

```plaintext
pbs_application_environment_variables = [
  {
    name = "google_scp_otel_enabled"
    value = "true"
  },
  {
    name = "google_scp_otel_metric_export_interval_msec"
    value = "60000"   # Export metrics every 60 seconds
  },
  {
    name = "google_scp_otel_metric_export_timeout_msec"
    value = "50000"   # Set a timeout of 50 seconds for exporting metrics
  },
  {
    name = "OTEL_METRICS_EXPORTER"
    value = "googlecloud"  # Specifies Google Cloud as the metrics exporter
  },
]
```

---

# Server Side Metrics Documentation

This document provides a detailed list of server side metrics exported for the
PBS service, following the OpenTelemetry conventions.

---

## Common Labels

These labels are common across multiple server side metrics and can be referred
to in each metric's specific table.

### Common Metric Fields

| Label Name                | Type    | Description                              |
|---------------------------|---------|------------------------------------------|
| `cloud_project_number`    | string  | Project number for cloud metrics         |
| `instrumentation_version` | string  | Version of the instrumentation used      |
| `instrumentation_source`  | string  | Source of the instrumentation            |
| `service_name`            | string  | Name of the service exporting the metric |
| `task_id`                 | string  | Task identifier for the PBS task         |
| `otel_metrics_overflow`   | boolean | Indicates overflow in OTEL metrics       |
| `job`                     | string  | Job identifier                           |
| `client_email`            | string  | Client email                             |
| `client_subject`          | string  | Client subject                           |
| `environment_name`        | string  | Environment where the service is running |

### GCP Resource Fields

| Label Name       | Type   | Description            |
|------------------|--------|------------------------|
| `gcp_project_id` | string | GCP Project identifier |

### AWS Resource Fields

| Label Name    | Type   | Description                            |
|---------------|--------|----------------------------------------|
| `aws_account` | string | AWS account associated with the metric |

---

### Metric: http.server.request.duration

| Name                           | Instrument Type | Unit (UCUM) | Description                       | Stability |
|--------------------------------|-----------------|-------------|-----------------------------------|-----------|
| `http.server.request.duration` | Histogram       | s           | Duration of HTTP server requests. | Stable    |

| Attribute                   | Type   | Description                                  | Examples        |
|-----------------------------|--------|----------------------------------------------|-----------------|
| `http.request.method`       | string | HTTP request method                          | GET; POST; HEAD |
| `url.scheme`                | string | The URI scheme identifying the protocol      | http; https     |
| `error.type`                | string | Class of error the operation ended with      | timeout; 500    |
| `http.response.status_code` | int    | HTTP response status code                    | 200             |
| `http.route`                | string | The matched route, path template             | /users/:userID  |
| `server.address`            | string | Local HTTP server name that received request | example.com     |
| `server.port`               | int    | Port of the local HTTP server                | 80; 8080; 443   |

---

### Metric: http.server.active_requests

| Name                          | Instrument Type  | Unit (UCUM)                 | Description                                  | Stability |
|-------------------------------|------------------|-----------------------------|----------------------------------------------|-----------|
| `http.server.active_requests` | Observable Gauge | {Number of active requests} | Number of active HTTP server (PBS) requests. | Stable    |

| Attribute                 | Type   | Description                      | Examples |
|---------------------------|--------|----------------------------------|----------|
| `cloud_project_number`    | string | Project number for cloud metrics |          |
| `instrumentation_version` | string | Version of instrumentation used  | 1.0, 2.0 |
| `instrumentation_source`  | string | Source of the instrumentation    |          |

---

### Metric: http.server.request.body.size

| Name                            | Instrument Type | Unit (UCUM) | Description                                     | Stability |
|---------------------------------|-----------------|-------------|-------------------------------------------------|-----------|
| `http.server.request.body.size` | Histogram       | By          | Size of HTTP server request body (uncompressed) | Stable    |

| Attribute                         | Type   | Description                                  | Examples        |
|-----------------------------------|--------|----------------------------------------------|-----------------|
| `server.address`                  | string | Local HTTP server name that received request | example.com     |
| `server.port`                     | int    | Port of the local HTTP server                | 80; 8080; 443   |
| `http.route`                      | string | The matched route, path template             | /users/:userID  |
| `http.request.method`             | string | HTTP request method                          | GET; POST; HEAD |
| `pbs.claimed_identity`            | string | Claimed identity in PBS                      |                 |
| `scp.http.request.client_version` | string | Client version in SCP                        |                 |
| `pbs.auth_domain`                 | string | Authentication domain in PBS                 |                 |

---

### Metric: http.server.response.body.size

| Name                             | Instrument Type | Unit (UCUM) | Description                                      | Stability |
|----------------------------------|-----------------|-------------|--------------------------------------------------|-----------|
| `http.server.response.body.size` | Histogram       | By          | Size of HTTP server response body (uncompressed) | Stable    |

| Attribute                         | Type   | Description                                  | Examples        |
|-----------------------------------|--------|----------------------------------------------|-----------------|
| `server.address`                  | string | Local HTTP server name that received request | example.com     |
| `server.port`                     | int    | Port of the local HTTP server                | 80; 8080; 443   |
| `http.route`                      | string | The matched route, path template             | /users/:userID  |
| `http.request.method`             | string | HTTP request method                          | GET; POST; HEAD |
| `http.response.status_code`       | int    | HTTP response status code                    | 200             |
| `pbs.claimed_identity`            | string | Claimed identity in PBS                      |                 |
| `scp.http.request.client_version` | string | Client version in SCP                        |                 |
| `pbs.auth_domain`                 | string | Authentication domain in PBS                 |                 |

---

# Client Side Metrics Documentation

This document provides a detailed list of client side metrics exported for the
PBS service, following the OpenTelemetry conventions.

---

## Common Labels

These labels are common across multiple client side metrics and can be referred
to in each metric's specific table.

### Common Metric Fields

| Label Name                | Type    | Description                              |
|---------------------------|---------|------------------------------------------|
| `cloud_project_number`    | string  | Project number for cloud metrics         |
| `instrumentation_version` | string  | Version of the instrumentation used      |
| `instrumentation_source`  | string  | Source of the instrumentation            |
| `service_name`            | string  | Name of the service exporting the metric |
| `task_id`                 | string  | Task identifier for the PBS task         |
| `otel_metrics_overflow`   | boolean | Indicates overflow in OTEL metrics       |
| `job`                     | string  | Job identifier                           |
| `client_email`            | string  | Client email                             |
| `client_subject`          | string  | Client subject                           |
| `environment_name`        | string  | Environment where the service is running |

### GCP Resource Fields

| Label Name       | Type   | Description            |
|------------------|--------|------------------------|
| `gcp_project_id` | string | GCP Project identifier |

### AWS Resource Fields

| Label Name    | Type   | Description                            |
|---------------|--------|----------------------------------------|
| `aws_account` | string | AWS account associated with the metric |

---

### Metric: http.client.connect_errors

| Name                         | Instrument Type | Unit (UCUM) | Description                    | Stability |
|------------------------------|-----------------|-------------|--------------------------------|-----------|
| `http.client.connect_errors` | Counter         | {Number}    | Count of client connect errors | Stable    |

| Attribute        | Type   | Description                      | Examples    |
|------------------|--------|----------------------------------|-------------|
| `server.address` | string | Host of the server               | example.com |
| `server.port`    | int    | Port of the server               | 80; 8080    |
| `url.scheme`     | string | URL scheme (e.g., http or https) | http; https |

---

### Metric: http.client.server_latency

| Name                         | Instrument Type | Unit (UCUM) | Description                    | Stability |
|------------------------------|-----------------|-------------|--------------------------------|-----------|
| `http.client.server_latency` | Histogram       | s           | Measures client-server latency | Stable    |

| Attribute                   | Type   | Description               | Examples    |
|-----------------------------|--------|---------------------------|-------------|
| `server.address`            | string | Host of the server        | example.com |
| `server.port`               | int    | Port of the server        | 80; 8080    |
| `url.scheme`                | string | URL scheme                | http; https |
| `http.response.status_code` | int    | HTTP response status code | 200         |
| `client.return_code`        | int    | Client return code        | 0, 1        |
| `pbs.claimed_identity`      | string | Claimed identity in PBS   |             |

---

### Metric: http.client.request.duration

| Name                           | Instrument Type | Unit (UCUM) | Description                     | Stability |
|--------------------------------|-----------------|-------------|---------------------------------|-----------|
| `http.client.request.duration` | Histogram       | s           | Measures client-request latency | Stable    |

| Attribute                   | Type   | Description               | Examples    |
|-----------------------------|--------|---------------------------|-------------|
| `server.address`            | string | Host of the server        | example.com |
| `server.port`               | int    | Port of the server        | 80; 8080    |
| `url.scheme`                | string | URL scheme                | http; https |
| `http.response.status_code` | int    | HTTP response status code | 200         |
| `client.return_code`        | int    | Client return code        | 0, 1        |
| `pbs.claimed_identity`      | string | Claimed identity in PBS   |             |

---

### Metric: http.client.request.body.size

| Name                            | Instrument Type | Unit (UCUM) | Description                         | Stability |
|---------------------------------|-----------------|-------------|-------------------------------------|-----------|
| `http.client.request.body.size` | Histogram       | By          | Measures request body size in bytes | Stable    |

| Attribute              | Type   | Description             | Examples    |
|------------------------|--------|-------------------------|-------------|
| `server.address`       | string | Host of the server      | example.com |
| `server.port`          | int    | Port of the server      | 80; 8080    |
| `url.scheme`           | string | URL scheme              | http; https |
| `pbs.claimed_identity` | string | Claimed identity in PBS |             |

---

### Metric: http.client.response.body.size

| Name                             | Instrument Type | Unit (UCUM) | Description                          | Stability |
|----------------------------------|-----------------|-------------|--------------------------------------|-----------|
| `http.client.response.body.size` | Histogram       | By          | Measures response body size in bytes | Stable    |

| Attribute                   | Type   | Description               | Examples    |
|-----------------------------|--------|---------------------------|-------------|
| `server.address`            | string | Host of the server        | example.com |
| `server.port`               | int    | Port of the server        | 80; 8080    |
| `url.scheme`                | string | URL scheme                | http; https |
| `http.response.status_code` | int    | HTTP response status code | 200         |
| `client.return_code`        | int    | Client return code        | 0, 1        |
| `pbs.claimed_identity`      | string | Claimed identity in PBS   |             |

---

### Metric: http.client.connection.duration

| Name                              | Instrument Type | Unit (UCUM) | Description                  | Stability |
|-----------------------------------|-----------------|-------------|------------------------------|-----------|
| `http.client.connection.duration` | Histogram       | s           | Measures connection duration | Stable    |

| Attribute        | Type   | Description        | Examples    |
|------------------|--------|--------------------|-------------|
| `server.address` | string | Host of the server | example.com |
| `server.port`    | int    | Port of the server | 80; 8080    |
| `url.scheme`     | string | URL scheme         | http; https |

---

### Metric: http.client.active_requests

| Name                          | Instrument Type  | Unit (UCUM)                 | Description                                       | Stability |
|-------------------------------|------------------|-----------------------------|---------------------------------------------------|-----------|
| `http.client.active_requests` | Observable Gauge | {Number of active requests} | Measures the number of active PBS client requests | Stable    |

---

### Metric: http.client.open_connections

| Name                           | Instrument Type  | Unit (UCUM)                  | Description                             | Stability |
|--------------------------------|------------------|------------------------------|-----------------------------------------|-----------|
| `http.client.open_connections` | Observable Gauge | {Number of open connections} | Measures the number of open connections | Stable    |

---

### Metric: http.client.address_errors

| Name                         | Instrument Type | Unit (UCUM) | Description                               | Stability |
|------------------------------|-----------------|-------------|-------------------------------------------|-----------|
| `http.client.address_errors` | Counter         | {Number}    | Measures client address resolution errors | Stable    |

| Attribute    | Type   | Description       | Examples         |
|--------------|--------|-------------------|------------------|
| `server.uri` | string | URI of the server | example.com/path |

---

### Metric: http.client.connection.creation_errors

| Name                                     | Instrument Type | Unit (UCUM) | Description                                         | Stability |
|------------------------------------------|-----------------|-------------|-----------------------------------------------------|-----------|
| `http.client.connection.creation_errors` | Counter         | {Number}    | Measures count of client connection creation errors | Stable    |

| Attribute              | Type   | Description             | Examples |
|------------------------|--------|-------------------------|----------|
| `pbs.claimed_identity` | string | Claimed identity in PBS |          |

---

# Application/Product Metrics Documentation

This document provides a detailed list of application/product metrics exported
for the PBS service, following the OpenTelemetry conventions.

---

## Common Labels

These labels are common across multiple application/product metrics and can be
referred to in each metric's specific table.

### Common Metric Fields

| Label Name                | Type    | Description                              |
|---------------------------|---------|------------------------------------------|
| `cloud_project_number`    | string  | Project number for cloud metrics         |
| `instrumentation_version` | string  | Version of the instrumentation used      |
| `instrumentation_source`  | string  | Source of the instrumentation            |
| `service_name`            | string  | Name of the service exporting the metric |
| `task_id`                 | string  | Task identifier for the PBS task         |
| `otel_metrics_overflow`   | boolean | Indicates overflow in OTEL metrics       |
| `job`                     | string  | Job identifier                           |
| `client_email`            | string  | Client email                             |
| `client_subject`          | string  | Client subject                           |
| `environment_name`        | string  | Environment where the service is running |
| `load_success`            | boolean | Indicator of load success status         |
| `unload_success`          | boolean | Indicator of unload success status       |
| `partition_id`            | string  | Partition id                             |

### GCP Resource Fields

| Label Name       | Type   | Description            |
|------------------|--------|------------------------|
| `gcp_project_id` | string | GCP Project identifier |

### AWS Resource Fields

| Label Name    | Type   | Description                            |
|---------------|--------|----------------------------------------|
| `aws_account` | string | AWS account associated with the metric |

---

### Metric: google.scp.pbs.frontend.successful_budget_consumed

| Name                                                 | Instrument Type | Unit (UCUM) | Description                                              | Stability |
|------------------------------------------------------|-----------------|-------------|----------------------------------------------------------|-----------|
| `google.scp.pbs.frontend.successful_budget_consumed` | Histogram       | {Number}    | Count of successful budgets consumed per transaction/job | Stable    |

| Attribute                         | Type   | Description              | Examples              |
|-----------------------------------|--------|--------------------------|-----------------------|
| `transaction_phase`               | string | Phase of the transaction | Initialization, Final |
| `reporting_origin`                | string | Origin of the report     | Internal, External    |
| `pbs.claimed_identity`            | string | Claimed identity in PBS  |                       |
| `scp.http.request.client_version` | string | Client version in SCP    |                       |
| `pbs.auth_domain`                 | string | Authentication domain    | example.com           |

---

### Metric: google.scp.pbs.frontend.keys_per_transaction

| Name                                           | Instrument Type | Unit (UCUM) | Description                                               | Stability |
|------------------------------------------------|-----------------|-------------|-----------------------------------------------------------|-----------|
| `google.scp.pbs.frontend.keys_per_transaction` | Histogram       | {Number}    | Count of keys, budgets, or shared IDs per transaction/job | Stable    |

| Attribute                         | Type   | Description              | Examples              |
|-----------------------------------|--------|--------------------------|-----------------------|
| `transaction_phase`               | string | Phase of the transaction | Initialization, Final |
| `reporting_origin`                | string | Origin of the report     | Internal, External    |
| `pbs.claimed_identity`            | string | Claimed identity in PBS  |                       |
| `scp.http.request.client_version` | string | Client version in SCP    |                       |
| `pbs.auth_domain`                 | string | Authentication domain    | example.com           |

---

### Metric: google.scp.pbs.consume_budget.budget_exhausted

| Name                                             | Instrument Type | Unit (UCUM) | Description                                    | Stability |
|--------------------------------------------------|-----------------|-------------|------------------------------------------------|-----------|
| `google.scp.pbs.consume_budget.budget_exhausted` | Histogram       | {Number}    | Count of budgets exhausted per transaction/job | Stable    |

| Attribute                         | Type   | Description             | Examples           |
|-----------------------------------|--------|-------------------------|--------------------|
| `reporting_origin`                | string | Origin of the report    | Internal, External |
| `pbs.claimed_identity`            | string | Claimed identity in PBS |                    |
| `scp.http.request.client_version` | string | Client version in SCP   |                    |
| `pbs.auth_domain`                 | string | Authentication domain   | example.com        |

---

# Existing Metrics Documentation - PBS v1

This document provides a detailed list of PBS v1 metrics exported for the PBS
service, following the OpenTelemetry conventions.

---

## Common Labels

These labels are common across multiple Existing Metrics - PBS v1 metrics and
can be referred to in each metric's specific table.

### Common Metric Fields

| Label Name                | Type    | Description                              |
|---------------------------|---------|------------------------------------------|
| `cloud_project_number`    | string  | Project number for cloud metrics         |
| `instrumentation_version` | string  | Version of the instrumentation used      |
| `instrumentation_source`  | string  | Source of the instrumentation            |
| `service_name`            | string  | Name of the service exporting the metric |
| `task_id`                 | string  | Task identifier for the PBS task         |
| `otel_metrics_overflow`   | boolean | Indicates overflow in OTEL metrics       |
| `job`                     | string  | Job identifier                           |
| `client_email`            | string  | Client email                             |
| `client_subject`          | string  | Client subject                           |
| `environment_name`        | string  | Environment where the service is running |
| `load_success`            | boolean | Indicator of load success status         |
| `unload_success`          | boolean | Indicator of unload success status       |
| `partition_id`            | string  | Partition id                             |

### GCP Resource Fields

| Label Name       | Type   | Description            |
|------------------|--------|------------------------|
| `gcp_project_id` | string | GCP Project identifier |

### AWS Resource Fields

| Label Name    | Type   | Description                            |
|---------------|--------|----------------------------------------|
| `aws_account` | string | AWS account associated with the metric |

---

### Metric: google.scp.pbs.health.memory_usage

| Name                                 | Instrument Type  | Unit (UCUM) | Description             | Stability |
|--------------------------------------|------------------|-------------|-------------------------|-----------|
| `google.scp.pbs.health.memory_usage` | Observable Gauge | %           | Memory usage percentage | Stable    |

---

### Metric: google.scp.pbs.health.filesystem_storage_usage

| Name                                             | Instrument Type  | Unit (UCUM) | Description                   | Stability |
|--------------------------------------------------|------------------|-------------|-------------------------------|-----------|
| `google.scp.pbs.health.filesystem_storage_usage` | Observable Gauge | %           | File storage usage percentage | Stable    |

---
