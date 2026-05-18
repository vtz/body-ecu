#include <gtest/gtest.h>

#include "MockCanBus.h"
#include "MockSomeIpService.h"
#include "can_gateway/CanGateway.h"
#include "can_gateway/MessageTranslator.h"

using namespace body_ecu;
using namespace body_ecu::platform;
using namespace body_ecu::mocks;
using ::testing::_;

class CanGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        light_mapping_.name = "light_command";
        light_mapping_.direction = GatewayDirection::SomeIpToCan;
        light_mapping_.someip_service_id = 0x1000;
        light_mapping_.someip_method_id = 0x0001;
        light_mapping_.can_id = 0x200;
        light_mapping_.can_dlc = 4;

        door_mapping_.name = "door_status";
        door_mapping_.direction = GatewayDirection::CanToSomeIp;
        door_mapping_.someip_service_id = 0x1001;
        door_mapping_.someip_event_id = 0x8001;
        door_mapping_.someip_eventgroup_id = 0x0001;
        door_mapping_.can_id = 0x300;
        door_mapping_.can_dlc = 2;

        gw_.addMapping(light_mapping_);
        gw_.addMapping(door_mapping_);
    }

    MockCanBus can_;
    MockSomeIpService someip_;
    CanGateway gw_{can_, someip_};

    ServiceMapping light_mapping_;
    ServiceMapping door_mapping_;
};

TEST_F(CanGatewayTest, SomeIpToCanTranslation) {
    ports::CanFrame captured_frame;
    EXPECT_CALL(can_, send(_)).WillOnce([&](const ports::CanFrame& f) {
        captured_frame = f;
        return true;
    });
    EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(can_, setRxCallback(_)).Times(1);

    gw_.start();

    ports::SomeIpMessage msg;
    msg.service_id = 0x1000;
    msg.method_id = 0x0001;
    msg.payload = {0x01, 0x02, 0x03, 0x04};

    gw_.onSomeIpMessage(msg);

    EXPECT_EQ(captured_frame.id, 0x200u);
    EXPECT_EQ(captured_frame.dlc, 4);
    EXPECT_EQ(captured_frame.data[0], 0x01);
    EXPECT_EQ(captured_frame.data[1], 0x02);
    EXPECT_EQ(captured_frame.data[2], 0x03);
    EXPECT_EQ(captured_frame.data[3], 0x04);
}

TEST_F(CanGatewayTest, CanToSomeIpTranslation) {
    std::vector<uint8_t> captured_payload;
    EXPECT_CALL(someip_, sendEvent(0x1001, 0x8001, _))
        .WillOnce(
            [&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
                captured_payload = p;
            });
    EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(can_, setRxCallback(_)).Times(1);

    gw_.start();

    ports::CanFrame frame;
    frame.id = 0x300;
    frame.dlc = 2;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;

    gw_.onCanFrame(frame);

    ASSERT_EQ(captured_payload.size(), 2u);
    EXPECT_EQ(captured_payload[0], 0xAA);
    EXPECT_EQ(captured_payload[1], 0xBB);
}

TEST_F(CanGatewayTest, UnmappedMessageDrop) {
    EXPECT_CALL(can_, send(_)).Times(0);
    EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(can_, setRxCallback(_)).Times(1);

    gw_.start();

    ports::SomeIpMessage msg;
    msg.service_id = 0x9999;
    msg.method_id = 0x0001;
    msg.payload = {0xFF};

    gw_.onSomeIpMessage(msg);
}

TEST_F(CanGatewayTest, PayloadSerialization) {
    std::vector<uint8_t> payload = {0x11, 0x22, 0x33};
    auto frame = MessageTranslator::someipToCanFrame(0x200, 4, payload);
    EXPECT_EQ(frame.id, 0x200u);
    EXPECT_EQ(frame.dlc, 4);
    EXPECT_EQ(frame.data[0], 0x11);
    EXPECT_EQ(frame.data[1], 0x22);
    EXPECT_EQ(frame.data[2], 0x33);

    auto roundtrip = MessageTranslator::canFrameToSomeipPayload(frame);
    ASSERT_EQ(roundtrip.size(), 4u);
    EXPECT_EQ(roundtrip[0], 0x11);
    EXPECT_EQ(roundtrip[1], 0x22);
    EXPECT_EQ(roundtrip[2], 0x33);
}

TEST_F(CanGatewayTest, GatewayLifecycle) {
    EXPECT_FALSE(gw_.isRunning());

    EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(can_, setRxCallback(_)).Times(1);

    gw_.start();
    EXPECT_TRUE(gw_.isRunning());

    gw_.stop();
    EXPECT_FALSE(gw_.isRunning());
}

