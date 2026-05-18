#include <gtest/gtest.h>

#include "autosd_adapters/NatsCloudTransportAdapter.h"

using namespace body_ecu::adapters;

class NatsCloudTransportAdapterStubTest : public ::testing::Test {
protected:
    NatsConfig config_{"nats://test:4222"};
    NatsCloudTransportAdapter adapter_{config_};
};

TEST_F(NatsCloudTransportAdapterStubTest, ConnectSucceeds) {
    EXPECT_TRUE(adapter_.connect());
}

TEST_F(NatsCloudTransportAdapterStubTest, PublishBeforeConnectFails) {
    EXPECT_FALSE(adapter_.publish("test.subject", {0x01}));
}

TEST_F(NatsCloudTransportAdapterStubTest, PublishAfterConnectSucceeds) {
    adapter_.connect();
    EXPECT_TRUE(adapter_.publish("test.subject", {0x01}));
}

TEST_F(NatsCloudTransportAdapterStubTest, SubscribeAndDispatch) {
    adapter_.connect();

    std::string received_subject;
    std::vector<uint8_t> received_data;
    bool called = false;

    adapter_.subscribe("test.subject",
                       [&](const std::string& subject,
                           const std::vector<uint8_t>& data) {
                           received_subject = subject;
                           received_data = data;
                           called = true;
                       });

    std::vector<uint8_t> payload = {0x01, 0x02};
    adapter_.dispatchMessage("test.subject", payload);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_subject, "test.subject");
    EXPECT_EQ(received_data, payload);
}

TEST_F(NatsCloudTransportAdapterStubTest, DispatchToUnsubscribedIgnored) {
    adapter_.connect();

    bool called = false;
    adapter_.subscribe("subject.a",
                       [&](const std::string&,
                           const std::vector<uint8_t>&) { called = true; });

    adapter_.dispatchMessage("subject.b", {0x01});
    EXPECT_FALSE(called);
}

TEST_F(NatsCloudTransportAdapterStubTest, DisconnectPreventsPublish) {
    adapter_.connect();
    adapter_.disconnect();
    EXPECT_FALSE(adapter_.publish("test.subject", {0x01}));
}

TEST_F(NatsCloudTransportAdapterStubTest, SubscribeBeforeConnectNoOp) {
    bool called = false;
    adapter_.subscribe("test.subject",
                       [&](const std::string&,
                           const std::vector<uint8_t>&) { called = true; });

    adapter_.dispatchMessage("test.subject", {0x01});
    EXPECT_FALSE(called);
}

TEST_F(NatsCloudTransportAdapterStubTest, EmptyPayloadDispatch) {
    adapter_.connect();

    std::vector<uint8_t> received;
    adapter_.subscribe("test.subject",
                       [&](const std::string&,
                           const std::vector<uint8_t>& data) { received = data; });

    adapter_.dispatchMessage("test.subject", {});
    EXPECT_TRUE(received.empty());
}

// --- Wildcard dispatch regression tests (P0 #2) ---

TEST_F(NatsCloudTransportAdapterStubTest, WildcardSubscriptionMatchesConcrete) {
    adapter_.connect();

    std::string received_subject;
    std::vector<uint8_t> received_data;
    bool called = false;

    adapter_.subscribe("vehicles.*.command.door.lock",
                       [&](const std::string& subject,
                           const std::vector<uint8_t>& data) {
                           received_subject = subject;
                           received_data = data;
                           called = true;
                       });

    std::vector<uint8_t> payload = {0x01};
    adapter_.dispatchMessage(
        "vehicles.WVWZZZ3CZWE000001.command.door.lock", payload);

    EXPECT_TRUE(called) << "Wildcard * must match a concrete VIN token";
    EXPECT_EQ(received_subject, "vehicles.WVWZZZ3CZWE000001.command.door.lock");
    EXPECT_EQ(received_data, payload);
}

TEST_F(NatsCloudTransportAdapterStubTest, WildcardDoesNotMatchWrongDepth) {
    adapter_.connect();

    bool called = false;
    adapter_.subscribe("vehicles.*.command.door.lock",
                       [&](const std::string&,
                           const std::vector<uint8_t>&) { called = true; });

    adapter_.dispatchMessage("vehicles.extra.segment.command.door.lock", {0x01});
    EXPECT_FALSE(called) << "* matches exactly one token, not multiple";
}

TEST_F(NatsCloudTransportAdapterStubTest, TailWildcardMatchesRemaining) {
    adapter_.connect();

    bool called = false;
    adapter_.subscribe("vehicles.>",
                       [&](const std::string&,
                           const std::vector<uint8_t>&) { called = true; });

    adapter_.dispatchMessage("vehicles.VIN123.command.door.lock", {0x01});
    EXPECT_TRUE(called) << "> must match all remaining tokens";
}

TEST_F(NatsCloudTransportAdapterStubTest, ExactMatchStillWorks) {
    adapter_.connect();

    bool called = false;
    adapter_.subscribe("vehicles.VIN.state",
                       [&](const std::string&,
                           const std::vector<uint8_t>&) { called = true; });

    adapter_.dispatchMessage("vehicles.VIN.state", {0x01});
    EXPECT_TRUE(called);
}

// --- subjectMatchesPattern unit tests ---

using body_ecu::adapters::NatsCloudTransportAdapter;

TEST(NatsWildcardTest, ExactMatch) {
    EXPECT_TRUE(NatsCloudTransportAdapter::subjectMatchesPattern("a.b.c", "a.b.c"));
}

TEST(NatsWildcardTest, StarMatchesSingleToken) {
    EXPECT_TRUE(NatsCloudTransportAdapter::subjectMatchesPattern("a.*.c", "a.b.c"));
    EXPECT_TRUE(NatsCloudTransportAdapter::subjectMatchesPattern("a.*.c", "a.xyz.c"));
    EXPECT_FALSE(NatsCloudTransportAdapter::subjectMatchesPattern("a.*.c", "a.b.d.c"));
}

TEST(NatsWildcardTest, GreaterThanMatchesTail) {
    EXPECT_TRUE(NatsCloudTransportAdapter::subjectMatchesPattern("a.>", "a.b"));
    EXPECT_TRUE(NatsCloudTransportAdapter::subjectMatchesPattern("a.>", "a.b.c.d"));
}

TEST(NatsWildcardTest, NoMatchDifferentSubject) {
    EXPECT_FALSE(NatsCloudTransportAdapter::subjectMatchesPattern("a.b.c", "a.b.d"));
    EXPECT_FALSE(NatsCloudTransportAdapter::subjectMatchesPattern("a.b", "a.b.c"));
}
