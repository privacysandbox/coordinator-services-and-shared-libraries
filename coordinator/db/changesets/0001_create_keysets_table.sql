START BATCH DDL;

CREATE TABLE KeySets(
  KeyId STRING(50) NOT NULL,
  PublicKey STRING(1000) NOT NULL,
  PrivateKey STRING(1000) NOT NULL,
  PublicKeyMaterial STRING(500) NOT NULL,
  KeySplitData JSON,
  KeyType STRING(500) NOT NULL,
  KeyEncryptionKeyUri STRING(1000) NOT NULL,
  ExpiryTime TIMESTAMP NOT NULL,
  TtlTime TIMESTAMP NOT NULL,
  CreatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp = TRUE),
  UpdatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp = TRUE),
  ActivationTime TIMESTAMP
) PRIMARY KEY(KeyId);

CREATE
  INDEX KeySetsByExpiryActivationDesc
ON KeySets(ExpiryTime DESC, ActivationTime DESC);

RUN BATCH;
