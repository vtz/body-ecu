#include <gtest/gtest.h>

#include "MockModeObserver.h"
#include "MockSomeIpService.h"
#include "vehicle_mode/VehicleModeManager.h"

using namespace body_ecu;
using namespace body_ecu::body;
using namespace body_ecu::mocks;
using ::testing::_;

class VehicleModeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_CALL(someip_, registerMethod(_, _, _)).Times(2);
        EXPECT_CALL(someip_, registerEvent(_, _, _)).Times(1);
        mgr_.init();
    }

    MockSomeIpService someip_;
    VehicleModeConfig config_{};
    VehicleModeManager mgr_{someip_, config_};
};

TEST_F(VehicleModeManagerTest, InitialModeIsOff) {
    EXPECT_EQ(mgr_.getMode(), ports::VehicleMode::Off);
}

TEST_F(VehicleModeManagerTest, ModeGetter) {
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);
    mgr_.setMode(ports::VehicleMode::Accessory);
    EXPECT_EQ(mgr_.getMode(), ports::VehicleMode::Accessory);
}

TEST_F(VehicleModeManagerTest, ModeSetter) {
    EXPECT_CALL(someip_, sendEvent(config_.service_id,
                                   config_.notifier_event, _))
        .Times(1);
    EXPECT_TRUE(mgr_.setMode(ports::VehicleMode::Accessory));
    EXPECT_EQ(mgr_.getMode(), ports::VehicleMode::Accessory);
}

TEST_F(VehicleModeManagerTest, InvalidTransitionRejected) {
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);
    EXPECT_FALSE(mgr_.setMode(ports::VehicleMode::Crank));
    EXPECT_EQ(mgr_.getMode(), ports::VehicleMode::Off);
}

TEST_F(VehicleModeManagerTest, ValidTransitionChain) {
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(3);

    EXPECT_TRUE(mgr_.setMode(ports::VehicleMode::Accessory));
    EXPECT_TRUE(mgr_.setMode(ports::VehicleMode::Run));
    EXPECT_TRUE(mgr_.setMode(ports::VehicleMode::Crank));
    EXPECT_EQ(mgr_.getMode(), ports::VehicleMode::Crank);
}

TEST_F(VehicleModeManagerTest, SubscriberNotification) {
    MockModeObserver observer;
    mgr_.addObserver(&observer);

    EXPECT_CALL(observer,
                onModeChanged(ports::VehicleMode::Off,
                              ports::VehicleMode::Accessory))
        .Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    mgr_.setMode(ports::VehicleMode::Accessory);
}

TEST_F(VehicleModeManagerTest, MultipleSubscribers) {
    MockModeObserver obs1, obs2, obs3;
    mgr_.addObserver(&obs1);
    mgr_.addObserver(&obs2);
    mgr_.addObserver(&obs3);

    EXPECT_CALL(obs1, onModeChanged(_, _)).Times(1);
    EXPECT_CALL(obs2, onModeChanged(_, _)).Times(1);
    EXPECT_CALL(obs3, onModeChanged(_, _)).Times(1);
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(1);

    mgr_.setMode(ports::VehicleMode::Accessory);
}

TEST_F(VehicleModeManagerTest, SameModeSetsNoOp) {
    EXPECT_CALL(someip_, sendEvent(_, _, _)).Times(0);
    EXPECT_TRUE(mgr_.setMode(ports::VehicleMode::Off));
}
