#include "common/busid/BusId.h"

namespace common
{
namespace busid
{
char const* getBusName(uint8_t const busId)
{
    switch (busId)
    {
        case 1: return "SELFDIAG";
        case 2: return "CAN_0";
        default: return "UNKNOWN";
    }
}
} // namespace busid
} // namespace common
