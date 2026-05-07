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
