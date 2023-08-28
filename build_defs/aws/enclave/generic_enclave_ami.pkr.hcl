// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

packer {
  required_plugins {
    amazon = {
      version = ">= 0.0.1"
      source  = "github.com/hashicorp/amazon"
    }
  }
}

locals {
  //AMI naming does not support some special characters
  timestamp = formatdate("YYYY-MM-DD'T'hh-mm-ssZ", timestamp())

  # List of additional systemd configuration to add to enclave watcher.
  enclave_watcher_overrides = [
    {enable_enclave_debug_mode} ? "Environment=ENABLE_ENCLAVE_DEBUG_MODE=1" : "",
  ]
}

source "amazon-ebs" "amzn2-ami" {
  ami_name      = "{ami_name}--${local.timestamp}"
  instance_type = "{ec2_instance}"
  region        = "{aws_region}"
  ami_groups    = {ami_groups}
  subnet_id     = "{subnet_id}"

  // Custom Base AMI
  source_ami_filter {
    filters = {
      name                = "amzn2-ami-hvm-*-x86_64-ebs"
      root-device-type    = "ebs"
      virtualization-type = "hvm"
    }
    most_recent = true
    owners = [
      "amazon"]
  }
  ssh_username = "ec2-user"
  ssh_timeout = "15m"
}

build {
  sources = [
    "source.amazon-ebs.amzn2-ami"
  ]

  provisioner "file" {
    source = "{licenses}"
    destination = "/tmp/licenses.tar"
  }

  provisioner "file" {
    source      = "{container_path}"
    destination = "/tmp/{container_filename}"
  }

  provisioner "shell" {
    inline = ["mkdir /tmp/rpms"]
  }

  provisioner "file" {
    sources     = [
      "{proxy_rpm}",
      "{enclave_watcher_rpm}",
    ]
    destination = "/tmp/rpms/"
  }

  provisioner "file" {
    source      = "{enclave_allocator}"
    destination = "/tmp/allocator.yaml"
  }

  provisioner "shell" {
    inline = [
      # Install and enable Docker.
      "sudo amazon-linux-extras install docker",
      "sudo systemctl enable docker",
      "sudo systemctl start docker",
      # Build enclave
      "sudo docker load --input /tmp/{container_filename}",
      "sudo amazon-linux-extras install aws-nitro-enclaves-cli",
      "sudo yum install aws-nitro-enclaves-cli-devel -y",
      "sudo systemctl enable nitro-enclaves-allocator.service",
      # Install helper services
      "sudo rpm -i /tmp/rpms/*.rpm",
      "sudo systemctl enable vsockproxy",
      "sudo systemctl enable enclave-watcher",
      # This script runs in the boot phase in which environment variables are not set.
      # This variable is needed to run nitro-cli build-enclave.
      "export NITRO_CLI_ARTIFACTS=\"/tmp/enclave-images/\"",
      "sudo nitro-cli build-enclave --docker-uri bazel/{docker_repo}:{docker_tag} --output-file /opt/google/scp/enclave.eif",
      # Move config files which require elevated permissions.",
      # https://github.com/hashicorp/packer/issues/1551#issuecomment-59131451",
      "sudo mv /tmp/allocator.yaml /etc/nitro_enclaves/allocator.yaml",
      "sudo mkdir /licenses",
      "sudo tar xf /tmp/licenses.tar -C /licenses",
    ]
  }

  # Populate enclave overrides file.
  provisioner "file" {
    content = <<-EOF
    [Service]
    ${join("\n", local.enclave_watcher_overrides)}
    EOF
    destination = "/tmp/enclave-watcher_override.conf"
  }

  # Create enclave watcher overrides directory and move override there;
  provisioner "shell" {
    inline = [
      "sudo mkdir /etc/systemd/system/enclave-watcher.service.d/",
      "sudo mv /tmp/enclave-watcher_override.conf /etc/systemd/system/enclave-watcher.service.d/override.conf",
      # Clean up files used by provision_script
      "sudo rm -rf /tmp/rpms",
      "sudo rm -f /tmp/{container_filename}",
      "sudo rm -f /tmp/allocator.yaml",
      "sudo rm -f /tmp/licenses.tar",
    ]
  }

  provisioner "shell" {
    # This should be the last step because it removes access from Packer.

    environment_vars = [
      "UNINSTALL_SSH_SERVER={uninstall_ssh_server}",
    ]

    inline = [
      # Standard AMI cleanup per:
      # https://aws.amazon.com/articles/how-to-share-and-use-public-amis-in-a-secure-manner/
      # This removes the auto-generated Packer key (created in
      # /home/ec2-user/.ssh/authorized_keys).
      "sudo find / -name authorized_keys -delete -print",
      # Note: omitting deletion of shell history and VCS files because they are
      # not present in these AMIs.
      "if ( $UNINSTALL_SSH_SERVER ); then sudo rpm -e ec2-instance-connect openssh-server; fi",
    ]
  }
}
