#include <gtest/gtest.h>

#include "MockDiagDataProvider.h"
#include "diagnostics/DtcStore.h"
#include "diagnostics/UdsServiceHandler.h"

using namespace body_ecu;
using namespace body_ecu::platform;
using namespace body_ecu::mocks;
using ::testing::_;
using ::testing::Return;

class DiagnosticsTest : public ::testing::Test {
protected:
    void SetUp() override { handler_.addProvider(&provider_); }

    MockDiagDataProvider provider_;
    DtcStore dtc_store_;
    UdsServiceHandler handler_{dtc_store_};
};

TEST_F(DiagnosticsTest, ReadDataByIdLighting) {
    ports::DiagData diag_data;
    diag_data.did = 0xF100;
    diag_data.data = {1, 0, 1};

    EXPECT_CALL(provider_, readData(0xF100))
        .WillOnce(Return(std::optional<ports::DiagData>(diag_data)));

    DiagRequest req = {0x22, 0xF1, 0x00};
    auto resp = handler_.handleRequest(req);

    ASSERT_GE(resp.size(), 4u);
    EXPECT_EQ(resp[0], 0x62);
    EXPECT_EQ(resp[1], 0xF1);
    EXPECT_EQ(resp[2], 0x00);
    EXPECT_EQ(resp[3], 1);
    EXPECT_EQ(resp[4], 0);
    EXPECT_EQ(resp[5], 1);
}

TEST_F(DiagnosticsTest, ReadDataByIdDoor) {
    ports::DiagData diag_data;
    diag_data.did = 0xF101;
    diag_data.data = {0x01};

    EXPECT_CALL(provider_, readData(0xF101))
        .WillOnce(Return(std::optional<ports::DiagData>(diag_data)));

    DiagRequest req = {0x22, 0xF1, 0x01};
    auto resp = handler_.handleRequest(req);

    ASSERT_GE(resp.size(), 4u);
    EXPECT_EQ(resp[0], 0x62);
    EXPECT_EQ(resp[3], 0x01);
}

TEST_F(DiagnosticsTest, ReadDataByIdMode) {
    ports::DiagData diag_data;
    diag_data.did = 0xF102;
    diag_data.data = {0x02};

    EXPECT_CALL(provider_, readData(0xF102))
        .WillOnce(Return(std::optional<ports::DiagData>(diag_data)));

    DiagRequest req = {0x22, 0xF1, 0x02};
    auto resp = handler_.handleRequest(req);

    ASSERT_GE(resp.size(), 4u);
    EXPECT_EQ(resp[0], 0x62);
    EXPECT_EQ(resp[3], 0x02);
}

TEST_F(DiagnosticsTest, UnsupportedDid) {
    EXPECT_CALL(provider_, readData(0xFFFF))
        .WillOnce(Return(std::nullopt));

    DiagRequest req = {0x22, 0xFF, 0xFF};
    auto resp = handler_.handleRequest(req);

    ASSERT_EQ(resp.size(), 3u);
    EXPECT_EQ(resp[0], 0x7F);
    EXPECT_EQ(resp[1], 0x22);
    EXPECT_EQ(resp[2], 0x31);
}

TEST_F(DiagnosticsTest, IoControlInExtendedSession) {
    DiagRequest session_req = {0x10, 0x03};
    handler_.handleRequest(session_req);

    EXPECT_CALL(provider_, ioControl(0xF100, std::vector<uint8_t>{0x00, 0x01}))
        .WillOnce(Return(true));

    DiagRequest req = {0x2F, 0xF1, 0x00, 0x00, 0x01};
    auto resp = handler_.handleRequest(req);

    ASSERT_GE(resp.size(), 3u);
    EXPECT_EQ(resp[0], 0x6F);
    EXPECT_EQ(resp[1], 0xF1);
    EXPECT_EQ(resp[2], 0x00);
}

TEST_F(DiagnosticsTest, IoControlRejectedInDefaultSession) {
    EXPECT_EQ(handler_.currentSession(), DiagSession::Default);

    DiagRequest req = {0x2F, 0xF1, 0x00, 0x00, 0x01};
    auto resp = handler_.handleRequest(req);

    ASSERT_EQ(resp.size(), 3u);
    EXPECT_EQ(resp[0], 0x7F);
    EXPECT_EQ(resp[1], 0x2F);
    EXPECT_EQ(resp[2], 0x22);
}

TEST_F(DiagnosticsTest, ReadDtcWithStoredDtcs) {
    dtc_store_.store(0x010203, 0x09);
    dtc_store_.store(0x040506, 0x27);

    DiagRequest req = {0x19, 0x02};
    auto resp = handler_.handleRequest(req);

    EXPECT_EQ(resp[0], 0x59);
    EXPECT_EQ(resp[1], 0x02);
    ASSERT_EQ(resp.size(), 10u);

    EXPECT_EQ(resp[2], 0x01);
    EXPECT_EQ(resp[3], 0x02);
    EXPECT_EQ(resp[4], 0x03);
    EXPECT_EQ(resp[5], 0x09);

    EXPECT_EQ(resp[6], 0x04);
    EXPECT_EQ(resp[7], 0x05);
    EXPECT_EQ(resp[8], 0x06);
    EXPECT_EQ(resp[9], 0x27);
}

TEST_F(DiagnosticsTest, ReadDtcNoDtcs) {
    DiagRequest req = {0x19, 0x02};
    auto resp = handler_.handleRequest(req);

    ASSERT_EQ(resp.size(), 2u);
    EXPECT_EQ(resp[0], 0x59);
    EXPECT_EQ(resp[1], 0x02);
}

TEST_F(DiagnosticsTest, SessionControl) {
    EXPECT_EQ(handler_.currentSession(), DiagSession::Default);

    auto resp = handler_.handleRequest({0x10, 0x03});
    EXPECT_EQ(resp[0], 0x50);
    EXPECT_EQ(handler_.currentSession(), DiagSession::Extended);

    resp = handler_.handleRequest({0x10, 0x01});
    EXPECT_EQ(resp[0], 0x50);
    EXPECT_EQ(handler_.currentSession(), DiagSession::Default);
}

TEST_F(DiagnosticsTest, UnsupportedService) {
    DiagRequest req = {0xFF};
    auto resp = handler_.handleRequest(req);

    ASSERT_EQ(resp.size(), 3u);
    EXPECT_EQ(resp[0], 0x7F);
    EXPECT_EQ(resp[1], 0xFF);
    EXPECT_EQ(resp[2], 0x11);
}
