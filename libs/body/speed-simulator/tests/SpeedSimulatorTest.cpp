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

TEST_F(SpeedSimulatorTest, ZeroThrottleNoSpeed) {
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

TEST_F(SpeedSimulatorTest, FullThrottleAccelerates) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(5);

    for (int i = 0; i < 5; ++i) {
        timer_cb_();
    }

    EXPECT_GT(sim_.getSpeedKmh(), 0.0f);
    float expected_after_5_ticks =
        config_.max_acceleration * 0.1f * 3.6f * 5.0f;
    EXPECT_NEAR(sim_.getSpeedKmh(), expected_after_5_ticks, 0.1f);
}

TEST_F(SpeedSimulatorTest, CoastDecelerates) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(10);

    for (int i = 0; i < 10; ++i) {
        timer_cb_();
    }
    float speed_at_throttle = sim_.getSpeedKmh();
    EXPECT_GT(speed_at_throttle, 0.0f);

    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(0));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(5);

    for (int i = 0; i < 5; ++i) {
        timer_cb_();
    }

    EXPECT_LT(sim_.getSpeedKmh(), speed_at_throttle);
}

TEST_F(SpeedSimulatorTest, SpeedClampsAtMax) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1000);

    for (int i = 0; i < 1000; ++i) {
        timer_cb_();
    }

    EXPECT_LE(sim_.getSpeedKmh(), config_.max_speed_kmh);
    EXPECT_FLOAT_EQ(sim_.getSpeedKmh(), config_.max_speed_kmh);
}

TEST_F(SpeedSimulatorTest, GetSpeedMethod) {
    EXPECT_CALL(adc_, read(_)).WillRepeatedly(Return(4095));
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(3);

    for (int i = 0; i < 3; ++i) {
        timer_cb_();
    }

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
        .WillOnce([](const std::string&, const ports::SignalValue& v) {
            auto* f = std::get_if<float>(&v);
            ASSERT_NE(f, nullptr);
            EXPECT_GT(*f, 0.0f);
            return true;
        });

    timer_cb_();
}
