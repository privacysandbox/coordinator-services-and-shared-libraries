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

package backupfunction

import (
	"context"
	"errors"
	"testing"
	"time"

	database "cloud.google.com/go/spanner/admin/database/apiv1"
	"github.com/googleapis/gax-go/v2"
	"github.com/stretchr/testify/assert"
)

// MockSpannerClient is a mock implementation of the SpannerClient interface
type MockSpannerClient struct {
}

// StartBackupOperation mocks the StartBackupOperation method
func (*MockSpannerClient) StartBackupOperation(ctx context.Context, backupID, dbName string, expireTime time.Time, opts ...gax.CallOption) (*database.CreateBackupOperation, error) {
	return &database.CreateBackupOperation{}, nil
}

// TestSpannerCreateBackup tests the SpannerCreateBackup function
func TestSpannerCreateBackup(t *testing.T) {
	// Create a mock instance
	mockClient := &MockSpannerClient{}

	tests := []struct {
		name          string
		pubSubMessage PubSubMessage
		wantErr       error
	}{
		{
			name: "Success",
			pubSubMessage: PubSubMessage{
				Data: []byte(`{
					"backupIdPrefix": "test-prefix",
					"database": "test-database",
					"expire": "1h"
				}`),
			},
			wantErr: nil,
		},
		{
			name: "UnmarshalError",
			pubSubMessage: PubSubMessage{
				Data: []byte("invalid json"),
			},
			wantErr: errors.New("failed to parse data invalid json: invalid character 'i' looking for beginning of value"),
		},
		{
			name: "ParseExpireError",
			pubSubMessage: PubSubMessage{
				Data: []byte(`{
					"backupIdPrefix": "test-prefix",
					"database": "test-database",
					"expire": "invalid-duration"
				}`),
			},
			wantErr: errors.New("failed to parse expire duration invalid-duration: time: invalid duration \"invalid-duration\""),
		},
	}
	for _, tt := range tests {
		err := handleBackupRequest(context.Background(), tt.pubSubMessage, mockClient)
		assert.Equal(t, tt.wantErr, err)
	}
}
