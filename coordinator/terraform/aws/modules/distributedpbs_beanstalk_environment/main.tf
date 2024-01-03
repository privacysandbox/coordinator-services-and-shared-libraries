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

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

locals {
  privacy_budget_domain = (var.environment_prefix != "prod" ? "${var.service_subdomain}-${var.environment_prefix}.${var.parent_domain_name}" : "${var.service_subdomain}.${var.parent_domain_name}")
  output_domain         = var.enable_domain_management ? "https://${local.privacy_budget_domain}" : "http://${aws_elastic_beanstalk_environment.beanstalk_app_environment.cname}"

  # NOTE: If the value of health_service_host_port changes, then we need to also update this value in:
  # files/alb-target-group.config and files/nlb-target-group.config
  health_service_host_port = "81"

  remote_role_arn = "arn:aws:iam::${var.remote_coordinator_aws_account_id}:role/${var.environment_prefix}_pbs_remote_role_${var.aws_account_id}"

  # The default should be kept up to date with the latest version, if possible,
  # as solution stack versions do not remain available if they have not been
  # used by a given AWS account.
  # https://docs.aws.amazon.com/elasticbeanstalk/latest/platforms/platform-history-docker.html
  solution_stack_name                   = var.solution_stack_name != "" ? var.solution_stack_name : "64bit Amazon Linux 2 v3.5.9 running Docker"
  alarming_sns_topic_permissions        = <<EOF
,
     {
      "Effect": "Allow",
      "Action": [
        "sns:SetTopicAttributes",
        "sns:GetTopicAttributes",
        "sns:Subscribe",
        "sns:Unsubscribe",
        "sns:Publish"
      ],
      "Resource": [
        "${var.sns_topic_arn}"
      ]
    }
EOF
  enable_alarming_sns_topic_permissions = var.enable_alarms ? local.alarming_sns_topic_permissions : ""
  min_instances_in_service              = var.autoscaling_min_size - 1
  ignore_4xx_errors_in_elb              = "{\"Rules\": { \"Environment\": { \"Application\": { \"ApplicationRequests4xx\": { \"Enabled\": false } }, \"ELB\": { \"ELBRequests4xx\": {\"Enabled\": false } } } }, \"Version\": 1 }"
}

module "sslcertificate" {
  source                = "../sslcertificateprovider"
  count                 = var.enable_domain_management ? 1 : 0
  environment           = var.environment_prefix
  service_domain_name   = local.privacy_budget_domain
  domain_hosted_zone_id = var.domain_hosted_zone_id
}

data "aws_elastic_beanstalk_hosted_zone" "current" {}

data "template_file" "dockerrun" {
  template = file("${path.module}/files/Dockerrun.aws.json.in")
  vars = {
    pbs_container_image_repo_path_and_tag = "${var.aws_account_id}.dkr.ecr.${var.aws_region}.amazonaws.com/${var.container_repo_name}:${var.beanstalk_app_version}"
    pbs_container_port                    = var.pbs_container_port
    pbs_container_health_service_port     = var.pbs_container_health_service_port
    health_service_host_port              = local.health_service_host_port
  }
}

data "local_file" "log_streaming_config" {
  filename = "${path.module}/files/logs-streamtocloudwatch-linux.config"
}

data "local_file" "alb_target_group_config" {
  filename = "${path.module}/files/alb-target-group.config"
}

data "local_file" "nlb_target_group_config" {
  filename = "${path.module}/files/nlb-target-group.config"
}

data "local_file" "autoscaling_config" {
  filename = "${path.module}/files/autoscaling.config"
}

data "local_file" "alb_listeners_config" {
  filename = "${path.module}/files/alb-listeners.config"
}

data "local_file" "cloudwatch_agent_config" {
  filename = "${path.module}/files/cloudwatch-agent.config"
}

data "local_file" "autoscaling_launch_config" {
  filename = "${path.module}/files/autoscaling-launch.config"
}

