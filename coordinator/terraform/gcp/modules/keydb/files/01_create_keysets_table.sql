CREATE TABLE KeySets (
  KeyId STRING(50) NOT NULL,
  PublicKey STRING(1000) NOT NULL,
  PrivateKey STRING(1000) NOT NULL,
  PublicKeyMaterial STRING(500) NOT NULL,
  KeySplitData JSON,
  KeyType STRING(500) NOT NULL,
  KeyEncryptionKeyUri STRING(1000) NOT NULL,
  ExpiryTime TIMESTAMP NOT NULL,
  TtlTime TIMESTAMP NOT NULL,
  CreatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true),
  UpdatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true))
PRIMARY KEY (KeyId),
ROW DELETION POLICY (OLDER_THAN(TtlTime, INTERVAL 0 DAY))
