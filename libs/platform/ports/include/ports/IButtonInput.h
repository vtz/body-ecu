#pragma once

#include <functional>

namespace body_ecu::ports {

using ButtonCallback = std::function<void()>;

class IButtonInput {
public:
    virtual ~IButtonInput() = default;
    virtual void onPress(ButtonCallback callback) = 0;
};

}  // namespace body_ecu::ports