resource "random_uuid" "files" {
  keepers = {
    for filename in fileset(path.module, "files/*") :
    filename => filemd5("${path.module}/${filename}")
  }
}

data "archive_file" "version_bundle" {
  type        = "zip"
  output_path = "${path.module}/generated/${var.beanstalk_app_version}-${random_uuid.files.result}.zip"

  source {
    content  = data.template_file.dockerrun.rendered
    filename = "Dockerrun.aws.json"
  }

  source {
    content  = data.local_file.log_streaming_config.content
    filename = ".ebextensions/logs-streamtocloudwatch-linux.config"
  }

  source {
    content  = data.local_file.autoscaling_config.content
    filename = ".ebextensions/autoscaling.config"
  }

  source {
    content  = data.local_file.cloudwatch_agent_config.content
    filename = ".ebextensions/cloudwatch-agent.config"
  }

  source {
    content  = data.local_file.autoscaling_launch_config.content
    filename = ".ebextensions/ec2-launch-template.config"
  }

  # These settings are only added if domain management is ENABLED
  dynamic "source" {
    for_each = (var.enable_domain_management ? [
      {
        content  = data.local_file.alb_target_group_config.content
        filename = ".ebextensions/alb-target-group.config"
      },
      {
        content  = data.local_file.alb_listeners_config.content
        filename = ".ebextensions/alb-listeners.config"
      }
    ] : [])
    content {
      content  = source.value.content
      filename = source.value.filename
    }
  }

  # These settings are only added if domain management is DISABLED
  dynamic "source" {
    for_each = (var.enable_domain_management ? [] : [
      {
        content  = data.local_file.nlb_target_group_config.content
        filename = ".ebextensions/nlb-target-group.config"
      }
    ])
    content {
      content  = source.value.content
      filename = source.value.filename
    }
  }
}

resource "aws_s3_bucket_object" "version_object" {
  bucket = var.beanstalk_app_s3_bucket_name
  key    = "pbs_beanstalk_app/${var.beanstalk_app_version}.zip"
  source = data.archive_file.version_bundle.output_path
  # This must be an attribute of the version bundle archive as otherwise
  # terraform plan could fail because this resource fails to find the file
  # that will be used to create the S3 object.
  # Making it an attribute ensures there won't be timing issues as this attribute
  # is only known after object creation.
  etag = data.archive_file.version_bundle.output_md5

  server_side_encryption = "AES256"

  lifecycle {
    create_before_destroy = true
  }

  depends_on = [
    data.archive_file.version_bundle
  ]
}

resource "aws_elastic_beanstalk_application" "pbs" {
  name        = "${var.environment_prefix}-google-scp-pbs-beanstalk-app"
  description = "Privacy Budget Service application"
}

resource "aws_elastic_beanstalk_application_version" "app_version" {
  name        = var.beanstalk_app_version
  application = aws_elastic_beanstalk_application.pbs.name
  description = "Privacy Budget Service application ${var.beanstalk_app_version}"
  bucket      = var.beanstalk_app_s3_bucket_name
  key         = aws_s3_bucket_object.version_object.id

  lifecycle {
    create_before_destroy = true
  }

  depends_on = [
    aws_elastic_beanstalk_application.pbs
  ]
}

resource "aws_iam_role" "ec2_iam_role" {
  name = "${var.environment_prefix}-google-scp-pbs-ec2-role"

  assume_role_policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Action": "sts:AssumeRole",
      "Principal": {
        "Service": "ec2.amazonaws.com"
      },
      "Effect": "Allow"
    }
  ]
}
EOF
}

resource "aws_iam_instance_profile" "ec2_user" {
  name = "${var.environment_prefix}-google-scp-pbs-ec2-user"
  role = aws_iam_role.ec2_iam_role.name
  depends_on = [
    aws_iam_role.ec2_iam_role
  ]
}

