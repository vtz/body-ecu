#include <gtest/gtest.h>

#include "DoCanTransport.h"
#include "DoIpTransport.h"
#include "MockCanBus.h"

using namespace body_ecu::adapters;
using namespace body_ecu::mocks;
namespace platform = body_ecu::platform;
namespace ports = body_ecu::ports;
using platform::DiagRequest;
using platform::DiagResponse;
using ::testing::_;

TEST(DoIpTransportTest, InitAndShutdown) {
    DoIpTransport transport;
    EXPECT_FALSE(transport.isConnected());

    transport.init();
    EXPECT_TRUE(transport.isConnected());

    transport.shutdown();
    EXPECT_FALSE(transport.isConnected());
}

TEST(DoIpTransportTest, RequestHandlerDispatch) {
    DoIpTransport transport;
    transport.init();

    bool handler_called = false;
    transport.setRequestHandler([&](const DiagRequest& req) -> DiagResponse {
        handler_called = true;
        return {static_cast<uint8_t>(req[0] + 0x40), req[1], req[2]};
    });

    transport.onDoIpRequest({0x22, 0xF1, 0x00});
    EXPECT_TRUE(handler_called);
}

TEST(DoIpTransportTest, NoHandlerNoOp) {
    DoIpTransport transport;
    transport.init();
    // Should not crash when no handler is set
    transport.onDoIpRequest({0x22, 0xF1, 0x00});
}

TEST(DoCanTransportTest, SingleFrameRequestDispatch) {
    MockCanBus can;
    DoCanTransport transport(can);

    EXPECT_CALL(can, setRxCallback(_)).WillOnce([&](ports::CanRxCallback cb) {
        // Simulate an incoming single-frame diagnostic request
        ports::CanFrame frame;
        frame.id = DoCanTransport::kDiagRxCanId;  // 0x600
        frame.dlc = 4;
        frame.data[0] = 0x03;  // SF PCI: length=3
        frame.data[1] = 0x22;  // SID: ReadDataByIdentifier
        frame.data[2] = 0xF1;  // DID high
        frame.data[3] = 0x00;  // DID low
        cb(frame);
    });

    bool handler_called = false;
    transport.setRequestHandler([&](const DiagRequest& req) -> DiagResponse {
        handler_called = true;
        EXPECT_EQ(req.size(), 3u);
        EXPECT_EQ(req[0], 0x22);
        return {0x62, 0xF1, 0x00, 0x01};
    });

    // Response frame should be sent back
    EXPECT_CALL(can, send(_)).WillOnce([](const ports::CanFrame& f) {
        EXPECT_EQ(f.id, DoCanTransport::kDiagTxCanId);
        EXPECT_EQ(f.data[0], 0x04);  // SF PCI: length=4
        EXPECT_EQ(f.data[1], 0x62);  // positive response SID
        return true;
    });

    transport.init();
    EXPECT_TRUE(handler_called);
}

TEST(DoCanTransportTest, IgnoresNonDiagCanIds) {
    MockCanBus can;
    DoCanTransport transport(can);

    EXPECT_CALL(can, setRxCallback(_)).WillOnce([&](ports::CanRxCallback cb) {
        ports::CanFrame frame;
        frame.id = 0x999;  // Not the diagnostic CAN ID
        frame.dlc = 4;
        frame.data[0] = 0x03;
        frame.data[1] = 0x22;
        frame.data[2] = 0xF1;
        frame.data[3] = 0x00;
        cb(frame);
    });

    bool handler_called = false;
    transport.setRequestHandler([&](const DiagRequest&) -> DiagResponse {
        handler_called = true;
        return {};
    });

    transport.init();
    EXPECT_FALSE(handler_called);
}
