#pragma once

#include "ports/IButtonInput.h"

namespace body_ecu::ports {

class NullButtonInput : public IButtonInput {
public:
    void onPress(ButtonCallback /*callback*/) override {}
};

}  // namespace body_ecu::ports
