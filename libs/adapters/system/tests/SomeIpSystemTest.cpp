#include <gtest/gtest.h>

#include "SomeIpSystem.h"

using namespace body_ecu::adapters;
using namespace body_ecu::ports;

class SomeIpSystemTest : public ::testing::Test {
protected:
    SomeIpSystem sys_{{.host = "127.0.0.1", .port = 30490}};
};

TEST_F(SomeIpSystemTest, LifecycleTransitions) {
    EXPECT_FALSE(sys_.isRunning());
    sys_.init();
    EXPECT_FALSE(sys_.isRunning());
    sys_.run();
    EXPECT_TRUE(sys_.isRunning());
    sys_.shutdown();
    EXPECT_FALSE(sys_.isRunning());
}

TEST_F(SomeIpSystemTest, MethodRegistrationAndDispatch) {
    sys_.init();

    bool handler_called = false;
    std::vector<uint8_t> received_payload;

    sys_.registerMethod(0x1000, 0x0001,
                        [&](const SomeIpMessage& req) -> SomeIpMessage {
                            handler_called = true;
                            received_payload = req.payload;
                            SomeIpMessage resp = req;
                            resp.message_type = 0x80;
                            resp.return_code = 0x00;
                            resp.payload = {0xAA};
                            return resp;
                        });

    SomeIpMessage request;
    request.service_id = 0x1000;
    request.method_id = 0x0001;
    request.client_id = 0x0042;
    request.session_id = 0x0001;
    request.payload = {0x01, 0x02};

    auto response = sys_.dispatch(request);

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_payload, (std::vector<uint8_t>{0x01, 0x02}));
    EXPECT_EQ(response.message_type, 0x80);
    EXPECT_EQ(response.return_code, 0x00);
    EXPECT_EQ(response.service_id, 0x1000);
    EXPECT_EQ(response.method_id, 0x0001);
    EXPECT_EQ(response.client_id, 0x0042);
    EXPECT_EQ(response.payload, (std::vector<uint8_t>{0xAA}));
}

TEST_F(SomeIpSystemTest, UnregisteredMethodReturnsError) {
    sys_.init();

    SomeIpMessage request;
    request.service_id = 0x9999;
    request.method_id = 0x0001;

    auto response = sys_.dispatch(request);

    EXPECT_EQ(response.message_type, 0x81);
    EXPECT_EQ(response.return_code, 0x05);
}

TEST_F(SomeIpSystemTest, EventPublish) {
    sys_.init();
    sys_.registerEvent(0x1000, 0x8001, 0x0001);

    std::vector<uint8_t> payload = {0x01, 0x00, 0x01};
    sys_.sendEvent(0x1000, 0x8001, payload);

    ASSERT_EQ(sys_.sentEvents().size(), 1u);
    const auto& evt = sys_.sentEvents()[0];
    EXPECT_EQ(evt.service_id, 0x1000);
    EXPECT_EQ(evt.method_id, 0x8001);
    EXPECT_EQ(evt.message_type, 0x02);
    EXPECT_EQ(evt.return_code, 0x00);
    EXPECT_EQ(evt.payload, payload);
}

TEST_F(SomeIpSystemTest, ResponseBuilding) {
    sys_.init();

    SomeIpMessage response;
    response.service_id = 0x1000;
    response.method_id = 0x0001;
    response.client_id = 0x0042;
    response.session_id = 0x0007;
    response.message_type = 0x80;
    response.return_code = 0x00;
    response.payload = {0xDE, 0xAD};

    sys_.sendResponse(response);

    ASSERT_EQ(sys_.sentResponses().size(), 1u);
    const auto& r = sys_.sentResponses()[0];
    EXPECT_EQ(r.service_id, 0x1000);
    EXPECT_EQ(r.client_id, 0x0042);
    EXPECT_EQ(r.session_id, 0x0007);
    EXPECT_EQ(r.message_type, 0x80);
    EXPECT_EQ(r.return_code, 0x00);
    EXPECT_EQ(r.payload, (std::vector<uint8_t>{0xDE, 0xAD}));
}

TEST_F(SomeIpSystemTest, ShutdownClearsRegistrations) {
    sys_.init();
    sys_.registerMethod(0x1000, 0x0001,
                        [](const SomeIpMessage& req) { return req; });
    sys_.shutdown();

    SomeIpMessage request;
    request.service_id = 0x1000;
    request.method_id = 0x0001;
    auto response = sys_.dispatch(request);
    EXPECT_EQ(response.message_type, 0x81);
}
