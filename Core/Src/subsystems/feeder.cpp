#include "subsystems/feeder.hpp"
#include "can.h"
#include "information/can_protocol.hpp"
#include "information/uart_protocol.hpp"
#include "information/rc_protocol.h"
#include "init.hpp"
#include "main.h"

namespace feeder {

feederStates currState = notRunning;

float feederPower = 0;
int direction = 1;

uint16_t angle = 0;
int16_t speed = 0;
int16_t torque_current = 0;
int8_t temp = 0;

void task() {
    //osDelay(5000);

    for (;;) {
        update();

        act();

        osDelay(10);
    }
}

void update() {
    angle = canDevice.feeder_fb.rotor_angle;
    speed = canDevice.feeder_fb.rotor_speed / 36;
    torque_current = canDevice.feeder_fb.torque_current;

    if (getSwitch(switchType::left) == switchPosition::mid) {
        currState = running;
        direction = 1;
    } else if (getSwitch(switchType::right) == switchPosition::up) {
        currState = running;
        direction = -1;
    } else {
        currState = notRunning;
    }
}

void act() {
    switch (currState) {
        case notRunning:
            feederPower = 0;
            break;

        case running:
            if (getSwitch(switchType::right) == switchPosition::down) {
                feederPower = 0.333f * direction;
            } else if (getSwitch(switchType::right) == switchPosition::mid) {
                feederPower = 0.666f * direction;
            } else if (getSwitch(switchType::right) == switchPosition::up) {
                feederPower = 1.0f * direction;
            }
            break;
    }
}
// set power to global variable here, message is actually sent with the others in the CAN task

}  // namespace feeder
