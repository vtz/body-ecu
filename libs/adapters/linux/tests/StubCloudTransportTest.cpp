#include <gtest/gtest.h>

#include "linux_adapters/StubCloudTransport.h"

using namespace body_ecu::adapters;

class StubCloudTransportTest : public ::testing::Test {
protected:
    StubCloudTransport transport_;
};

TEST_F(StubCloudTransportTest, ConnectSucceeds) {
    EXPECT_TRUE(transport_.connect());
    EXPECT_TRUE(transport_.isConnected());
}

TEST_F(StubCloudTransportTest, DisconnectClearsState) {
    transport_.connect();
    transport_.disconnect();
    EXPECT_FALSE(transport_.isConnected());
}

TEST_F(StubCloudTransportTest, PublishBeforeConnectStillSucceeds) {
    // Stub transport is intentionally permissive -- always succeeds
    std::vector<uint8_t> data = {0x01};
    EXPECT_TRUE(transport_.publish("test.subject", data));
}

TEST_F(StubCloudTransportTest, PublishAfterConnectSucceeds) {
    transport_.connect();
    std::vector<uint8_t> data = {0x01};
    EXPECT_TRUE(transport_.publish("test.subject", data));
}

TEST_F(StubCloudTransportTest, SubscribeAndInjectMessage) {
    transport_.connect();

    std::string received_subject;
    std::vector<uint8_t> received_data;
    bool called = false;

    transport_.subscribe("test.subject",
                         [&](const std::string& subject,
                             const std::vector<uint8_t>& data) {
                             received_subject = subject;
                             received_data = data;
                             called = true;
                         });

    std::vector<uint8_t> payload = {0x42, 0x43};
    transport_.injectMessage("test.subject", payload);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_subject, "test.subject");
    EXPECT_EQ(received_data, payload);
}

TEST_F(StubCloudTransportTest, InjectToUnsubscribedSubjectDoesNothing) {
    transport_.connect();

    bool called = false;
    transport_.subscribe("subject.a",
                         [&](const std::string&,
                             const std::vector<uint8_t>&) {
                             called = true;
                         });

    transport_.injectMessage("subject.b", {0x01});
    EXPECT_FALSE(called);
}

TEST_F(StubCloudTransportTest, MultipleSubscribersReceiveMessage) {
    transport_.connect();

    int call_count = 0;
    auto callback = [&](const std::string&, const std::vector<uint8_t>&) {
        call_count++;
    };

    transport_.subscribe("test.subject", callback);
    transport_.subscribe("test.subject", callback);

    transport_.injectMessage("test.subject", {0x01});
    EXPECT_EQ(call_count, 2);
}

TEST_F(StubCloudTransportTest, PublishEmptyPayload) {
    transport_.connect();
    std::vector<uint8_t> empty;
    EXPECT_TRUE(transport_.publish("test.subject", empty));
}

TEST_F(StubCloudTransportTest, InjectEmptyPayload) {
    transport_.connect();

    std::vector<uint8_t> received;
    transport_.subscribe("test.subject",
                         [&](const std::string&,
                             const std::vector<uint8_t>& data) {
                             received = data;
                         });

    transport_.injectMessage("test.subject", {});
    EXPECT_TRUE(received.empty());
}
