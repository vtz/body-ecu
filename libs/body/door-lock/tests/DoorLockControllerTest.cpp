#include <gtest/gtest.h>

#include "MockButtonInput.h"
#include "MockGpioPort.h"
#include "MockSignalBus.h"
#include "MockSomeIpService.h"
#include "door_lock/DoorLockController.h"

using namespace body_ecu;
using namespace body_ecu::body;
using namespace body_ecu::mocks;
using ::testing::_;
using ::testing::Return;

// --- Tests without signal bus (backward compatibility) ---

class DoorLockControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(3);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(button_, onPress(_))
            .WillOnce(
                [this](ports::ButtonCallback cb) { button_cb_ = cb; });
        ctrl_.init();
    }

    MockGpioPort gpio_;
    MockButtonInput button_;
    MockSomeIpService someip_;
    DoorLockConfig config_{};
    DoorLockController ctrl_{gpio_, button_, someip_, config_};
    ports::ButtonCallback button_cb_;
};

TEST_F(DoorLockControllerTest, LockUnlockTransition) {
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);

    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, true)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_TRUE(ctrl_.lock());
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);

    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, false)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_TRUE(ctrl_.unlock());
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);
}

TEST_F(DoorLockControllerTest, DoubleLockNoOp) {
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    ctrl_.lock();

    EXPECT_CALL(gpio_, write(_, _)).Times(0);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);
    EXPECT_TRUE(ctrl_.lock());
}

TEST_F(DoorLockControllerTest, GetStatus) {
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);

    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    ctrl_.lock();
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);
}

TEST_F(DoorLockControllerTest, LockStateChangedEvent) {
    std::vector<uint8_t> captured_payload;
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_,
                sendEvent(config_.service_id,
                          config_.lock_state_changed_event, _))
        .WillOnce(
            [&](uint16_t, uint16_t, const std::vector<uint8_t>& payload) {
                captured_payload = payload;
            });

    ctrl_.lock();

    ASSERT_EQ(captured_payload.size(), 2u);
    EXPECT_EQ(captured_payload[0],
              static_cast<uint8_t>(LockState::Unlocked));
    EXPECT_EQ(captured_payload[1], static_cast<uint8_t>(LockState::Locked));
}

TEST_F(DoorLockControllerTest, ButtonToggle) {
    ASSERT_TRUE(button_cb_);

    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, true)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    button_cb_();
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);

    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, false)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    button_cb_();
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);
}

TEST_F(DoorLockControllerTest, ErrorState) {
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    ctrl_.setError();
    EXPECT_EQ(ctrl_.getState(), LockState::Error);

    EXPECT_FALSE(ctrl_.lock());
    EXPECT_FALSE(ctrl_.unlock());
}

TEST_F(DoorLockControllerTest, AutoLockOnRunMode) {
    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, true)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    ctrl_.onModeChanged(ports::VehicleMode::Accessory,
                        ports::VehicleMode::Run);
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);
}

// --- Tests with signal bus ---

class DoorLockWithSignalBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(3);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        EXPECT_CALL(button_, onPress(_))
            .WillOnce(
                [this](ports::ButtonCallback cb) { button_cb_ = cb; });
        EXPECT_CALL(signal_bus_, subscribe(config_.signal_command_lock, _))
            .WillOnce([this](const std::string&,
                             ports::SignalCallback cb) { cmd_cb_ = cb; });
        ctrl_.init();
    }

    MockGpioPort gpio_;
    MockButtonInput button_;
    MockSomeIpService someip_;
    MockSignalBus signal_bus_;
    DoorLockConfig config_{};
    DoorLockController ctrl_{gpio_, button_, someip_, config_, &signal_bus_};
    ports::ButtonCallback button_cb_;
    ports::SignalCallback cmd_cb_;
};

TEST_F(DoorLockWithSignalBusTest, LockPublishesToSignalBus) {
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_CALL(signal_bus_, get(_)).WillRepeatedly(Return(std::nullopt));
    EXPECT_CALL(signal_bus_,
                publish(config_.signal_is_locked,
                        ports::SignalValue{true}))
        .Times(1);

    EXPECT_TRUE(ctrl_.lock());
}

TEST_F(DoorLockWithSignalBusTest, UnlockPublishesToSignalBus) {
    EXPECT_CALL(gpio_, write(_, _)).Times(2);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(2);
    EXPECT_CALL(signal_bus_, get(_)).WillRepeatedly(Return(std::nullopt));
    EXPECT_CALL(signal_bus_, publish(config_.signal_is_locked, _)).Times(2);

    ctrl_.lock();
    ctrl_.unlock();
}

TEST_F(DoorLockWithSignalBusTest, CommandSignalLocksVehicle) {
    ASSERT_TRUE(cmd_cb_);

    EXPECT_CALL(gpio_, write(config_.lock_gpio_pin, true)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_CALL(signal_bus_, get(_)).WillRepeatedly(Return(std::nullopt));
    EXPECT_CALL(signal_bus_, publish(config_.signal_is_locked, _)).Times(1);
    EXPECT_CALL(signal_bus_, publish(config_.signal_command_response, _))
        .Times(1);

    cmd_cb_(config_.signal_command_lock, ports::SignalValue{true});
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);
}

TEST_F(DoorLockWithSignalBusTest, CommandSignalUnlocksVehicle) {
    ASSERT_TRUE(cmd_cb_);

    EXPECT_CALL(gpio_, write(_, _)).Times(2);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(2);
    EXPECT_CALL(signal_bus_, get(_)).WillRepeatedly(Return(std::nullopt));
    EXPECT_CALL(signal_bus_, publish(config_.signal_is_locked, _)).Times(2);
    EXPECT_CALL(signal_bus_, publish(config_.signal_command_response, _))
        .Times(1);

    ctrl_.lock();
    cmd_cb_(config_.signal_command_lock, ports::SignalValue{false});
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);
}

// --- Safety constraint tests ---

TEST_F(DoorLockWithSignalBusTest, LockRejectedWhenVehicleMoving) {
    EXPECT_CALL(signal_bus_, get(config_.signal_speed))
        .WillOnce(Return(ports::SignalValue{10.0f}));
    EXPECT_CALL(signal_bus_, get(config_.signal_door_open))
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_FALSE(ctrl_.lock());
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);
}

TEST_F(DoorLockWithSignalBusTest, LockRejectedWhenDoorOpen) {
    EXPECT_CALL(signal_bus_, get(config_.signal_speed))
        .WillOnce(Return(std::nullopt));
    EXPECT_CALL(signal_bus_, get(config_.signal_door_open))
        .WillOnce(Return(ports::SignalValue{true}));

    EXPECT_FALSE(ctrl_.lock());
    EXPECT_EQ(ctrl_.getState(), LockState::Unlocked);
}

TEST_F(DoorLockWithSignalBusTest, LockAllowedWhenStationaryDoorClosed) {
    EXPECT_CALL(signal_bus_, get(config_.signal_speed))
        .WillOnce(Return(ports::SignalValue{0.0f}));
    EXPECT_CALL(signal_bus_, get(config_.signal_door_open))
        .WillOnce(Return(ports::SignalValue{false}));
    EXPECT_CALL(gpio_, write(_, _)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    EXPECT_CALL(signal_bus_, publish(config_.signal_is_locked, _)).Times(1);

    EXPECT_TRUE(ctrl_.lock());
    EXPECT_EQ(ctrl_.getState(), LockState::Locked);
}
