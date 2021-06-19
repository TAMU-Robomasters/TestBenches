#include "logic/decisions.hpp"
#include "cmsis_os.h"

float position = 0;
float testSpeed = METERSPS_TO_RPM(0.25f);

namespace decisions {

void task() {
    osDelay(2000);

    for (;;) {
        position++;
        //chassis::currState = chassis::chassisStates::yield;
        //chassis::positionMove(6); // to 6in on the rail
        // chassis::currState = chassis::chassisStates::yield;
        // chassis::profiledMove(IN_TO_METER(9)); // 5in
        // chassis::currState = chassis::chassisStates::notRunning;
        // position++;

        osDelay(5000);
    }
}

}; // namespace decisions