resource "aws_iam_role_policy" "beanstalk_base_policy" {
  name = "${var.environment_prefix}-google-scp-pbs-beanstalk-policy"
  role = aws_iam_role.ec2_iam_role.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
        "Action": [
          "autoscaling:DescribeAccountLimits",
          "autoscaling:DescribeAutoScalingGroups",
          "autoscaling:DescribeAutoScalingInstances",
          "autoscaling:DescribeLaunchConfigurations",
          "autoscaling:DescribeLoadBalancers",
          "autoscaling:DescribeNotificationConfigurations",
          "autoscaling:DescribeScalingActivities",
          "autoscaling:DescribeScheduledActions",

          "cloudwatch:PutMetricData",

          "ec2:DescribeInstanceStatus",
          "ec2:DescribeInstances",
          "ec2:DescribeTags",

          "ec2messages:AcknowledgeMessage",
          "ec2messages:DeleteMessage",
          "ec2messages:FailMessage",
          "ec2messages:GetEndpoint",
          "ec2messages:GetMessages",
          "ec2messages:SendReply",

          "logs:CreateLogStream",
          "logs:CreateLogGroup",
          "logs:DescribeLogStreams",
          "logs:DescribeLogGroups",

          "ssm:DescribeInstanceProperties",
          "ssm:DescribeDocumentParameters",
          "ssm:ListInstanceAssociations",
          "ssm:RegisterManagedInstance",
          "ssm:UpdateInstanceInformation",

          "elasticbeanstalk:PutInstanceStatistics",

          "elasticloadbalancing:DescribeInstanceHealth",
          "elasticloadbalancing:DescribeTargetHealth",
          "elasticloadbalancing:DescribeLoadBalancers",
          "elasticloadbalancing:DescribeTargetGroups"
        ],
        "Effect": "Allow",
        "Resource": "*"
    },
    {
        "Sid": "AllowPutLogEventsOnExpectedLogGroupsOnly",
        "Effect": "Allow",
        "Action": [
          "logs:PutLogEvents"
        ],
        "Resource": [
          "arn:aws:logs:${var.aws_region}:${var.aws_account_id}:log-group:/aws/elasticbeanstalk/*",
          "arn:aws:logs:${var.aws_region}:${var.aws_account_id}:log-group:/aws/vendedlogs/*"
        ]
    },
    {
        "Sid": "AllowCloudWatchAuth",
        "Effect": "Allow",
        "Action": [
              "ssm:GetParameter",
              "ssm:PutParameter"
        ],
        "Resource": "arn:aws:ssm:*:*:parameter/AmazonCloudWatch-*"
    },
    {
        "Sid": "AllowEbEcrAuth",
        "Effect": "Allow",
        "Action": [
          "ecr:GetAuthorizationToken"
        ],
        "Resource": [
          "*"
        ]
    },
    {
        "Sid": "AllowEbEcrPull",
        "Action": [
          "ecr:GetAuthorizationToken",
          "ecr:BatchCheckLayerAvailability",
          "ecr:GetDownloadUrlForLayer",
          "ecr:GetRepositoryPolicy",
          "ecr:DescribeRepositories",
          "ecr:ListImages",
          "ecr:BatchGetImage"
        ],
        "Effect": "Allow",
        "Resource": [
          "arn:aws:ecr:${var.aws_region}:${var.aws_account_id}:repository/${var.container_repo_name}",
          "arn:aws:ecr:${var.aws_region}:${var.aws_account_id}:repository/${var.container_repo_name}/*"
        ]
    },
    {
        "Sid": "AllowEbS3BucketAccess",
        "Action": [
            "s3:ListBucket",
            "s3:GetObjectAcl",
            "s3:GetBucketPolicy",
            "s3:DeleteObject",
            "s3:PutObject",
            "s3:PutObjectAcl"
        ],
        "Effect": "Allow",
        "Resource": [
            "arn:aws:s3:::elasticbeanstalk-${var.aws_region}-${var.aws_account_id}",
            "arn:aws:s3:::elasticbeanstalk-${var.aws_region}-${var.aws_account_id}/*"
        ]
    },
    {
        "Sid": "AllowEbS3VersionBucketAccess",
        "Action": [
            "s3:ListBucket",
            "s3:GetObject",
            "s3:GetObjectAcl"
        ],
        "Effect": "Allow",
        "Resource": [
            "arn:aws:s3:::${var.beanstalk_app_s3_bucket_name}",
            "arn:aws:s3:::${var.beanstalk_app_s3_bucket_name}/*"
        ]
    },
    {
        "Sid": "AllowPbsBudgetKeysTableAccess",
        "Action": [
          "dynamodb:BatchGetItem",
          "dynamodb:BatchWriteItem",
          "dynamodb:ConditionCheckItem",
          "dynamodb:PutItem",
          "dynamodb:DescribeTable",
          "dynamodb:DeleteItem",
          "dynamodb:GetItem",
          "dynamodb:Scan",
          "dynamodb:Query",
          "dynamodb:UpdateItem",
          "dynamodb:GetShardIterator",
          "dynamodb:DescribeStream",
          "dynamodb:GetRecords",
          "dynamodb:ListStreams"
        ],
        "Effect": "Allow",
        "Resource": [
            "arn:aws:dynamodb:${var.aws_region}:${var.aws_account_id}:table/${var.pbs_budget_keys_dynamodb_table_name}",
            "arn:aws:dynamodb:${var.aws_region}:${var.aws_account_id}:table/${var.pbs_budget_keys_dynamodb_table_name}/*"
        ]
    },
    {
        "Sid": "AllowPbsPartitionLockTableAccess",
        "Action": [
          "dynamodb:BatchGetItem",
          "dynamodb:BatchWriteItem",
          "dynamodb:ConditionCheckItem",
          "dynamodb:PutItem",
          "dynamodb:DescribeTable",
          "dynamodb:DeleteItem",
          "dynamodb:GetItem",
          "dynamodb:Scan",
          "dynamodb:Query",
          "dynamodb:UpdateItem",
          "dynamodb:GetShardIterator",
          "dynamodb:DescribeStream",
          "dynamodb:GetRecords",
          "dynamodb:ListStreams"
        ],
        "Effect": "Allow",
        "Resource": [
            "arn:aws:dynamodb:${var.aws_region}:${var.aws_account_id}:table/${var.pbs_partition_lock_dynamodb_table_name}",
            "arn:aws:dynamodb:${var.aws_region}:${var.aws_account_id}:table/${var.pbs_partition_lock_dynamodb_table_name}/*"
        ]
    },
    {
        "Sid": "AllowPbsS3JournalBucketAccess",
        "Action": [
          "s3:ListBucket",
          "s3:PutObject",
          "s3:GetObject",
          "s3:DeleteObject",
          "s3:HeadObject"
        ],
        "Effect": "Allow",
        "Resource": [
            "arn:aws:s3:::${var.pbs_s3_bucket_name}",
            "arn:aws:s3:::${var.pbs_s3_bucket_name}/*"
        ]
    },
    {
        "Sid": "AllowPbsAuthApiGwExecution",
        "Effect": "Allow",
        "Action": "execute-api:Invoke",
        "Resource": [
          "arn:aws:execute-api:${var.aws_region}:${var.aws_account_id}:${var.auth_api_gateway_id}/${var.environment_prefix}/POST/auth"
        ]
    },
    {
        "Sid": "AllowPbsToAssumeRemoteRole",
        "Effect": "Allow",
        "Action": "sts:AssumeRole",
        "Resource": [
          "${local.remote_role_arn}"
        ]
    }
    ${local.enable_alarming_sns_topic_permissions}
  ]
}
EOF
}

