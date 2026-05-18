#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ignition/IgnitionController.h"
#include "MockButtonInput.h"
#include "MockSomeIpService.h"
#include "MockTimerService.h"
#include "MockSignalBus.h"

using namespace body_ecu;
using namespace body_ecu::body;
using namespace body_ecu::mocks;
using namespace testing;

class IgnitionControllerTest : public Test {
protected:
    void SetUp() override {
        ON_CALL(button_, onPress(_))
            .WillByDefault(SaveArg<0>(&button_cb_));

        ON_CALL(timer_, startOneShot(_, _))
            .WillByDefault(DoAll(SaveArg<1>(&timer_cb_), Return(42)));

        ON_CALL(signal_bus_, get("Vehicle.Speed"))
            .WillByDefault(Return(std::optional<ports::SignalValue>{0.0f}));
    }

    void createController() {
        controller_ = std::make_unique<IgnitionController>(
            button_, mode_manager_, timer_, config_, &signal_bus_);
    }

    void initController() {
        createController();
        controller_->init();
    }

    void pressButton() {
        ASSERT_TRUE(button_cb_);
        button_cb_();
    }

    void setSpeed(float speed) {
        ON_CALL(signal_bus_, get("Vehicle.Speed"))
            .WillByDefault(Return(std::optional<ports::SignalValue>{speed}));
    }

    void fireCrankTimer() {
        ASSERT_TRUE(timer_cb_);
        timer_cb_();
    }

    NiceMock<MockButtonInput> button_;
    NiceMock<MockSomeIpService> someip_;
    NiceMock<MockTimerService> timer_;
    NiceMock<MockSignalBus> signal_bus_;

    VehicleModeManager mode_manager_{someip_};
    IgnitionConfig config_;

    std::unique_ptr<IgnitionController> controller_;
    ports::ButtonCallback button_cb_;
    ports::TimerCallback timer_cb_;
};

TEST_F(IgnitionControllerTest, InitTransitionsToAccessory) {
    mode_manager_.init();
    initController();

    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);
}

TEST_F(IgnitionControllerTest, ButtonInAccessoryStartsCranking) {
    mode_manager_.init();
    initController();

    EXPECT_CALL(timer_, startOneShot(config_.crank_duration_ms, _));
    pressButton();

    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Crank);
}

TEST_F(IgnitionControllerTest, CrankTimerTransitionsToRun) {
    mode_manager_.init();
    initController();

    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Crank);

    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);
}

TEST_F(IgnitionControllerTest, ButtonInRunGoesToAccessory) {
    mode_manager_.init();
    initController();

    pressButton();
    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);

    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);
}

TEST_F(IgnitionControllerTest, ButtonInRunBlockedBySpeed) {
    mode_manager_.init();
    initController();

    pressButton();
    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);

    setSpeed(50.0f);
    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);
}

TEST_F(IgnitionControllerTest, ButtonIgnoredDuringCrank) {
    mode_manager_.init();
    initController();

    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Crank);

    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Crank);
}

TEST_F(IgnitionControllerTest, FullCycleAccessoryRunAccessory) {
    mode_manager_.init();
    initController();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);

    pressButton();
    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);

    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);

    pressButton();
    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);
}

TEST_F(IgnitionControllerTest, SpeedZeroAllowsTurnOff) {
    mode_manager_.init();
    initController();

    pressButton();
    fireCrankTimer();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);

    setSpeed(50.0f);
    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Run);

    setSpeed(0.0f);
    pressButton();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);
}

TEST_F(IgnitionControllerTest, TimerExhaustionRevertsToAccessory) {
    ON_CALL(timer_, startOneShot(_, _))
        .WillByDefault(Return(ports::kInvalidTimerId));

    mode_manager_.init();
    initController();
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);

    pressButton();
    // Timer allocation failed — controller must revert to Accessory,
    // not stay stuck in Crank indefinitely.
    EXPECT_EQ(mode_manager_.getMode(), ports::VehicleMode::Accessory);
}
