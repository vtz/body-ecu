#include <gtest/gtest.h>

#include "VehicleInfoProvider.h"
#include "diagnostics/DtcStore.h"
#include "diagnostics/UdsServiceHandler.h"

using namespace body_ecu;
using namespace body_ecu::adapters;
using namespace body_ecu::platform;

TEST(VehicleInfoTest, ReadVinDefault) {
    VehicleInfoProvider provider;
    auto data = provider.readData(VehicleInfoProvider::kDidVin);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data->did, 0xF190);
    ASSERT_EQ(data->data.size(), 17u);
    for (auto b : data->data) {
        EXPECT_EQ(b, '0');
    }
}

TEST(VehicleInfoTest, ReadVinCustom) {
    VehicleInfoProvider provider;
    provider.setVin("WVWZZZ3CZWE123456");
    auto data = provider.readData(VehicleInfoProvider::kDidVin);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(data->data.size(), 17u);
    std::string vin(data->data.begin(), data->data.end());
    EXPECT_EQ(vin, "WVWZZZ3CZWE123456");
}

TEST(VehicleInfoTest, ReadEcuSerial) {
    VehicleInfoProvider provider;
    provider.setEcuSerial("BECU-001");
    auto data = provider.readData(VehicleInfoProvider::kDidEcuSerial);
    ASSERT_TRUE(data.has_value());
    std::string serial(data->data.begin(), data->data.end());
    EXPECT_EQ(serial, "BECU-001");
}

TEST(VehicleInfoTest, UnknownDid) {
    VehicleInfoProvider provider;
    auto data = provider.readData(0xFFFF);
    EXPECT_FALSE(data.has_value());
}

TEST(VehicleInfoTest, IoControlAlwaysFalse) {
    VehicleInfoProvider provider;
    EXPECT_FALSE(provider.ioControl(0xF190, {}));
}

TEST(VehicleInfoTest, ReadVinViaUdsHandler) {
    VehicleInfoProvider provider;
    provider.setVin("WVWZZZ3CZWE123456");

    DtcStore dtc_store;
    UdsServiceHandler handler(dtc_store);
    handler.addProvider(&provider);

    DiagRequest req = {0x22, 0xF1, 0x90};
    auto resp = handler.handleRequest(req);

    ASSERT_GE(resp.size(), 3u + 17u);
    EXPECT_EQ(resp[0], 0x62);
    EXPECT_EQ(resp[1], 0xF1);
    EXPECT_EQ(resp[2], 0x90);

    std::string vin(resp.begin() + 3, resp.begin() + 3 + 17);
    EXPECT_EQ(vin, "WVWZZZ3CZWE123456");
}
