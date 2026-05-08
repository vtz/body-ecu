#include <gtest/gtest.h>

#include "MockSignalBus.h"
#include "MockSomeIpService.h"
#include "autosd_adapters/SomeIpKuksaBridge.h"

using namespace body_ecu;
using namespace body_ecu::adapters;
using namespace body_ecu::mocks;
using ::testing::_;
using ::testing::Return;

class SomeIpKuksaBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        event_mapping_.signal_path =
            "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked";
        event_mapping_.direction = BridgeDirection::EventToSignal;
        event_mapping_.someip_service_id = 0x1001;
        event_mapping_.someip_method_or_event_id = 0x8001;
        event_mapping_.someip_eventgroup_id = 0x0001;

        cmd_mapping_.signal_path = "Vehicle.Command.Door.Lock";
        cmd_mapping_.direction = BridgeDirection::SignalToMethod;
        cmd_mapping_.someip_service_id = 0x1001;
        cmd_mapping_.someip_method_or_event_id = 0x0001;
    }

    MockSomeIpService someip_;
    MockSignalBus signal_bus_;
    BridgeMapping event_mapping_;
    BridgeMapping cmd_mapping_;
};

TEST_F(SomeIpKuksaBridgeTest, EventToSignalPublishesIntForSingleByte) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(event_mapping_);

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(0x1001, 0x8001, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = h;
        });

    bridge.init();
    ASSERT_TRUE(handler);

    EXPECT_CALL(signal_bus_,
                publish("Vehicle.Cabin.Door.Row1.DriverSide.IsLocked",
                        ports::SignalValue{int32_t{1}}))
        .WillOnce(Return(true));

    ports::SomeIpMessage msg;
    msg.service_id = 0x1001;
    msg.method_id = 0x8001;
    msg.payload = {0x01};
    handler(msg);
}

TEST_F(SomeIpKuksaBridgeTest, EventToSignalPublishesZeroForSingleByte) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(event_mapping_);

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(0x1001, 0x8001, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = h;
        });

    bridge.init();

    EXPECT_CALL(signal_bus_,
                publish("Vehicle.Cabin.Door.Row1.DriverSide.IsLocked",
                        ports::SignalValue{int32_t{0}}))
        .WillOnce(Return(true));

    ports::SomeIpMessage msg;
    msg.service_id = 0x1001;
    msg.method_id = 0x8001;
    msg.payload = {0x00};
    handler(msg);
}

TEST_F(SomeIpKuksaBridgeTest, EventToSignalEmptyPayloadIgnored) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(event_mapping_);

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(_, _, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = h;
        });

    bridge.init();

    EXPECT_CALL(signal_bus_, publish(_, _)).Times(0);

    ports::SomeIpMessage msg;
    msg.service_id = 0x1001;
    msg.method_id = 0x8001;
    handler(msg);
}

TEST_F(SomeIpKuksaBridgeTest, SignalToMethodSendsSomeIpRequest) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(cmd_mapping_);

    ports::SignalCallback signal_cb;
    EXPECT_CALL(signal_bus_, subscribe("Vehicle.Command.Door.Lock", _))
        .WillOnce([&](const std::string&, ports::SignalCallback cb) {
            signal_cb = cb;
        });

    bridge.init();
    ASSERT_TRUE(signal_cb);

    ports::SomeIpMessage captured;
    EXPECT_CALL(someip_, sendResponse(_))
        .WillOnce([&](const ports::SomeIpMessage& msg) { captured = msg; });

    signal_cb("Vehicle.Command.Door.Lock", ports::SignalValue{true});

    EXPECT_EQ(captured.service_id, 0x1001);
    EXPECT_EQ(captured.method_id, 0x0001);
    EXPECT_EQ(captured.message_type, 0x00);
    ASSERT_EQ(captured.payload.size(), 1u);
    EXPECT_EQ(captured.payload[0], 1);
}

TEST_F(SomeIpKuksaBridgeTest, SignalToMethodBoolFalseSendsZero) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(cmd_mapping_);

    ports::SignalCallback signal_cb;
    EXPECT_CALL(signal_bus_, subscribe(_, _))
        .WillOnce([&](const std::string&, ports::SignalCallback cb) {
            signal_cb = cb;
        });

    bridge.init();

    ports::SomeIpMessage captured;
    EXPECT_CALL(someip_, sendResponse(_))
        .WillOnce([&](const ports::SomeIpMessage& msg) { captured = msg; });

    signal_cb("Vehicle.Command.Door.Lock", ports::SignalValue{false});

    ASSERT_EQ(captured.payload.size(), 1u);
    EXPECT_EQ(captured.payload[0], 0);
}

TEST_F(SomeIpKuksaBridgeTest, MultipleMappings) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(event_mapping_);
    bridge.addMapping(cmd_mapping_);

    EXPECT_CALL(someip_, registerMethod(0x1001, 0x8001, _)).Times(1);
    EXPECT_CALL(someip_, registerMethod(0x1001, 0x0001, _)).Times(1);
    EXPECT_CALL(signal_bus_, subscribe("Vehicle.Command.Door.Lock", _))
        .Times(1);

    bridge.init();

    EXPECT_EQ(bridge.mappings().size(), 2u);
}

TEST_F(SomeIpKuksaBridgeTest, EventHandlerReturnsAck) {
    SomeIpKuksaBridge bridge(someip_, signal_bus_);
    bridge.addMapping(event_mapping_);

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(_, _, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = h;
        });

    bridge.init();

    EXPECT_CALL(signal_bus_, publish(_, _)).WillOnce(Return(true));

    ports::SomeIpMessage msg;
    msg.service_id = 0x1001;
    msg.method_id = 0x8001;
    msg.payload = {0x01};
    auto ack = handler(msg);

    EXPECT_EQ(ack.message_type, 0x80);
    EXPECT_EQ(ack.return_code, 0x00);
}
