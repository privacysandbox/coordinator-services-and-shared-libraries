# Changelog

## [1.2.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.1.1...v1.2.0) (2023-08-24)

### Changes
  * Fixed terraform plan output
  * Extended operator lifecycle hook timeout
  * Added Codebuild terraform and instructions

## [1.1.1](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.1.0...v1.1.1) (2023-08-08)

### Changes
  * Ensured ACL is set on CloudFront logs bucket by adding an explicit dependency
  * Added option to uninstall SSH server from AMI
  * Pinned and locked versions for nsm-api dependencies
  * Enforced newer SSLPolicy on ELB LB

## [1.1.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.0.0...v1.1.0) (2023-07-27)

### Changes
- Fixed Alert-Queue by updating region
- Fixed kmstool build by upgrading rust toolchain to 1.63 and pinning the nsm-api dependencies
- Added retry with exponential back off for decryption service
- Added new error code to map private key endpoint
- Improved ConstantJobClient usability for testing

## [1.0.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v0.51.15...v1.0.0) (2023-07-10)

### Changes

- Release of first major version v1.0.0 including open sourcing of coordinator services source code
