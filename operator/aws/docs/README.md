## Operator setup

Welcome to the README for setting up AWS enclave Aggregation Service!

Please follow the instructions below to set up the Aggregation service AWS enclaves
for processing the encrypted conversion events.
Ensure to batch your encrypted events and put them in S3 bucket. [Link]


## Deployment pre-requisites
1. Install Terraform (v1.0.4)

## Steps to Build Enclave Images
1. Configure AWS credentials using [AWS documentation](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
2. Create a S3 bucket using AWS console/cli for terraform state.
3. Edit the following fields in the `main.tf` script:
    - environment: name of the environment
    - region: region to deploy services [Currently only us-east-1 supported]
    - assume_role_parameter: IAM role given by coordinator
    - backend bucket and key: S3 bucket and key to maintain the terraform state (created in step 2)
    - alarm_notification_email: Email which receives alarm notifications

## Applying changes to setup Aggregation Service

```
terraform init
terraform plan
terraform apply
```

## Congratulations! Your setup is ready

Start using `createJob`/ `getJob` APIs to submit your batch request for Aggregation

```
https://<api-gateway>.execute-api.us-east-1.amazonaws.com/v1alpha/createJob
{
    "input_data_blob_prefix": <filename>,
    "input_data_bucket_name": <bucket>,
    "output_data_blob_prefix": <filename>,
    "output_data_bucket_name": <bucket>,
    "postback_url": "http://postback.com",
    "job_parameters": {
        "attribution_report_to": "foo.com",
    },
    "job_request_id": <request-id>
}



https://<api-gateway>.execute-api.us-east-1.amazonaws.com/v1alpha/getJob?job_request_id=<request_id>&attribution_report_to=<>

```
