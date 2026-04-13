#include <gtest/gtest.h>

#include "MockCloudTransport.h"
#include "MockSignalBus.h"
#include "cloud_gateway/CloudGatewayClient.h"

using namespace body_ecu;
using namespace body_ecu::platform;
using namespace body_ecu::mocks;
using ::testing::_;
using ::testing::Return;

class CloudGatewayClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(transport_, connect()).WillOnce(Return(true));
        EXPECT_CALL(transport_, subscribe(_, _))
            .WillOnce([this](const std::string&,
                             ports::CloudMessageCallback cb) {
                cloud_cmd_cb_ = cb;
            });
        EXPECT_CALL(signal_bus_, subscribe(config_.signal_is_locked, _))
            .WillOnce([this](const std::string&,
                             ports::SignalCallback cb) {
                lock_state_cb_ = cb;
            });
        EXPECT_CALL(signal_bus_, subscribe(config_.signal_command_response, _))
            .WillOnce([this](const std::string&,
                             ports::SignalCallback cb) {
                cmd_response_cb_ = cb;
            });
        client_.init();
    }

    MockCloudTransport transport_;
    MockSignalBus signal_bus_;
    CloudGatewayConfig config_{};
    CloudGatewayClient client_{transport_, signal_bus_, config_};

    ports::CloudMessageCallback cloud_cmd_cb_;
    ports::SignalCallback lock_state_cb_;
    ports::SignalCallback cmd_response_cb_;
};

TEST_F(CloudGatewayClientTest, InitConnectsAndSubscribes) {
    EXPECT_TRUE(client_.isConnected());
}

TEST_F(CloudGatewayClientTest, CloudLockCommandPublishesToSignalBus) {
    ASSERT_TRUE(cloud_cmd_cb_);

    EXPECT_CALL(signal_bus_,
                publish(config_.signal_command_lock,
                        ports::SignalValue{true}))
        .WillOnce(Return(true));

    std::vector<uint8_t> payload = {0x01};
    cloud_cmd_cb_("vehicles.WVWZZZ3CZWE000001.command.door.lock", payload);
}

TEST_F(CloudGatewayClientTest, CloudUnlockCommandPublishesToSignalBus) {
    ASSERT_TRUE(cloud_cmd_cb_);

    EXPECT_CALL(signal_bus_,
                publish(config_.signal_command_lock,
                        ports::SignalValue{false}))
        .WillOnce(Return(true));

    std::vector<uint8_t> payload = {0x00};
    cloud_cmd_cb_("vehicles.WVWZZZ3CZWE000001.command.door.lock", payload);
}

TEST_F(CloudGatewayClientTest, LockStateForwardedToCloud) {
    ASSERT_TRUE(lock_state_cb_);

    std::vector<uint8_t> expected_payload = {0x01};
    EXPECT_CALL(transport_,
                publish("vehicles.WVWZZZ3CZWE000001.state.door.locked",
                        expected_payload))
        .WillOnce(Return(true));

    lock_state_cb_(config_.signal_is_locked, ports::SignalValue{true});
}

TEST_F(CloudGatewayClientTest, UnlockStateForwardedToCloud) {
    ASSERT_TRUE(lock_state_cb_);

    std::vector<uint8_t> expected_payload = {0x00};
    EXPECT_CALL(transport_,
                publish("vehicles.WVWZZZ3CZWE000001.state.door.locked",
                        expected_payload))
        .WillOnce(Return(true));

    lock_state_cb_(config_.signal_is_locked, ports::SignalValue{false});
}

TEST_F(CloudGatewayClientTest, CommandResponseForwardedToCloud) {
    ASSERT_TRUE(cmd_response_cb_);

    std::vector<uint8_t> expected_payload = {0x00};
    EXPECT_CALL(transport_,
                publish("vehicles.WVWZZZ3CZWE000001.command.door.response",
                        expected_payload))
        .WillOnce(Return(true));

    cmd_response_cb_(config_.signal_command_response,
                     ports::SignalValue{static_cast<int32_t>(0)});
}

TEST_F(CloudGatewayClientTest, EmptyCloudCommandIgnored) {
    ASSERT_TRUE(cloud_cmd_cb_);

    EXPECT_CALL(signal_bus_, publish(_, _)).Times(0);

    std::vector<uint8_t> empty;
    cloud_cmd_cb_("vehicles.WVWZZZ3CZWE000001.command.door.lock", empty);
}

TEST_F(CloudGatewayClientTest, ShutdownDisconnects) {
    EXPECT_CALL(transport_, disconnect()).Times(1);
    client_.shutdown();
    EXPECT_FALSE(client_.isConnected());
}

TEST(CloudGatewayClientInitTest, FailedConnectionSetsDisconnected) {
    MockCloudTransport transport;
    MockSignalBus signal_bus;
    CloudGatewayConfig config;
    CloudGatewayClient client(transport, signal_bus, config);

    EXPECT_CALL(transport, connect()).WillOnce(Return(false));
    client.init();

    EXPECT_FALSE(client.isConnected());
}
