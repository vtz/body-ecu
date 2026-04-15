#include <gtest/gtest.h>

#include "diagnostics/DoIpProtocol.h"

using namespace body_ecu::platform::doip;

TEST(DoIpProtocolTest, SerializeAndParseHeader) {
    auto data = serializeHeader(PayloadType::DiagnosticMessage, 42);
    ASSERT_EQ(data.size(), kHeaderLen);

    auto hdr = parseHeader(data.data());
    EXPECT_EQ(hdr.version, kProtocolVersion);
    EXPECT_EQ(hdr.inverse, kInverseVersion);
    EXPECT_EQ(hdr.payload_type,
              static_cast<uint16_t>(PayloadType::DiagnosticMessage));
    EXPECT_EQ(hdr.payload_length, 42u);
}

TEST(DoIpProtocolTest, RoutingActivationResponse) {
    auto msg = makeRoutingActivationResponse(
        0x0E00, 0x0E80, RoutingActivationCode::Success);

    ASSERT_EQ(msg.size(), kHeaderLen + 9);
    auto hdr = parseHeader(msg.data());
    EXPECT_EQ(hdr.payload_type,
              static_cast<uint16_t>(PayloadType::RoutingActivationResponse));
    EXPECT_EQ(hdr.payload_length, 9u);

    EXPECT_EQ(msg[kHeaderLen + 0], 0x0E);
    EXPECT_EQ(msg[kHeaderLen + 1], 0x00);
    EXPECT_EQ(msg[kHeaderLen + 2], 0x0E);
    EXPECT_EQ(msg[kHeaderLen + 3], 0x80);
    EXPECT_EQ(msg[kHeaderLen + 4],
              static_cast<uint8_t>(RoutingActivationCode::Success));
}

TEST(DoIpProtocolTest, DiagnosticMessage) {
    std::vector<uint8_t> uds = {0x22, 0xF1, 0x90};
    auto msg = makeDiagnosticMessage(0x0E80, 0x0E00, uds);

    ASSERT_EQ(msg.size(), kHeaderLen + 4 + uds.size());
    auto hdr = parseHeader(msg.data());
    EXPECT_EQ(hdr.payload_type,
              static_cast<uint16_t>(PayloadType::DiagnosticMessage));
    EXPECT_EQ(hdr.payload_length, 4 + uds.size());

    EXPECT_EQ(msg[kHeaderLen + 0], 0x0E);
    EXPECT_EQ(msg[kHeaderLen + 1], 0x80);
    EXPECT_EQ(msg[kHeaderLen + 2], 0x0E);
    EXPECT_EQ(msg[kHeaderLen + 3], 0x00);
    EXPECT_EQ(msg[kHeaderLen + 4], 0x22);
    EXPECT_EQ(msg[kHeaderLen + 5], 0xF1);
    EXPECT_EQ(msg[kHeaderLen + 6], 0x90);
}

TEST(DoIpProtocolTest, DiagnosticAck) {
    auto msg = makeDiagnosticAck(0x0E80, 0x0E00, 0x00);

    ASSERT_EQ(msg.size(), kHeaderLen + 5);
    auto hdr = parseHeader(msg.data());
    EXPECT_EQ(hdr.payload_type,
              static_cast<uint16_t>(PayloadType::DiagnosticPositiveAck));
    EXPECT_EQ(hdr.payload_length, 5u);
    EXPECT_EQ(msg[kHeaderLen + 4], 0x00);
}

TEST(DoIpProtocolTest, HeaderVersionInverse) {
    EXPECT_EQ(static_cast<uint8_t>(kProtocolVersion ^ 0xFF), kInverseVersion);
}
