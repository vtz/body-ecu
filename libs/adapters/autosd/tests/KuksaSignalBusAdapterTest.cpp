#include <gtest/gtest.h>

#include "autosd_adapters/KuksaSignalBusAdapter.h"

using namespace body_ecu;
using namespace body_ecu::adapters;

class KuksaSignalBusAdapterStubTest : public ::testing::Test {
protected:
    KuksaConfig config_{"test-host", 55555};
    KuksaSignalBusAdapter adapter_{config_};
};

TEST_F(KuksaSignalBusAdapterStubTest, PublishBeforeConnectFails) {
    EXPECT_FALSE(adapter_.publish("Vehicle.Speed",
                                  ports::SignalValue{42.0f}));
}

TEST_F(KuksaSignalBusAdapterStubTest, ConnectAndPublish) {
    adapter_.connect();
    EXPECT_TRUE(adapter_.publish("Vehicle.Speed",
                                 ports::SignalValue{42.0f}));
}

TEST_F(KuksaSignalBusAdapterStubTest, GetBeforeConnectReturnsNullopt) {
    EXPECT_FALSE(adapter_.get("Vehicle.Speed").has_value());
}

TEST_F(KuksaSignalBusAdapterStubTest, GetAfterConnectReturnsNulloptInStub) {
    adapter_.connect();
    auto val = adapter_.get("Vehicle.Speed");
    // Stub mode returns nullopt (no real databroker)
    EXPECT_FALSE(val.has_value());
}

TEST_F(KuksaSignalBusAdapterStubTest, SubscribeAfterConnect) {
    adapter_.connect();
    bool called = false;
    adapter_.subscribe("Vehicle.Speed",
                       [&](const std::string&,
                           const ports::SignalValue&) { called = true; });
    // Stub subscribe doesn't fire callbacks, just registers
    EXPECT_FALSE(called);
}

TEST_F(KuksaSignalBusAdapterStubTest, SubscribeBeforeConnectNoOp) {
    bool called = false;
    adapter_.subscribe("Vehicle.Speed",
                       [&](const std::string&,
                           const ports::SignalValue&) { called = true; });
    EXPECT_FALSE(called);
}

TEST_F(KuksaSignalBusAdapterStubTest, DisconnectAfterConnect) {
    adapter_.connect();
    adapter_.disconnect();
    EXPECT_FALSE(adapter_.publish("Vehicle.Speed",
                                  ports::SignalValue{42.0f}));
}

TEST_F(KuksaSignalBusAdapterStubTest, PublishDifferentTypes) {
    adapter_.connect();
    EXPECT_TRUE(adapter_.publish("Vehicle.Speed",
                                 ports::SignalValue{42.0f}));
    EXPECT_TRUE(adapter_.publish("Vehicle.Door.Locked",
                                 ports::SignalValue{true}));
    EXPECT_TRUE(adapter_.publish("Vehicle.Command.Response",
                                 ports::SignalValue{int32_t{0}}));
    EXPECT_TRUE(adapter_.publish("Vehicle.VIN",
                                 ports::SignalValue{std::string("VIN123")}));
}

TEST_F(KuksaSignalBusAdapterStubTest, DoubleDisconnectIsSafe) {
    adapter_.connect();
    adapter_.disconnect();
    adapter_.disconnect();
}

TEST_F(KuksaSignalBusAdapterStubTest, ConnectDisconnectReconnect) {
    adapter_.connect();
    EXPECT_TRUE(adapter_.publish("Vehicle.Speed",
                                 ports::SignalValue{1.0f}));
    adapter_.disconnect();
    EXPECT_FALSE(adapter_.publish("Vehicle.Speed",
                                  ports::SignalValue{1.0f}));
    adapter_.connect();
    EXPECT_TRUE(adapter_.publish("Vehicle.Speed",
                                 ports::SignalValue{1.0f}));
}
