#include <gtest/gtest.h>

#include "CanGatewaySystem.h"
#include "DiagnosticsSystem.h"
#include "DoorLockSystem.h"
#include "LightingSystem.h"
#include "VehicleModeSystem.h"
#include "MockButtonInput.h"
#include "MockCanBus.h"
#include "MockGpioPort.h"

using namespace body_ecu::adapters;
using namespace body_ecu::mocks;
namespace body = body_ecu::body;
namespace platform = body_ecu::platform;
namespace ports = body_ecu::ports;
using body_ecu::ports::VehicleMode;
using ::testing::_;

class LightingSystemTest : public ::testing::Test {
protected:
    MockGpioPort gpio_;
    SomeIpSystem someip_;
    LightingSystem sys_{gpio_, someip_};
};

TEST_F(LightingSystemTest, InitRegistersMethodsAndEvents) {
    sys_.init();
    auto& ctrl = sys_.controller();
    EXPECT_FALSE(ctrl.getLightStatus()[0]);
}

TEST_F(LightingSystemTest, ShutdownTurnsOffAllLights) {
    EXPECT_CALL(gpio_, write(0, true)).Times(1);
    EXPECT_CALL(gpio_, write(0, false)).Times(1);
    EXPECT_CALL(gpio_, write(1, false)).Times(1);
    EXPECT_CALL(gpio_, write(2, false)).Times(1);

    sys_.init();
    sys_.controller().setLightState(body::LightId::Headlight, true);
    sys_.shutdown();
}

class DoorLockSystemTest : public ::testing::Test {
protected:
    MockGpioPort gpio_;
    MockButtonInput button_;
    SomeIpSystem someip_;
    DoorLockSystem sys_{gpio_, button_, someip_};
};

TEST_F(DoorLockSystemTest, InitAndLifecycle) {
    EXPECT_CALL(button_, onPress(_)).Times(1);
    sys_.init();
    EXPECT_EQ(sys_.controller().getState(), body::LockState::Unlocked);
    sys_.run();
    sys_.shutdown();
    EXPECT_EQ(sys_.controller().getState(), body::LockState::Unlocked);
}

TEST_F(DoorLockSystemTest, LockAndShutdownUnlocks) {
    EXPECT_CALL(button_, onPress(_)).Times(1);
    EXPECT_CALL(gpio_, write(_, _)).Times(testing::AtLeast(1));

    sys_.init();
    sys_.controller().lock();
    EXPECT_EQ(sys_.controller().getState(), body::LockState::Locked);

    sys_.shutdown();
    EXPECT_EQ(sys_.controller().getState(), body::LockState::Unlocked);
}

class VehicleModeSystemTest : public ::testing::Test {
protected:
    SomeIpSystem someip_;
    VehicleModeSystem sys_{someip_};
};

TEST_F(VehicleModeSystemTest, InitAndLifecycle) {
    sys_.init();
    EXPECT_EQ(sys_.manager().getMode(), VehicleMode::Off);
    sys_.run();
    sys_.manager().setMode(VehicleMode::Accessory);
    EXPECT_EQ(sys_.manager().getMode(), VehicleMode::Accessory);
    sys_.shutdown();
    EXPECT_EQ(sys_.manager().getMode(), VehicleMode::Off);
}

class CanGatewaySystemTest : public ::testing::Test {
protected:
    MockCanBus can_;
    SomeIpSystem someip_;
    CanGatewaySystem sys_{can_, someip_};
};

TEST_F(CanGatewaySystemTest, LifecycleStartsAndStopsGateway) {
    platform::ServiceMapping mapping;
    mapping.name = "test";
    mapping.direction = platform::GatewayDirection::SomeIpToCan;
    mapping.someip_service_id = 0x1000;
    mapping.someip_method_id = 0x0001;
    mapping.can_id = 0x200;
    mapping.can_dlc = 4;
    sys_.addMapping(mapping);

    sys_.init();
    sys_.run();
    EXPECT_TRUE(sys_.gateway().isRunning());

    sys_.shutdown();
    EXPECT_FALSE(sys_.gateway().isRunning());
}

class DiagnosticsSystemTest : public ::testing::Test {
protected:
    DiagnosticsSystem sys_;
};

TEST_F(DiagnosticsSystemTest, InitAndHandleRequest) {
    sys_.init();

    // No providers: sending 0x22 should return negative response
    platform::DiagRequest req = {0x22, 0xF1, 0x00};
    auto resp = sys_.handler().handleRequest(req);

    // Should get NRC 0x31 (requestOutOfRange) for unknown DID
    EXPECT_EQ(resp[0], 0x7F);
    EXPECT_EQ(resp[1], 0x22);
    EXPECT_EQ(resp[2], 0x31);
}
