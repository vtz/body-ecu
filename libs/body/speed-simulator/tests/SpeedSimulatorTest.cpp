#include <gtest/gtest.h>

#include <cstring>

#include "MockAdcInput.h"
#include "MockSignalBus.h"
#include "MockSomeIpService.h"
#include "MockTimerService.h"
#include "speed_simulator/SpeedSimulator.h"

using namespace body_ecu;
using namespace body_ecu::body;
using namespace body_ecu::mocks;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

static float deserializeFloat(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4) return 0.0f;
    uint32_t bits = (static_cast<uint32_t>(payload[0]) << 24) |
                    (static_cast<uint32_t>(payload[1]) << 16) |
                    (static_cast<uint32_t>(payload[2]) << 8) |
                     static_cast<uint32_t>(payload[3]);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

class SpeedSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(2);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(timer_, startPeriodic(config_.update_interval_ms, _))
            .WillOnce(Invoke(
                [this](uint32_t, ports::TimerCallback cb) -> ports::TimerId {
                    timer_cb_ = std::move(cb);
                    return 42;
                }));
        sim_.init();
        sim_.onModeChanged(ports::VehicleMode::Off, ports::VehicleMode::Run);
    }

    MockAdcInput adc_;
    MockSomeIpService someip_;
    MockTimerService timer_;
    SpeedSimulatorConfig config_{};
    SpeedSimulator sim_{adc_, someip_, timer_, config_};
    ports::TimerCallback timer_cb_;
};

TEST_F(SpeedSimulatorTest, InitRegistersTimerAndSomeIp) {
    EXPECT_TRUE(timer_cb_);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, ZeroAdcStaysAtZero) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(0));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);

    for (int i = 0; i < 50; ++i) timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, FullAdcConvergesToMaxSpeed) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);
}

TEST_F(SpeedSimulatorTest, SpeedIsQuantizedTo5KmhSteps) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    float speed = sim_.getSpeedKmh();
    float remainder = std::fmod(speed, 5.0f);
    EXPECT_FLOAT_EQ(remainder, 0.0f);
}

TEST_F(SpeedSimulatorTest, DeadZoneSnapsToZero) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(150));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);

    for (int i = 0; i < 50; ++i) timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, DeadZoneSnapsToMax) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(3920));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);
}

TEST_F(SpeedSimulatorTest, NoEventWhenSpeedStable) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    testing::Mock::VerifyAndClearExpectations(&someip_);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);

    for (int i = 0; i < 100; ++i) timer_cb_();
}

TEST_F(SpeedSimulatorTest, NoisyAdcDoesNotTriggerEvents) {
    int tick = 0;
    EXPECT_CALL(adc_, read(_)).WillRepeatedly([&tick](uint8_t) -> int32_t {
        return 900 + ((tick++ % 2 == 0) ? -190 : 190);
    });
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    float settled = sim_.getSpeedKmh();

    testing::Mock::VerifyAndClearExpectations(&someip_);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);

    for (int i = 0; i < 200; ++i) timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), settled);
}

TEST_F(SpeedSimulatorTest, GetSpeedMethod) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    for (int i = 0; i < 300; ++i) timer_cb_();

    ports::SomeIpMessage req;
    req.service_id = config_.service_id;
    req.method_id = config_.get_speed_method;
    req.message_type = 0x00;

    ports::MethodHandler handler;
    EXPECT_CALL(someip_, registerMethod(config_.service_id,
                                        config_.get_speed_method, _))
        .WillOnce([&](uint16_t, uint16_t, ports::MethodHandler h) {
            handler = std::move(h);
        });
    EXPECT_CALL(someip_, registerMethod(config_.service_id,
                                        config_.set_speed_method, _))
        .Times(1);
    EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
    EXPECT_CALL(timer_, startPeriodic(_, _))
        .WillOnce(Return(99));

    SpeedSimulator sim2(adc_, someip_, timer_, config_);
    sim2.init();

    ASSERT_TRUE(handler);
    auto resp = handler(req);
    EXPECT_EQ(resp.message_type, 0x80);
    EXPECT_EQ(resp.return_code, 0x00);
    ASSERT_EQ(resp.payload.size(), 4u);
}

class SpeedSimulatorWithSignalBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(2);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(timer_, startPeriodic(config_.update_interval_ms, _))
            .WillOnce(Invoke(
                [this](uint32_t, ports::TimerCallback cb) -> ports::TimerId {
                    timer_cb_ = std::move(cb);
                    return 42;
                }));
        sim_.init();
        sim_.onModeChanged(ports::VehicleMode::Off, ports::VehicleMode::Run);
    }

    MockAdcInput adc_;
    MockSomeIpService someip_;
    MockTimerService timer_;
    MockSignalBus signal_bus_;
    SpeedSimulatorConfig config_{};
    SpeedSimulator sim_{adc_, someip_, timer_, config_, &signal_bus_};
    ports::TimerCallback timer_cb_;
};

TEST_F(SpeedSimulatorWithSignalBusTest, PublishesToSignalBus) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(signal_bus_, publish(config_.signal_speed, _))
        .Times(testing::AtLeast(1));

    for (int i = 0; i < 300; ++i) timer_cb_();

    EXPECT_GT(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, SpeedZeroWhenNotInRunMode) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    for (int i = 0; i < 300; ++i) timer_cb_();
    EXPECT_GT(sim_.getSpeedKmh(), 0.0f);

    testing::Mock::VerifyAndClearExpectations(&someip_);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AtLeast(1));

    sim_.onModeChanged(ports::VehicleMode::Run, ports::VehicleMode::Accessory);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);

    testing::Mock::VerifyAndClearExpectations(&someip_);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);
    for (int i = 0; i < 50; ++i) timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, SpeedResumesWhenBackInRun) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    for (int i = 0; i < 300; ++i) timer_cb_();
    float before = sim_.getSpeedKmh();
    EXPECT_GT(before, 0.0f);

    sim_.onModeChanged(ports::VehicleMode::Run, ports::VehicleMode::Accessory);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);

    sim_.onModeChanged(ports::VehicleMode::Accessory, ports::VehicleMode::Run);
    for (int i = 0; i < 300; ++i) timer_cb_();
    EXPECT_GT(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, OffModeSpeedIsZero) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    sim_.onModeChanged(ports::VehicleMode::Run, ports::VehicleMode::Off);
    for (int i = 0; i < 50; ++i) timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, CrankModeSpeedIsZero) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    sim_.onModeChanged(ports::VehicleMode::Run, ports::VehicleMode::Crank);
    for (int i = 0; i < 50; ++i) timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, AdcFailureHoldsLastSpeed) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(testing::AnyNumber());

    for (int i = 0; i < 300; ++i) timer_cb_();
    float speed_before = sim_.getSpeedKmh();
    EXPECT_GT(speed_before, 0.0f);

    // ADC starts returning error
    testing::Mock::VerifyAndClearExpectations(&adc_);
    testing::Mock::VerifyAndClearExpectations(&someip_);
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(-1));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);

    for (int i = 0; i < 50; ++i) timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), speed_before)
        << "ADC failure must hold last known speed, not drive to zero";
}
