#include <gtest/gtest.h>

#include "linux_adapters/InProcessSignalBus.h"

using namespace body_ecu;
using namespace body_ecu::adapters;

class InProcessSignalBusTest : public ::testing::Test {
protected:
    InProcessSignalBus bus_;
};

TEST_F(InProcessSignalBusTest, PublishAndGet) {
    EXPECT_TRUE(bus_.publish("Vehicle.Speed", ports::SignalValue{42.5f}));
    auto val = bus_.get("Vehicle.Speed");
    ASSERT_TRUE(val.has_value());
    auto* f = std::get_if<float>(&*val);
    ASSERT_NE(f, nullptr);
    EXPECT_FLOAT_EQ(*f, 42.5f);
}

TEST_F(InProcessSignalBusTest, GetNonExistentReturnsNullopt) {
    EXPECT_FALSE(bus_.get("Vehicle.NonExistent").has_value());
}

TEST_F(InProcessSignalBusTest, PublishOverwritesPrevious) {
    bus_.publish("Vehicle.Speed", ports::SignalValue{10.0f});
    bus_.publish("Vehicle.Speed", ports::SignalValue{20.0f});

    auto val = bus_.get("Vehicle.Speed");
    ASSERT_TRUE(val.has_value());
    auto* f = std::get_if<float>(&*val);
    ASSERT_NE(f, nullptr);
    EXPECT_FLOAT_EQ(*f, 20.0f);
}

TEST_F(InProcessSignalBusTest, SubscribeReceivesPublished) {
    std::string received_path;
    ports::SignalValue received_value;
    bool called = false;

    bus_.subscribe("Vehicle.Speed",
                   [&](const std::string& path, const ports::SignalValue& v) {
                       received_path = path;
                       received_value = v;
                       called = true;
                   });

    bus_.publish("Vehicle.Speed", ports::SignalValue{55.0f});

    EXPECT_TRUE(called);
    EXPECT_EQ(received_path, "Vehicle.Speed");
    auto* f = std::get_if<float>(&received_value);
    ASSERT_NE(f, nullptr);
    EXPECT_FLOAT_EQ(*f, 55.0f);
}

TEST_F(InProcessSignalBusTest, SubscribeDoesNotReceiveOtherPaths) {
    bool called = false;

    bus_.subscribe("Vehicle.Speed",
                   [&](const std::string&, const ports::SignalValue&) {
                       called = true;
                   });

    bus_.publish("Vehicle.Cabin.Door.Row1.DriverSide.IsLocked",
                 ports::SignalValue{true});

    EXPECT_FALSE(called);
}

TEST_F(InProcessSignalBusTest, MultipleSubscribers) {
    int call_count = 0;

    bus_.subscribe("Vehicle.Speed",
                   [&](const std::string&, const ports::SignalValue&) {
                       call_count++;
                   });
    bus_.subscribe("Vehicle.Speed",
                   [&](const std::string&, const ports::SignalValue&) {
                       call_count++;
                   });

    bus_.publish("Vehicle.Speed", ports::SignalValue{10.0f});

    EXPECT_EQ(call_count, 2);
}

TEST_F(InProcessSignalBusTest, BoolSignal) {
    bus_.publish("Vehicle.Cabin.Door.Row1.DriverSide.IsLocked",
                 ports::SignalValue{true});

    auto val = bus_.get("Vehicle.Cabin.Door.Row1.DriverSide.IsLocked");
    ASSERT_TRUE(val.has_value());
    auto* b = std::get_if<bool>(&*val);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(*b);
}

TEST_F(InProcessSignalBusTest, Int32Signal) {
    bus_.publish("Vehicle.Command.Door.Response",
                 ports::SignalValue{static_cast<int32_t>(0)});

    auto val = bus_.get("Vehicle.Command.Door.Response");
    ASSERT_TRUE(val.has_value());
    auto* i = std::get_if<int32_t>(&*val);
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(*i, 0);
}

TEST_F(InProcessSignalBusTest, StringSignal) {
    bus_.publish("Vehicle.VIN",
                 ports::SignalValue{std::string("WVWZZZ3CZWE000001")});

    auto val = bus_.get("Vehicle.VIN");
    ASSERT_TRUE(val.has_value());
    auto* s = std::get_if<std::string>(&*val);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "WVWZZZ3CZWE000001");
}