TEST_F(CanGatewayTest, BidirectionalMapping) {
    MockCanBus bidir_can;
    MockSomeIpService bidir_someip;
    CanGateway bidir_gw(bidir_can, bidir_someip);

    ServiceMapping bidir;
    bidir.name = "sensor_relay";
    bidir.direction = GatewayDirection::SomeIpToCan;
    bidir.someip_service_id = 0x2000;
    bidir.someip_method_id = 0x0010;
    bidir.can_id = 0x400;
    bidir.can_dlc = 3;

    ServiceMapping bidir_rev;
    bidir_rev.name = "sensor_relay_return";
    bidir_rev.direction = GatewayDirection::CanToSomeIp;
    bidir_rev.someip_service_id = 0x2000;
    bidir_rev.someip_event_id = 0x8010;
    bidir_rev.someip_eventgroup_id = 0x0002;
    bidir_rev.can_id = 0x401;
    bidir_rev.can_dlc = 3;

    bidir_gw.addMapping(bidir);
    bidir_gw.addMapping(bidir_rev);

    EXPECT_CALL(bidir_someip, registerMethod(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(bidir_someip, registerEvent(_, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(bidir_can, setRxCallback(_)).Times(1);

    ports::CanFrame captured_can;
    EXPECT_CALL(bidir_can, send(_)).WillOnce([&](const ports::CanFrame& f) {
        captured_can = f;
        return true;
    });

    bidir_gw.start();

    // SOME/IP -> CAN direction
    ports::SomeIpMessage msg;
    msg.service_id = 0x2000;
    msg.method_id = 0x0010;
    msg.payload = {0xAA, 0xBB, 0xCC};
    bidir_gw.onSomeIpMessage(msg);

    EXPECT_EQ(captured_can.id, 0x400u);
    EXPECT_EQ(captured_can.data[0], 0xAA);

    // CAN -> SOME/IP direction (same gateway)
    std::vector<uint8_t> captured_payload;
    EXPECT_CALL(bidir_someip, sendEvent(0x2000, 0x8010, _))
        .WillOnce([&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
            captured_payload = p;
        });

    ports::CanFrame return_frame;
    return_frame.id = 0x401;
    return_frame.dlc = 3;
    return_frame.data[0] = 0x11;
    return_frame.data[1] = 0x22;
    return_frame.data[2] = 0x33;
    bidir_gw.onCanFrame(return_frame);

    ASSERT_EQ(captured_payload.size(), 3u);
    EXPECT_EQ(captured_payload[0], 0x11);
    EXPECT_EQ(captured_payload[1], 0x22);
    EXPECT_EQ(captured_payload[2], 0x33);
}

TEST(CanFrameDlcValidation, MaxDlcClampedInTranslation) {
    // DLC > 64 must not cause overflow in MessageTranslator
    auto frame = MessageTranslator::someipToCanFrame(0x100, 255, {0x01, 0x02});
    EXPECT_LE(frame.dlc, sizeof(frame.data));

    auto payload = MessageTranslator::canFrameToSomeipPayload(frame);
    EXPECT_LE(payload.size(), sizeof(frame.data));
}

TEST(CanFrameDlcValidation, ZeroDlcIsValid) {
    auto frame = MessageTranslator::someipToCanFrame(0x100, 0, {});
    EXPECT_EQ(frame.dlc, 0);
    auto payload = MessageTranslator::canFrameToSomeipPayload(frame);
    EXPECT_TRUE(payload.empty());
}

TEST(CanFrameDlcValidation, ClassicCanMaxDlc) {
    std::vector<uint8_t> data(8, 0xAA);
    auto frame = MessageTranslator::someipToCanFrame(0x100, 8, data);
    EXPECT_EQ(frame.dlc, 8);
}

TEST_F(CanGatewayTest, MappingLoadFromConfig) {
    MockCanBus cfg_can;
    MockSomeIpService cfg_someip;
    CanGateway cfg_gw(cfg_can, cfg_someip);

    ServiceMapping m1;
    m1.name = "cfg_test_a";
    m1.direction = GatewayDirection::SomeIpToCan;
    m1.someip_service_id = 0x3000;
    m1.someip_method_id = 0x0001;
    m1.can_id = 0x500;
    m1.can_dlc = 8;

    ServiceMapping m2;
    m2.name = "cfg_test_b";
    m2.direction = GatewayDirection::CanToSomeIp;
    m2.someip_service_id = 0x3001;
    m2.someip_event_id = 0x8001;
    m2.someip_eventgroup_id = 0x0001;
    m2.can_id = 0x501;
    m2.can_dlc = 4;

    cfg_gw.addMapping(m1);
    cfg_gw.addMapping(m2);

    EXPECT_CALL(cfg_someip, registerMethod(0x3000, 0x0001, _)).Times(1);
    EXPECT_CALL(cfg_someip, registerEvent(0x3001, 0x8001, 0x0001)).Times(1);
    EXPECT_CALL(cfg_can, setRxCallback(_)).Times(1);

    cfg_gw.start();
    EXPECT_TRUE(cfg_gw.isRunning());

    // Verify SOME/IP-to-CAN mapping works for the loaded config
    ports::CanFrame captured;
    EXPECT_CALL(cfg_can, send(_)).WillOnce([&](const ports::CanFrame& f) {
        captured = f;
        return true;
    });

    ports::SomeIpMessage msg;
    msg.service_id = 0x3000;
    msg.method_id = 0x0001;
    msg.payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    cfg_gw.onSomeIpMessage(msg);

    EXPECT_EQ(captured.id, 0x500u);
    EXPECT_EQ(captured.dlc, 8);
    EXPECT_EQ(captured.data[0], 0xDE);
    EXPECT_EQ(captured.data[3], 0xEF);
}
