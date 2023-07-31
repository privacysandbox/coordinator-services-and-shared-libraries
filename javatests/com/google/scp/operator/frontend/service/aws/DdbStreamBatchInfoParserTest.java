/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.frontend.service.aws;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.aws.DdbStreamBatchInfoParser.DdbStreamBatchInfoParseException;
import com.google.scp.operator.frontend.service.aws.model.DdbStreamBatchInfo;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class DdbStreamBatchInfoParserTest {

  private static final String RESOURCES_DIR =
      "javatests/com/google/scp/operator/frontend/service/aws/resources/";
  // A valid failed stream event message
  private static final String VALID_MESSAGE_PATH = RESOURCES_DIR + "failed_stream_event_valid.json";
  // An invalid failed stream event message with DDBStreamBatchInfo.shardId missing
  private static final String INVALID_MISSING_SHARD_ID_PATH =
      RESOURCES_DIR + "failed_stream_event_invalid_missing_shard_id.json";
  // An invalid failed stream event message with no DDBStreamBatchInfo
  private static final String INVALID_MISSING_STREAM_BATCH_INFO =
      RESOURCES_DIR + "failed_stream_event_invalid_missing_stream_batch_info.json";

  @Rule public Acai acai = new Acai(TestEnv.class);

  @Inject DdbStreamBatchInfoParser ddbStreamBatchInfoParser;

  @Test
  public void testParseValid() throws Exception {
    String messageBody = Files.readString(Path.of(VALID_MESSAGE_PATH));
    DdbStreamBatchInfo expectedBatchInfo =
        DdbStreamBatchInfo.builder()
            .shardId("shardId-00000001632435325753-824df67e")
            .startSequenceNumber("204654000000000042658847971")
            .batchSize(4)
            .streamArn(
                "arn:aws:dynamodb:us-east-1:123456789012:table/"
                    + "JobMetadata/stream/2021-01-01T00:00:00")
            .build();

    DdbStreamBatchInfo ddbStreamBatchInfo =
        ddbStreamBatchInfoParser.batchInfoFromMessageBody(messageBody);

    assertThat(ddbStreamBatchInfo).isEqualTo(expectedBatchInfo);
  }

  @Test
  public void testInvalidNoShardId() throws Exception {
    String messageBody = Files.readString(Path.of(INVALID_MISSING_SHARD_ID_PATH));

    assertThrows(
        DdbStreamBatchInfoParseException.class,
        () -> ddbStreamBatchInfoParser.batchInfoFromMessageBody(messageBody));
  }

  @Test
  public void testInvalidNoDdbStreamBatchInfo() throws Exception {
    String messageBody = Files.readString(Path.of(INVALID_MISSING_STREAM_BATCH_INFO));

    assertThrows(
        DdbStreamBatchInfoParseException.class,
        () -> ddbStreamBatchInfoParser.batchInfoFromMessageBody(messageBody));
  }

  public static final class TestEnv extends AbstractModule {}
}
