#include <gtest/gtest.h>

#include "MockGpioPort.h"
#include "MockSomeIpService.h"
#include "lighting/LightingController.h"

using namespace body_ecu;
using namespace body_ecu::body;
using namespace body_ecu::mocks;
using ::testing::_;

class LightingControllerTest : public ::testing::Test {
protected:
    void SetUp() override { ctrl_.init(); }

    MockGpioPort gpio_;
    MockSomeIpService someip_;
    LightingConfig config_{};
    LightingController ctrl_{gpio_, someip_, config_};
};

TEST_F(LightingControllerTest, SetLightState) {
    EXPECT_CALL(gpio_, write(config_.gpio_pins[0], true)).Times(1);
    EXPECT_TRUE(ctrl_.setLightState(LightId::Headlight, true));
}

TEST_F(LightingControllerTest, GetLightStatus) {
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    ctrl_.setLightState(LightId::Turn, true);

    auto status = ctrl_.getLightStatus();
    EXPECT_FALSE(status[0]);
    EXPECT_TRUE(status[1]);
    EXPECT_FALSE(status[2]);
}

TEST_F(LightingControllerTest, LightStatusChangedEvent) {
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_,
                sendEvent(config_.service_id,
                          config_.light_status_changed_event, _))
        .Times(1);

    ctrl_.setLightState(LightId::Headlight, true);
}

TEST_F(LightingControllerTest, NoEventOnSameState) {
    EXPECT_CALL(gpio_, write(_, _)).Times(2);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    ctrl_.setLightState(LightId::Headlight, true);
    ctrl_.setLightState(LightId::Headlight, true);
}

TEST_F(LightingControllerTest, InvalidLightId) {
    EXPECT_CALL(gpio_, write(_, _)).Times(0);
    EXPECT_FALSE(ctrl_.setLightState(static_cast<LightId>(99), true));
}

TEST_F(LightingControllerTest, AllLightsOffOnModeChange) {
    EXPECT_CALL(gpio_, write(_, _)).Times(testing::AtLeast(3));

    ctrl_.setLightState(LightId::Headlight, true);
    ctrl_.setLightState(LightId::Turn, true);
    ctrl_.setLightState(LightId::Brake, true);

    ctrl_.onModeChanged(ports::VehicleMode::Run, ports::VehicleMode::Off);

    auto status = ctrl_.getLightStatus();
    EXPECT_FALSE(status[0]);
    EXPECT_FALSE(status[1]);
    EXPECT_FALSE(status[2]);
}

TEST_F(LightingControllerTest, HandleSetLightStateMethod) {
    ports::SomeIpMessage request;
    request.service_id = config_.service_id;
    request.method_id = config_.set_light_state_method;
    request.payload = {0x00, 0x01};

    EXPECT_CALL(gpio_, write(config_.gpio_pins[0], true)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(config_.service_id,
                                        config_.set_light_state_method, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = std::move(h);
        });
    EXPECT_CALL(someip_, registerMethod(config_.service_id,
                                        config_.get_light_status_method, _));
    EXPECT_CALL(someip_, registerEvent(_, _, _));

    ctrl_.init();

    auto response = handler(request);
    EXPECT_EQ(response.return_code, 0x00);
    EXPECT_EQ(response.message_type, 0x80);
}

TEST_F(LightingControllerTest, DiagReadData) {
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    ctrl_.setLightState(LightId::Headlight, true);

    auto data = ctrl_.readData(config_.diag_did);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(data->data.size(), 3u);
    EXPECT_EQ(data->data[0], 1);
    EXPECT_EQ(data->data[1], 0);
    EXPECT_EQ(data->data[2], 0);
}

TEST_F(LightingControllerTest, DiagReadUnknownDid) {
    auto data = ctrl_.readData(0xFFFF);
    EXPECT_FALSE(data.has_value());
}
