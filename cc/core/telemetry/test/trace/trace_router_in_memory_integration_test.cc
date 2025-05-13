//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <variant>

#include "cc/core/telemetry/mock/trace/trace_router_fake.h"
#include "cc/core/telemetry/src/common/trace/trace_utils.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/tracer.h"

namespace privacy_sandbox::pbs_common {
namespace {

using ::opentelemetry::common::AttributeValue;
using ::opentelemetry::sdk::common::OwnedAttributeValue;
using ::opentelemetry::sdk::trace::SpanData;
using ::opentelemetry::sdk::trace::SpanDataEvent;
using ::opentelemetry::sdk::trace::SpanDataLink;
using ::opentelemetry::trace::Provider;
using ::opentelemetry::trace::Span;
using ::opentelemetry::trace::SpanContext;
using ::opentelemetry::trace::StartSpanOptions;
using ::opentelemetry::trace::StatusCode;
using ::opentelemetry::trace::TraceFlags;
using ::opentelemetry::trace::Tracer;
using ::opentelemetry::trace::TracerProvider;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

constexpr absl::string_view kTestSpanName = "test_span";
constexpr absl::string_view kTracerName = "test_tracer";
constexpr absl::string_view kParentSpanName = "parent_span";
constexpr absl::string_view kTracerVersion = "1";
constexpr absl::string_view kTracerSchema = "dummy_schema_url";

MATCHER_P(HasEventWithName, event_name,
          "Checks if the span has an event with the specified name") {
  return arg.GetName() == event_name;
}

void ValidateLinks(const std::vector<SpanDataLink>& links,
                   const SpanContext& expected_context) {
  // Validate the number of links.
  ASSERT_THAT(links, SizeIs(1)) << "Span should have one link";

  // Validate the SpanContext of the link.
  EXPECT_EQ(links[0].GetSpanContext().trace_id(), expected_context.trace_id());
  EXPECT_EQ(links[0].GetSpanContext().span_id(), expected_context.span_id());

  // Validate the link attributes.
  const SpanDataLink& link = links[0];
  const std::unordered_map<std::string, OwnedAttributeValue>& link_attributes =
      link.GetAttributes();

  EXPECT_THAT(
      link_attributes,
      UnorderedElementsAre(
          Pair("link_attribute1", VariantWith<std::string>("link_value1")),
          Pair("link_attribute2", VariantWith<int>(123))));
}

class TraceRouterInMemoryIntegrationTest : public testing::Test {
 protected:
  void SetUp() override { trace_router_ = std::make_unique<TraceRouterFake>(); }

