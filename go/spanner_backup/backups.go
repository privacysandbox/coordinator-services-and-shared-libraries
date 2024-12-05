// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Package backupfunction is a Cloud function that can be periodically
// triggered to create a backup for the specified database.
package backupfunction

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"time"

	database "cloud.google.com/go/spanner/admin/database/apiv1"
	"github.com/googleapis/gax-go/v2"
)

// Provide flexibility for unit testing without relying on the Spanner service.
type SpannerClient interface {
	StartBackupOperation(ctx context.Context, backupID string, dbName string, expireTime time.Time, opts ...gax.CallOption) (*database.CreateBackupOperation, error)
}

var (
	// client is a global Spanner client, to avoid initializing a new
	// client for every request.
	client     SpannerClient
	clientOnce sync.Once
)

// PubSubMessage is the payload of a Pub/Sub event.
type PubSubMessage struct {
	Data []byte `json:"data"`
}

// BackupParameters is the payload of the `Data` field.
type BackupParameters struct {
	BackupIDPrefix string `json:"backupIdPrefix"`
	Database       string `json:"database"`
	Expire         string `json:"expire"`
}

// SpannerCreateBackup is intended to be called by a scheduled cloud event that
// is passed in as a PubSub message. The PubSubMessage can contain the
// parameters to call the backup function if required.
func SpannerCreateBackup(ctx context.Context, m PubSubMessage) error {
	clientOnce.Do(func() {
		// Declare a separate err variable to avoid shadowing client.
		var err error
		client, err = database.NewDatabaseAdminClient(context.Background())
		if err != nil {
			log.Printf("Failed to create an instance of DatabaseAdminClient: %v", err)
			return
		}
	})
	if client == nil {
		return fmt.Errorf("client should not be nil")
	}
	err := handleBackupRequest(ctx, m, client)
	if err != nil {
		return err
	}
	return nil
}

// handleBackupRequest unmarshals the PubSubMessage, parses the expire duration,
// and initiates a Spanner backup creation using the provided client.
func handleBackupRequest(ctx context.Context, m PubSubMessage, client SpannerClient) error {
	var params BackupParameters
	err := json.Unmarshal(m.Data, &params)
	if err != nil {
		return fmt.Errorf("failed to parse data %s: %v", string(m.Data), err)
	}
	expire, err := time.ParseDuration(params.Expire)
	if err != nil {
		return fmt.Errorf("failed to parse expire duration %s: %v", params.Expire, err)
	}
	_, err = createBackup(ctx, params.BackupIDPrefix, params.Database, expire, client)
	if err != nil {
		return err
	}
	return nil
}

// createBackup starts a backup operation but does not wait for its completion.
func createBackup(ctx context.Context, backupIDPrefix, dbName string, expire time.Duration, client SpannerClient) (*database.CreateBackupOperation, error) {
	now := time.Now()
	backupID := fmt.Sprintf("schedule-%s-%d", backupIDPrefix, now.UTC().Unix())
	expireTime := now.Add(expire)
	op, err := client.StartBackupOperation(ctx, backupID, dbName, expireTime)
	if err != nil {
		return nil, fmt.Errorf("failed to start a backup operation for database [%s], expire time [%s], backupID = [%s] with error = %v", dbName, expireTime.Format(time.RFC3339), backupID, err)
	}
	log.Printf("Create backup operation started for database [%s], expire time [%s], backupID = [%s]", dbName, expireTime.Format(time.RFC3339), backupID)
	return op, nil
}