# This is used to fail the deployment before creating/updating the beanstalk
# environment if the container version tag does not exist.
# This is needed because if the deployment goes through with a nonexistent
# docker image tag, the environment will be in an incosistent state which
# would require fixing outside of terraform.
resource "null_resource" "fail_if_missing_version" {
  triggers = {
    force_run = "${timestamp()}"
  }

  provisioner "local-exec" {
    command = <<EOT
#!/bin/bash
set -e
aws ecr describe-images \
  --region=${var.aws_region} \
  --repository-name=${var.container_repo_name} \
  --image-ids=imageTag=${var.beanstalk_app_version} 1> /dev/null
exit $?
EOT
  }
}

# Security group to allow instances within the private subnet to talk to one another
resource "aws_security_group" "allow_unrestricted_inbound_within_private_subnet" {
  # This resource only applies when VPC usage is enabled
  count = var.enable_vpc ? 1 : 0

  name_prefix = "allow_inbound_within_subnet"
  description = "Allow inbound traffic on port 80 within subnet"
  vpc_id      = var.vpc_id

  ingress {
    description = "Inbound TCP on port 80 within VPC"
    from_port   = 80
    to_port     = 80
    protocol    = "tcp"
    cidr_blocks = ["10.0.0.0/16"]
  }

  lifecycle {
    create_before_destroy = true
  }

  tags = {
    Environment = var.environment_prefix
    Name        = "allow_inbound_within_subnet"
  }
}