  std::unique_ptr<TraceRouterFake> trace_router_;
};

TEST_F(TraceRouterInMemoryIntegrationTest, SuccessfulTraceFakeInitialization) {
  ASSERT_NE(&trace_router_->GetSpanExporter(), nullptr);
  ASSERT_NE(Provider::GetTracerProvider(), nullptr);
  absl::flat_hash_map<std::string, std::vector<const SpanData*>>
      exported_traces = trace_router_->GetExportedTraces();
  EXPECT_THAT(exported_traces, IsEmpty());

  std::vector<const SpanData*> spans =
      trace_router_->GetSpansForTrace(/*trace_id=*/"fake_id");
  EXPECT_THAT(spans, IsEmpty());
}

// This test validates the behavior of routing and exporting trace data,
// ensuring that the span data, including attributes, links (A link is a
// reference to another span in the same or a different trace.), status, and
// events, is correctly captured and available in the exported trace structure.
// The test simulates a span with specific attributes, links, status, and
// events, retrieves the exported trace data, and verifies each field.
//
// Sample exported data:
// {
//   scope name    : test_tracer
//   schema url    : dummy_schema_url
//   version       : 1
//   span name     : test_span
//   start time    : Wed Feb 28 01:25:05 2024
//   end time      : Wed Feb 28 01:25:07 2024
//   attributes    :
//     attribute1 : value1
//     attribute2 : 42
//   status        : OK
//   links         :
//     trace_id    : <linked_trace_id>
//     span_id     : <linked_span_id>
//   events        :
//     {name: event1, timestamp: <timestamp>, attributes: {}}
//   resources     :
//     service.name               : unknown_service
//     telemetry.sdk.language     : cpp
//     telemetry.sdk.name         : opentelemetry
//     telemetry.sdk.version      : 1.13.0
// }
TEST_F(TraceRouterInMemoryIntegrationTest,
       SuccessfulSpanDataExportUsingGlobalTraceProvider) {
  // Setup.
  std::shared_ptr<TracerProvider> tracer_provider =
      Provider::GetTracerProvider();
  ASSERT_NE(tracer_provider, nullptr) << "Tracer provider should not be null";

  std::shared_ptr<Tracer> tracer =
      tracer_provider->GetTracer(kTracerName, kTracerVersion, kTracerSchema);
  ASSERT_NE(tracer, nullptr) << "Tracer should not be null";

  std::shared_ptr<Span> span = tracer->StartSpan(kTestSpanName);
  ASSERT_NE(span, nullptr) << "Span should not be null";
  span->SetAttribute("attribute1", "value1");
  span->SetAttribute("attribute2", 42);

  SpanContext linked_context(/*trace_id=*/{}, /*span_id=*/{}, TraceFlags(),
                             /*is_remote=*/false);
  std::unordered_map<std::string, AttributeValue> link_attributes = {
      {"link_attribute1", "link_value1"}, {"link_attribute2", 123}};
  span->AddLink(linked_context, link_attributes);

  span->SetStatus(StatusCode::kOk, "Operation completed successfully");
  span->AddEvent("event1");
  span->End();

  // Validations.
  SpanContext span_context = span->GetContext();
  ASSERT_TRUE(span_context.IsValid()) << "Span context should be valid";
  std::string trace_id = GetTraceIdString(span_context.trace_id());

  absl::flat_hash_map<std::string, std::vector<const SpanData*>>
      exported_traces = trace_router_->GetExportedTraces();
  ASSERT_THAT(exported_traces, UnorderedElementsAre(Pair(trace_id, SizeIs(1))))
      << "Only one trace with the correct span should be exported";

  const std::vector<const SpanData*>& span_data_vector =
      exported_traces[trace_id];
  ASSERT_THAT(span_data_vector, SizeIs(1))
      << "Exported trace should contain exactly one span";

  const SpanData* span_data = span_data_vector[0];
  ASSERT_NE(span_data, nullptr) << "Span data should not be null";
  EXPECT_THAT(span_data->GetName(), StrEq(kTestSpanName))
      << "Span name mismatch";

  EXPECT_THAT(span_data->GetAttributes(),
              UnorderedElementsAre(
                  Pair("attribute1", VariantWith<std::string>("value1")),
                  Pair("attribute2", VariantWith<int>(42))));
  ValidateLinks(span_data->GetLinks(), linked_context);
  EXPECT_THAT(span_data->GetEvents(),
              testing::ElementsAre(HasEventWithName("event1")))
      << "Expected exactly one event with the name 'event1'";

  EXPECT_EQ(span_data->GetStatus(), StatusCode::kOk) << "Span status mismatch";

  EXPECT_GT(span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Start time should be greater than zero";
  EXPECT_GT(span_data->GetDuration().count(), 0)
      << "Duration should be greater than zero";
}

// This test validates the behavior of routing and exporting trace data,
// ensuring that the span data, including attributes, links, status, events,
// and parent-child relationships (context propagation), is correctly captured
// and available in the exported trace structure.
// The test simulates a parent-child span relationship with specific attributes,
// links, status, and events, retrieves the exported trace data, and verifies
// each field.
//
// Sample exported data:
// {
//   scope name    : test_tracer
//   schema url    : dummy_schema_url
//   version       : 1
//   parent span name: parent_span
//   child span name : test_span
//   attributes    :
//     attribute1 : value1
//     attribute2 : 42
//   status        : OK
//   links         :
//     trace_id    : <linked_trace_id>
//     span_id     : <linked_span_id>
//   events        :
//     {name: event1, timestamp: <timestamp>, attributes: {}}
//   resources     :
//     service.name               : unknown_service
//     telemetry.sdk.language     : cpp
//     telemetry.sdk.name         : opentelemetry
//     telemetry.sdk.version      : 1.13.0
// }
TEST_F(TraceRouterInMemoryIntegrationTest,
       SuccessfulSpanDataExportWithContextPropagationOfMultipleSpans) {
  // Setup.
  // Retrieve the global tracer provider.
  std::shared_ptr<TracerProvider> tracer_provider =
      Provider::GetTracerProvider();
  ASSERT_NE(tracer_provider, nullptr) << "Tracer provider should not be null";

  // Retrieve a tracer with the specified name, version, and schema URL.
  std::shared_ptr<Tracer> tracer =
      tracer_provider->GetTracer(kTracerName, kTracerVersion, kTracerSchema);
  ASSERT_NE(tracer, nullptr) << "Tracer should not be null";

  // Start a parent span and set attributes.
  std::shared_ptr<Span> parent_span = tracer->StartSpan(kParentSpanName);
  ASSERT_NE(parent_span, nullptr) << "Parent span should not be null";
  parent_span->SetAttribute("parent_attribute1", "parent_value1");

  SpanContext linked_context(
      /*trace_id=*/{}, /*span_id=*/{}, TraceFlags(),
      /*is_remote=*/false);
  std::unordered_map<std::string, AttributeValue> link_attributes = {
      {"link_attribute1", "link_value1"}, {"link_attribute2", 123}};

  // Scoped block for child span propagation.
  {
    // Retrieve the context from the parent span.
    SpanContext parent_context = parent_span->GetContext();
    ASSERT_TRUE(parent_context.IsValid()) << "Parent context should be valid";

    // Start a child span with the parent context.
    StartSpanOptions child_span_options;
    child_span_options.parent = parent_context;

    std::shared_ptr<Span> child_span =
        tracer->StartSpan("test_span", child_span_options);
    ASSERT_NE(child_span, nullptr) << "Child span should not be null";
    child_span->SetAttribute("attribute1", "value1");
    child_span->SetAttribute("attribute2", 42);

    // Add a link to another trace/span.
    child_span->AddLink(linked_context, link_attributes);

    // Set status and add an event to the child span.
    child_span->SetStatus(StatusCode::kOk, "Operation completed successfully");
    child_span->AddEvent(/*name=*/"event1");

    // End the child span to trigger the export of trace data.
    child_span->End();

    // Validate the exported trace data for the child span.
    SpanContext child_context = child_span->GetContext();
    ASSERT_TRUE(child_context.IsValid()) << "Child context should be valid";
    std::string trace_id = GetTraceIdString(child_context.trace_id());

    absl::flat_hash_map<std::string, std::vector<const SpanData*>>
        exported_traces = trace_router_->GetExportedTraces();
    ASSERT_THAT(exported_traces,
                UnorderedElementsAre(Pair(trace_id, SizeIs(1))))
        << "Only one child span should be exported";
  }

  // End the parent span and validate exported trace data for both spans.
  parent_span->End();

  SpanContext parent_context = parent_span->GetContext();
  ASSERT_TRUE(parent_context.IsValid()) << "Parent context should be valid";
  std::string trace_id = GetTraceIdString(parent_context.trace_id());

  absl::flat_hash_map<std::string, std::vector<const SpanData*>>
      exported_traces = trace_router_->GetExportedTraces();
  ASSERT_THAT(exported_traces, UnorderedElementsAre(Pair(trace_id, SizeIs(2))))
      << "Trace should contain both parent and child spans";

  // Validate parent and child span data.
  const std::vector<const SpanData*>& span_data_vector =
      exported_traces[trace_id];
  ASSERT_THAT(span_data_vector, SizeIs(2)) << "Span data vector size mismatch";

  // Validate the parent span data.
  const SpanData* parent_span_data = span_data_vector[1];
  ASSERT_NE(parent_span_data, nullptr) << "Parent span data should not be null";
  EXPECT_EQ(parent_span_data->GetName(), kParentSpanName)
      << "Parent span name mismatch";

  // Validate the child span data.
  const SpanData* child_span_data = span_data_vector[0];
  ASSERT_NE(child_span_data, nullptr) << "Child span data should not be null";
  EXPECT_EQ(child_span_data->GetName(), "test_span")
      << "Child span name mismatch";

  EXPECT_THAT(child_span_data->GetAttributes(),
              UnorderedElementsAre(
                  Pair("attribute1", VariantWith<std::string>("value1")),
                  Pair("attribute2", VariantWith<int>(42))));
  ValidateLinks(child_span_data->GetLinks(), linked_context);
  EXPECT_THAT(child_span_data->GetEvents(),
              testing::ElementsAre(HasEventWithName("event1")))
      << "Expected exactly one event with the name 'event1'";

  // Verify start and end times for both parent and child spans.
  EXPECT_GT(parent_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Parent span start time invalid";
  EXPECT_GT(parent_span_data->GetDuration().count(), 0)
      << "Parent span duration invalid";
  EXPECT_GT(child_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Child span start time invalid";
  EXPECT_GT(child_span_data->GetDuration().count(), 0)
      << "Child span duration invalid";

  // Ensure parent span duration is greater than or equal to child span
  // duration.
  EXPECT_GE(parent_span_data->GetDuration().count(),
            child_span_data->GetDuration().count())
      << "Parent span duration should be greater than or equal to child span "
         "duration";
}

TEST_F(TraceRouterInMemoryIntegrationTest,
       SuccessfulSpanDataExportWithContextPropagationAndDifferentTracers) {
  // Get the global tracer provider.
  std::shared_ptr<TracerProvider> tracer_provider =
      Provider::GetTracerProvider();
  ASSERT_NE(tracer_provider, nullptr) << "Tracer provider should not be null";

  // Retrieve tracers for different names.
  std::shared_ptr<Tracer> tracer1 =
      tracer_provider->GetTracer("test_tracer1", kTracerVersion, kTracerSchema);
  std::shared_ptr<Tracer> tracer2 =
      tracer_provider->GetTracer("test_tracer2", kTracerVersion, kTracerSchema);
  ASSERT_NE(tracer1, nullptr) << "Tracer1 should not be null";
  ASSERT_NE(tracer2, nullptr) << "Tracer2 should not be null";

  // Start a parent span with some attributes.
  std::shared_ptr<Span> parent_span = tracer1->StartSpan(kParentSpanName);
  ASSERT_NE(parent_span, nullptr) << "Parent span should not be null";
  parent_span->SetAttribute("parent_attribute1", "parent_value1");

  SpanContext linked_context(
      /*trace_id=*/{}, /*span_id=*/{}, TraceFlags(),
      /*is_remote=*/false);
  std::unordered_map<std::string, AttributeValue> link_attributes = {
      {"link_attribute1", "link_value1"}, {"link_attribute2", 123}};

  // Create a scoped span to propagate the parent context.
  {
    SpanContext parent_context = parent_span->GetContext();
    ASSERT_TRUE(parent_context.IsValid()) << "Parent context should be valid";

    // Start a child span with the parent context.
    StartSpanOptions child_span_options;
    child_span_options.parent = parent_context;
    std::shared_ptr<Span> child_span =
        tracer2->StartSpan("test_span", child_span_options);
    ASSERT_NE(child_span, nullptr) << "Child span should not be null";

    child_span->SetAttribute("attribute1", "value1");
    child_span->SetAttribute("attribute2", 42);

    // Add a link to another trace/span.
    child_span->AddLink(linked_context, link_attributes);

    // Set status and add an event to the child span.
    child_span->SetStatus(StatusCode::kOk, "Operation completed successfully");
    child_span->AddEvent("event1");

    // End the child span to trigger export of trace data.
    child_span->End();

    // Retrieve the child trace ID. This would be same as parent trace id due to
    // context propagation.
    SpanContext child_context = child_span->GetContext();
    ASSERT_TRUE(child_context.IsValid()) << "Child context should be valid";
    std::string trace_id = GetTraceIdString(child_context.trace_id());

    // Retrieve and validate exported trace data.
    absl::flat_hash_map<std::string, std::vector<const SpanData*>>
        exported_traces = trace_router_->GetExportedTraces();
    EXPECT_THAT(exported_traces,
                UnorderedElementsAre(Pair(trace_id, SizeIs(1))))
        << "Only one trace should be exported, containing one child span";
  }

  // End the parent span.
  parent_span->End();

  // Retrieve the trace ID. Trace ID would be same for parent span and child
  // span as we have propagated the context.
  // Check that only one trace was exported (even when two separate tracers were
  // used), and it contains the expected trace ID and the associated vector of
  // span data has exactly two elements - parent span and child span.
  SpanContext parent_context = parent_span->GetContext();
  ASSERT_TRUE(parent_context.IsValid()) << "Parent context should be valid";
  std::string trace_id = GetTraceIdString(parent_context.trace_id());
  absl::flat_hash_map<std::string, std::vector<const SpanData*>>
      exported_traces = trace_router_->GetExportedTraces();
  EXPECT_THAT(exported_traces, UnorderedElementsAre(Pair(trace_id, SizeIs(2))))
      << "One trace should be exported, containing parent and child spans";

  // Validate span data.
  const std::vector<const SpanData*>& span_data_vector =
      exported_traces[trace_id];
  EXPECT_EQ(span_data_vector.size(), 2)
      << "Trace should contain exactly two spans";

  // Validate parent span data.
  const SpanData* parent_span_data = span_data_vector[1];
  ASSERT_NE(parent_span_data, nullptr) << "Parent span data should not be null";
  EXPECT_EQ(parent_span_data->GetName(), kParentSpanName)
      << "Parent span name mismatch";

  // Validate child span data.
  const SpanData* child_span_data = span_data_vector[0];
  ASSERT_NE(child_span_data, nullptr) << "Child span data should not be null";
  EXPECT_EQ(child_span_data->GetName(), "test_span")
      << "Child span name mismatch";

  EXPECT_THAT(child_span_data->GetAttributes(),
              UnorderedElementsAre(
                  Pair("attribute1", VariantWith<std::string>("value1")),
                  Pair("attribute2", VariantWith<int>(42))));
  ValidateLinks(child_span_data->GetLinks(), linked_context);
  EXPECT_THAT(child_span_data->GetEvents(),
              testing::ElementsAre(HasEventWithName("event1")))
      << "Expected exactly one event with the name 'event1'";

  // Verify span start and end times.
  EXPECT_GT(parent_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Parent span start time invalid";
  EXPECT_GT(parent_span_data->GetDuration().count(), 0)
      << "Parent span duration invalid";
  EXPECT_GT(child_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Child span start time invalid";
  EXPECT_GT(child_span_data->GetDuration().count(), 0)
      << "Child span duration invalid";
  EXPECT_GE(parent_span_data->GetDuration().count(),
            child_span_data->GetDuration().count())
      << "Parent span duration should be greater than or equal to child span "
         "duration";
}

TEST_F(TraceRouterInMemoryIntegrationTest,
       SuccessfulSpanDataExportUsingSpanName) {
  // Retrieve the global tracer provider.
  std::shared_ptr<TracerProvider> tracer_provider =
      Provider::GetTracerProvider();
  ASSERT_NE(tracer_provider, nullptr) << "Tracer provider should not be null";

  // Retrieve a tracer with the specified name, version, and schema URL.
  std::shared_ptr<Tracer> tracer =
      tracer_provider->GetTracer(kTracerName, kTracerVersion, kTracerSchema);
  ASSERT_NE(tracer, nullptr) << "Tracer should not be null";

  // Start a span and set specific attributes.
  std::shared_ptr<Span> span = tracer->StartSpan("test_span");
  ASSERT_NE(span, nullptr) << "Span should not be null";

  // Set the status for the span.
  span->SetStatus(StatusCode::kOk, "Operation completed successfully");

  // End the span to trigger the export of trace data.
  span->End();

  // Retrieve the spans collected for the given span name.
  std::vector<const SpanData*> span_data_vector =
      trace_router_->GetSpansForSpanName("test_span");
  ASSERT_EQ(span_data_vector.size(), 1)
      << "Exactly one span should be exported";

  const SpanData* span_data = span_data_vector[0];
  ASSERT_NE(span_data, nullptr) << "Span data should not be null";

  // Validate the span name.
  EXPECT_EQ(span_data->GetName(), "test_span") << "Span name mismatch";

  // Verify the span status.
  EXPECT_EQ(span_data->GetStatus(), StatusCode::kOk) << "Span status mismatch";
}

TEST_F(TraceRouterInMemoryIntegrationTest,
       SuccessfulSpanDataExportWithDifferentTracersUsingSpanName) {
  // Retrieve the global tracer provider.
  std::shared_ptr<TracerProvider> tracer_provider =
      Provider::GetTracerProvider();
  ASSERT_NE(tracer_provider, nullptr) << "Tracer provider should not be null";

  // Retrieve tracers for different names.
  std::shared_ptr<Tracer> tracer1 =
      tracer_provider->GetTracer("test_tracer1", kTracerVersion, kTracerSchema);
  std::shared_ptr<Tracer> tracer2 =
      tracer_provider->GetTracer("test_tracer2", kTracerVersion, kTracerSchema);
  ASSERT_NE(tracer1, nullptr) << "Tracer1 should not be null";
  ASSERT_NE(tracer2, nullptr) << "Tracer2 should not be null";

  // Start a parent span with some attributes.
  std::shared_ptr<Span> parent_span = tracer1->StartSpan(kParentSpanName);
  ASSERT_NE(parent_span, nullptr) << "Parent span should not be null";
  parent_span->SetAttribute("parent_attribute1", "parent_value1");

  SpanContext linked_context(
      /*trace_id=*/{}, /*span_id=*/{}, TraceFlags(),
      /*is_remote=*/false);
  std::unordered_map<std::string, AttributeValue> link_attributes = {
      {"link_attribute1", "link_value1"}, {"link_attribute2", 123}};
  // Start a child span with the parent context.
  {
    SpanContext parent_context = parent_span->GetContext();
    ASSERT_TRUE(parent_context.IsValid()) << "Parent context should be valid";

    StartSpanOptions child_span_options;
    child_span_options.parent = parent_context;

    std::shared_ptr<Span> child_span =
        tracer2->StartSpan("test_span", child_span_options);
    ASSERT_NE(child_span, nullptr) << "Child span should not be null";
    child_span->SetAttribute("attribute1", "value1");
    child_span->SetAttribute("attribute2", 42);

    // Add a link to another trace/span.
    child_span->AddLink(linked_context, link_attributes);

    // Set status and add an event to the child span.
    child_span->SetStatus(StatusCode::kOk, "Operation completed successfully");
    child_span->AddEvent("event1");

    // End the child span.
    child_span->End();
  }

  // End the parent span.
  parent_span->End();

  // Retrieve the exported spans by span name.
  absl::flat_hash_map<std::string, std::vector<const SpanData*>>
      exported_spans = trace_router_->GetExportedSpansBySpanName();

  // Validate the presence of parent and child spans.
  ASSERT_THAT(exported_spans,
              UnorderedElementsAre(Pair(kParentSpanName, SizeIs(1)),
                                   Pair("test_span", SizeIs(1))))
      << "Exported spans should contain both parent and child spans";

  // Validate parent span data.
  const std::vector<const SpanData*>& parent_span_data_vector =
      exported_spans[kParentSpanName];
  ASSERT_EQ(parent_span_data_vector.size(), 1)
      << "Parent span vector size mismatch";
  const SpanData* parent_span_data = parent_span_data_vector[0];
  ASSERT_NE(parent_span_data, nullptr) << "Parent span data should not be null";
  EXPECT_EQ(parent_span_data->GetName(), kParentSpanName)
      << "Parent span name mismatch";

  // Validate child span data.
  const std::vector<const SpanData*>& child_span_data_vector =
      exported_spans["test_span"];
  ASSERT_THAT(child_span_data_vector, SizeIs(1))
      << "Child span vector size mismatch";

  const SpanData* child_span_data = child_span_data_vector[0];
  ASSERT_NE(child_span_data, nullptr) << "Child span data should not be null";

  EXPECT_EQ(child_span_data->GetName(), "test_span")
      << "Child span name mismatch";

  EXPECT_THAT(child_span_data->GetAttributes(),
              UnorderedElementsAre(
                  Pair("attribute1", VariantWith<std::string>("value1")),
                  Pair("attribute2", VariantWith<int>(42))));
  ValidateLinks(child_span_data->GetLinks(), linked_context);
  EXPECT_THAT(child_span_data->GetEvents(),
              testing::ElementsAre(HasEventWithName("event1")))
      << "Expected exactly one event with the name 'event1'";

  // Verify start and end times for both spans.
  EXPECT_GT(parent_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Parent span start time invalid";
  EXPECT_GT(parent_span_data->GetDuration().count(), 0)
      << "Parent span duration invalid";
  EXPECT_GT(child_span_data->GetStartTime().time_since_epoch().count(), 0)
      << "Child span start time invalid";
  EXPECT_GT(child_span_data->GetDuration().count(), 0)
      << "Child span duration invalid";

  // Verify parent span duration is greater than or equal to child span
  // duration.
  EXPECT_GE(parent_span_data->GetDuration().count(),
            child_span_data->GetDuration().count())
      << "Parent span duration should be greater than or equal to child span "
         "duration";
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
