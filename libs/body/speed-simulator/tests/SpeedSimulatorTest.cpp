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
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(1);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(timer_, startPeriodic(config_.update_interval_ms, _))
            .WillOnce(Invoke(
                [this](uint32_t, ports::TimerCallback cb) -> ports::TimerId {
                    timer_cb_ = std::move(cb);
                    return 42;
                }));
        sim_.init();
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

TEST_F(SpeedSimulatorTest, ZeroAdcMapsToZeroSpeed) {
    EXPECT_CALL(adc_, read(config_.adc_channel)).WillOnce(Return(0));
    std::vector<uint8_t> captured;
    EXPECT_CALL(someip_, sendEvent(config_.service_id,
                                   config_.speed_changed_event, _))
        .WillOnce(
            [&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
                captured = p;
            });

    timer_cb_();

    ASSERT_EQ(captured.size(), 4u);
    EXPECT_FLOAT_EQ(deserializeFloat(captured), 0.0f);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, FullAdcMapsToMaxSpeed) {
    EXPECT_CALL(adc_, read(_)).WillOnce(Return(4095));
    std::vector<uint8_t> captured;
    EXPECT_CALL(someip_, sendEvent(_, _, _))
        .WillOnce(
            [&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
                captured = p;
            });

    timer_cb_();

    ASSERT_EQ(captured.size(), 4u);
    EXPECT_FLOAT_EQ(deserializeFloat(captured), config_.max_speed_kmh);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);
}

TEST_F(SpeedSimulatorTest, MidAdcMapsToHalfSpeed) {
    EXPECT_CALL(adc_, read(_)).WillOnce(Return(2048));
    std::vector<uint8_t> captured;
    EXPECT_CALL(someip_, sendEvent(_, _, _))
        .WillOnce(
            [&](uint16_t, uint16_t, const std::vector<uint8_t>& p) {
                captured = p;
            });

    timer_cb_();

    float expected = (2048.0f / 4095.0f) * config_.max_speed_kmh;
    EXPECT_NEAR(sim_.getSpeedKmh(), expected, 0.1f);
    EXPECT_NEAR(deserializeFloat(captured), expected, 0.1f);
}

TEST_F(SpeedSimulatorTest, SpeedTracksAdcImmediately) {
    EXPECT_CALL(adc_, read(_))
        .WillOnce(Return(4095))
        .WillOnce(Return(0));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(2);

    timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);

    timer_cb_();
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, AdcClampedToValidRange) {
    EXPECT_CALL(adc_, read(_)).WillOnce(Return(5000));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);
}

TEST_F(SpeedSimulatorTest, NegativeAdcClampedToZero) {
    EXPECT_CALL(adc_, read(_)).WillOnce(Return(-100));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    timer_cb_();

    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), 0.0f);
}

TEST_F(SpeedSimulatorTest, NoEventWhenSpeedUnchanged) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    timer_cb_();
    timer_cb_();
    timer_cb_();
}

TEST_F(SpeedSimulatorTest, GetSpeedMethod) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    timer_cb_();

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
    EXPECT_FLOAT_EQ(deserializeFloat(resp.payload), 0.0f);
}

class SpeedSimulatorWithSignalBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(1);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(timer_, startPeriodic(config_.update_interval_ms, _))
            .WillOnce(Invoke(
                [this](uint32_t, ports::TimerCallback cb) -> ports::TimerId {
                    timer_cb_ = std::move(cb);
                    return 42;
                }));
        sim_.init();
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
    EXPECT_CALL(adc_, read(_)).WillOnce(Return(2048));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_CALL(signal_bus_,
                publish(config_.signal_speed, _))
        .WillOnce([](const std::string&, const ports::SignalValue& v) -> bool {
            auto* f = std::get_if<float>(&v);
            EXPECT_NE(f, nullptr);
            if (f) EXPECT_GT(*f, 0.0f);
            return true;
        });

    timer_cb_();
}
