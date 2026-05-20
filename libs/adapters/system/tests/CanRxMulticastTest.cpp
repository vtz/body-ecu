#include <gtest/gtest.h>

#include "MockCanBus.h"
#include "MockSomeIpService.h"
#include "DoCanTransport.h"
#include "can_gateway/CanGateway.h"

using namespace body_ecu;
using namespace body_ecu::adapters;
using namespace body_ecu::platform;
using namespace body_ecu::mocks;
namespace ports = body_ecu::ports;
using ::testing::_;

/// Regression test for P0 CAN RX ownership bug:
/// Both DoCanTransport and CanGateway must receive frames from the same
/// ICanBus after full lifecycle initialization (init + run).
class CanRxMulticastTest : public ::testing::Test {
protected:
    void SetUp() override {
        ServiceMapping door_status;
        door_status.name = "door_status";
        door_status.direction = GatewayDirection::CanToSomeIp;
        door_status.someip_service_id = 0x1001;
        door_status.someip_event_id = 0x8001;
        door_status.someip_eventgroup_id = 0x0001;
        door_status.can_id = 0x300;
        door_status.can_dlc = 2;
        gateway_.addMapping(door_status);
    }

    MockCanBus can_;
    MockSomeIpService someip_;
    DoCanTransport docan_{can_};
    CanGateway gateway_{can_, someip_};
};

TEST_F(CanRxMulticastTest, BothReceiveFramesAfterLifecycleInit) {
    std::vector<ports::CanRxCallback> callbacks;

    EXPECT_CALL(can_, addRxCallback(_))
        .Times(testing::AtLeast(2))
        .WillRepeatedly([&](ports::CanRxCallback cb) {
            callbacks.push_back(std::move(cb));
        });
    EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(testing::AnyNumber());

    bool diag_handler_called = false;
    docan_.setRequestHandler([&](const DiagRequest& req) -> DiagResponse {
        diag_handler_called = true;
        return {0x62, 0xF1, 0x00, 0x42};
    });

    // Simulate lifecycle: init phase
    docan_.init();

    // Simulate lifecycle: run phase
    EXPECT_CALL(can_, send(_)).WillRepeatedly(testing::Return(true));
    gateway_.start();

    ASSERT_GE(callbacks.size(), 2u)
        << "Both DoCAN and CAN gateway must register RX callbacks";

    // Send a diagnostic frame (0x600) — should reach DoCAN
    ports::CanFrame diag_frame;
    diag_frame.id = 0x600;
    diag_frame.dlc = 4;
    diag_frame.data[0] = 0x03;  // SF PCI: length=3
    diag_frame.data[1] = 0x22;  // SID: ReadDataByIdentifier
    diag_frame.data[2] = 0xF1;
    diag_frame.data[3] = 0x00;

    // Send a gateway frame (0x300) — should reach CAN gateway
    ports::CanFrame gw_frame;
    gw_frame.id = 0x300;
    gw_frame.dlc = 2;
    gw_frame.data[0] = 0xAA;
    gw_frame.data[1] = 0xBB;

    std::vector<uint8_t> captured_event_payload;
    EXPECT_CALL(someip_, sendEvent(0x1001, 0x8001, _))
        .WillOnce([&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
            captured_event_payload = p;
        });

    // Deliver both frames to ALL registered callbacks (multicast behavior)
    for (auto& cb : callbacks) {
        cb(diag_frame);
        cb(gw_frame);
    }

    EXPECT_TRUE(diag_handler_called)
        << "DoCAN diagnostic handler must receive frames after gateway start";
    ASSERT_EQ(captured_event_payload.size(), 2u);
    EXPECT_EQ(captured_event_payload[0], 0xAA)
        << "CAN gateway must receive frames after DoCAN init";
}