resource "aws_elastic_beanstalk_environment" "beanstalk_app_environment" {
  name                = "${var.environment_prefix}-google-scp-pbs"
  application         = aws_elastic_beanstalk_application.pbs.name
  cname_prefix        = var.cname_prefix
  version_label       = var.beanstalk_app_version
  solution_stack_name = local.solution_stack_name
  tier                = "WebServer"

  # NOTE:
  # All the setting elements below need the resource = ""
  # to prevent terraform from picking up changes incorrectly on terraform
  # apply after the initial creation.
  # https://github.com/hashicorp/terraform-provider-aws/issues/1471

  setting {
    namespace = "aws:autoscaling:launchconfiguration"
    name      = "RootVolumeSize"
    value     = var.root_volume_size_gb
    resource  = ""
  }

  # Limit SSH to VMs to localhost only.
  setting {
    namespace = "aws:autoscaling:launchconfiguration"
    name      = "SSHSourceRestriction"
    value     = "tcp,22,22,127.0.0.1/32"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:cloudwatch:logs"
    name      = "StreamLogs"
    value     = "true"
    resource  = ""
  }

  # Retention policies for custom logs are not set here. Custom logs
  # retention policies must be set as commands in config files
  # for .ebextensions
  setting {
    namespace = "aws:elasticbeanstalk:cloudwatch:logs"
    name      = "RetentionInDays"
    value     = "90"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:cloudwatch:logs:health"
    name      = "HealthStreamingEnabled"
    value     = "true"
    resource  = ""
  }

  # Notifications configuration for your elasticbeanstalk.
  # These settings are only applied when alarms are enabled.
  dynamic "setting" {
    for_each = (var.enable_alarms ? [
      {
        namespace = "aws:elasticbeanstalk:sns:topics"
        name      = "endpoint"
        value     = var.sns_topic_arn == "" ? var.alarm_notification_email : var.sqs_queue_arn
      },
      {
        namespace = "aws:elasticbeanstalk:sns:topics"
        name      = "topic_arn"
        value     = var.sns_topic_arn
      },
      {
        namespace = "aws:elasticbeanstalk:sns:topics"
        name      = "protocol"
        value     = var.sns_topic_arn == "" ? "email" : "sqs"
      }
    ] : [])
    content {
      namespace = setting.value.namespace
      name      = setting.value.name
      value     = setting.value.value
      resource  = ""
    }
  }

  # If domain management is enabled we use an application load balancer (ALB).
  # Else we use a network load balancer (NLB).
  setting {
    namespace = "aws:elasticbeanstalk:environment"
    name      = "LoadBalancerType"
    value     = (var.enable_domain_management ? "application" : "network")
    resource  = ""
  }

  # Add health check process
  setting {
    namespace = "aws:elasticbeanstalk:environment:process:healthcheck"
    name      = "Port"
    value     = local.health_service_host_port
    resource  = ""
  }

  # These settings are only applied when the usage of a VPC is enabled
  dynamic "setting" {
    for_each = (var.enable_vpc ? [
      {
        namespace = "aws:ec2:vpc"
        name      = "VPCId"
        value     = var.vpc_id
      },
      # The subnets that the ELB uses. These should be public.
      {
        namespace = "aws:ec2:vpc"
        name      = "ELBSubnets"
        value     = join(",", sort(var.public_subnet_ids))
        resource  = ""
      },
      # The subnets that the EC2 instances are in. These should be private.
      {
        namespace = "aws:ec2:vpc"
        name      = "Subnets"
        value     = join(",", sort(var.private_subnet_ids))
      },
      # Disable public IP association for EC2 instances.
      {
        namespace = "aws:ec2:vpc"
        name      = "AssociatePublicIpAddress"
        value     = false
      },
      {
        namespace = "aws:autoscaling:launchconfiguration"
        name      = "SecurityGroups"
        value     = "${var.vpc_default_sg_id},${aws_security_group.allow_unrestricted_inbound_within_private_subnet[0].id}"
      }
    ] : [])
    content {
      namespace = setting.value.namespace
      name      = setting.value.name
      value     = setting.value.value
      resource  = ""
    }
  }

  # These settings are only applicable to ALB, so we only add them when
  # domain management is enabled.
  dynamic "setting" {
    for_each = (var.enable_domain_management ? [
      # Set up the health check path for ALB
      {
        namespace = "aws:elasticbeanstalk:environment:process:healthcheck"
        name      = "HealthCheckPath"
        value     = "/health"
      },
      {
        namespace = "aws:elasticbeanstalk:healthreporting:system"
        name      = "ConfigDocument"
        value     = local.ignore_4xx_errors_in_elb
      },
      # Setup default process (port 80) to use the health check process
      # The health check port for this process is set in files/alb-target-group.config
      # We have this config here so the target group gets created, and then we reference
      # it from the config file where we can provide advanced configurations.
      {
        namespace = "aws:elasticbeanstalk:environment:process:default"
        name      = "HealthCheckPath"
        value     = "/health"
      },
      {
        namespace = "aws:elbv2:listener:443"
        name      = "SSLCertificateArns"
        value     = module.sslcertificate[0].acm_certificate_arn
      },
      {
        namespace = "aws:elbv2:listener:443"
        name      = "Protocol"
        value     = "HTTPS"
      },
      {
        namespace = "aws:elbv2:listener:443"
        name      = "SSLPolicy"
        value     = "ELBSecurityPolicy-TLS13-1-2-2021-06"
      },
      # Health check timeout is 9 seconds
      # Must be smaller than HealthCheckInterval
      {
        namespace = "aws:elasticbeanstalk:environment:process:default"
        name      = "HealthCheckTimeout"
        value     = "9"
      },
      # Each rule has to have a unique priority
      # Priorities are from 1 to 1000. Lower numbers take precedence.
      # The priority is used when multiple rules match a request, but
      # in our case, this is not possible.
      {
        namespace = "aws:elbv2:listenerrule:allowtxbegin"
        name      = "PathPatterns"
        value     = "/v1/transactions:begin"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxbegin"
        name      = "Priority"
        value     = "1000"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxprepare"
        name      = "PathPatterns"
        value     = "/v1/transactions:prepare"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxprepare"
        name      = "Priority"
        value     = "999"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxcommit"
        name      = "PathPatterns"
        value     = "/v1/transactions:commit"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxcommit"
        name      = "Priority"
        value     = "998"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxnotify"
        name      = "PathPatterns"
        value     = "/v1/transactions:notify"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxnotify"
        name      = "Priority"
        value     = "997"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxabort"
        name      = "PathPatterns"
        value     = "/v1/transactions:abort"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxabort"
        name      = "Priority"
        value     = "996"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxend"
        name      = "PathPatterns"
        value     = "/v1/transactions:end"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxend"
        name      = "Priority"
        value     = "995"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxstatus"
        name      = "PathPatterns"
        value     = "/v1/transactions:status"
      },
      {
        namespace = "aws:elbv2:listenerrule:allowtxstatus"
        name      = "Priority"
        value     = "994"
      },
      {
        namespace = "aws:elbv2:listener:443"
        name      = "Rules"
        value     = "allowtxstatus,allowtxnotify,allowtxbegin,allowtxcommit,allowtxend,allowtxprepare,allowtxabort"
      },
      # Loadbalancer attributes settings.
      {
        namespace = "aws:elbv2:loadbalancer"
        name      = "IdleTimeout"
        value     = "300"
      },
      # Enabling ALB access logs
      {
        namespace = "aws:elbv2:loadbalancer"
        name      = "AccessLogsS3Bucket"
        value     = var.pbs_s3_bucket_lb_access_logs_id
      },
      {
        namespace = "aws:elbv2:loadbalancer"
        name      = "AccessLogsS3Enabled"
        value     = var.enable_pbs_lb_access_logs
      }
    ] : [])
    content {
      namespace = setting.value.namespace
      name      = setting.value.name
      value     = setting.value.value
      resource  = ""
    }
  }

  # If domain management is enabled we enable port 443.
  # Else we disable port 443.
  setting {
    namespace = "aws:elbv2:listener:443"
    name      = "ListenerEnabled"
    value     = (var.enable_domain_management ? "true" : "false")
    resource  = ""
  }

  # If domain management is enabled, we disable port 80.
  # Else, we enable port 80.
  setting {
    namespace = "aws:elbv2:listener:default"
    name      = "ListenerEnabled"
    value     = (var.enable_domain_management ? "false" : "true")
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:environment:proxy"
    name      = "ProxyServer"
    value     = "none"
    resource  = ""
  }

  setting {
    namespace = "aws:autoscaling:launchconfiguration"
    name      = "InstanceType"
    value     = var.ec2_instance_type
    resource  = ""
  }

  # If this value changes then MinInstancesInService must be updated
  setting {
    namespace = "aws:autoscaling:asg"
    name      = "MinSize"
    value     = var.autoscaling_min_size
    resource  = ""
  }

  setting {
    namespace = "aws:autoscaling:asg"
    name      = "MaxSize"
    value     = var.autoscaling_max_size
    resource  = ""
  }

  # Use rolling updates
  setting {
    namespace = "aws:autoscaling:updatepolicy:rollingupdate"
    name      = "RollingUpdateEnabled"
    value     = "true"
    resource  = ""
  }

  # Consider an instance's update successful once the health checks pass
  setting {
    namespace = "aws:autoscaling:updatepolicy:rollingupdate"
    name      = "RollingUpdateType"
    value     = "Health"
    resource  = ""
  }

  # This value should be MinSize-1
  setting {
    namespace = "aws:autoscaling:updatepolicy:rollingupdate"
    name      = "MinInstancesInService"
    value     = local.min_instances_in_service
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:command"
    name      = "DeploymentPolicy"
    value     = "Rolling"
    resource  = ""
  }

  # Do updates in batches of 1 instance
  setting {
    namespace = "aws:autoscaling:updatepolicy:rollingupdate"
    name      = "MaxBatchSize"
    value     = "1"
    resource  = ""
  }

  # Perform health checks every 10 seconds
  setting {
    namespace = "aws:elasticbeanstalk:environment:process:default"
    name      = "HealthCheckInterval"
    value     = "10"
    resource  = ""
  }

  # We need 5 consecutive successful health checks to consider the instance healthy
  setting {
    namespace = "aws:elasticbeanstalk:environment:process:default"
    name      = "HealthyThresholdCount"
    value     = "5"
    resource  = ""
  }

  # Wait up to 30 seconds for active requests to finish before deregistering the
  # instance
  setting {
    namespace = "aws:elasticbeanstalk:environment:process:default"
    name      = "DeregistrationDelay"
    value     = "30"
    resource  = ""
  }

  # If we get 5 consecutive unsuccessful health checks, then the instance is
  # considered unhealthy
  setting {
    namespace = "aws:elasticbeanstalk:environment:process:default"
    name      = "UnhealthyThresholdCount"
    value     = "5"
    resource  = ""
  }

  setting {
    namespace = "aws:autoscaling:launchconfiguration"
    name      = "IamInstanceProfile"
    value     = aws_iam_instance_profile.ec2_user.name
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_budget_key_table_name"
    value     = var.pbs_budget_keys_dynamodb_table_name
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_partition_lock_table_name"
    value     = var.pbs_partition_lock_dynamodb_table_name
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_journal_service_bucket_name"
    value     = var.pbs_s3_bucket_name
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_auth_endpoint"
    value     = "${var.auth_api_gateway_base_url}/auth"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_metrics_namespace"
    value     = "${var.environment_prefix}-google-scp-pbs"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_core_cloud_region"
    value     = var.aws_region
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_host_address"
    value     = "0.0.0.0"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_journal_service_partition_name"
    value     = "00000000-0000-0000-0000-000000000000"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_transaction_manager_capacity"
    value     = "15000"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_async_executor_queue_size"
    value     = "100000000"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_io_async_executor_queue_size"
    value     = "100000000"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_transaction_manager_queue_size"
    value     = "100000000"
    resource  = ""
  }

  dynamic "setting" {
    # If the environment variables already contain a remote auth endpoint, then we do not add it.
    # If they don't, then we set the remote auth endpoint as the local one since this is most likey
    # a dev deployment with a single coordinator.
    for_each = (contains(keys(var.application_environment_variables), "google_scp_pbs_remote_auth_endpoint") ? [] :
      [
        {
          namespace = "aws:elasticbeanstalk:application:environment"
          name      = "google_scp_pbs_remote_auth_endpoint"
          value     = "${var.auth_api_gateway_base_url}/auth"
        }
    ])

    content {
      namespace = setting.value.namespace
      name      = setting.value.name
      value     = setting.value.value
      resource  = ""
    }
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_host_port"
    value     = var.pbs_container_port
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_health_port"
    value     = var.pbs_container_health_service_port
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_partition_lease_duration_in_seconds"
    value     = var.pbs_partition_lease_duration_seconds
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_remote_claimed_identity"
    value     = "remote-coordinator.com"
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_remote_assume_role_arn"
    value     = local.remote_role_arn
    resource  = ""
  }

  setting {
    namespace = "aws:elasticbeanstalk:application:environment"
    name      = "google_scp_pbs_remote_assume_role_external_id"
    value     = var.remote_coordinator_aws_account_id
    resource  = ""
  }

  # Set additional environment variables for the application
  dynamic "setting" {
    for_each = var.application_environment_variables
    content {
      namespace = "aws:elasticbeanstalk:application:environment"
      name      = setting.key
      value     = setting.value
      resource  = ""
    }
  }

  lifecycle {
    precondition {
      condition     = var.autoscaling_max_size >= var.autoscaling_min_size
      error_message = "The max size of the autoscaling group must be greater than or equal to the minimum size."
    }
  }

  depends_on = [
    aws_elastic_beanstalk_application_version.app_version,
    null_resource.fail_if_missing_version,
    module.sslcertificate
  ]
}

resource "aws_route53_record" "pbs_domain_record" {
  count   = var.enable_domain_management ? 1 : 0
  zone_id = var.domain_hosted_zone_id
  name    = local.privacy_budget_domain
  type    = "A"

  alias {
    name                   = aws_elastic_beanstalk_environment.beanstalk_app_environment.cname
    zone_id                = data.aws_elastic_beanstalk_hosted_zone.current.id
    evaluate_target_health = false
  }
}
