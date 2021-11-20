#include "subsystems/chassis.hpp"

#include <arm_math.h>

#include "information/can_protocol.hpp"
#include "information/filters.hpp"
#include "information/pid.hpp"
#include "information/pwm_protocol.hpp"
#include "information/rc_protocol.h"
#include "information/uart_protocol.hpp"
#include "init.hpp"
#include "logic/settledUtils.hpp"
#include "movement/SCurveAcceleration.hpp"
#include "movement/SCurveMotionProfile.hpp"

float currTime;
float angle, magnitude;
float angleOutput;
float turning, disp;
float motor1P, motor2P, motor3P, motor4P;
float c1SentPower, c1Derivative, c1Output;
int c1Speed, c2Speed;

double railPosition;
float lastChassisAngle;

// int direction = 1;
// float rpmBoost = 1;

float c1Rx;
uint8_t chassisMsg[5];

float posTargetShow = 0;

int chasStateShow;

bool chassisPosSettled = false;
float dist = 0;

// INCLUDE userDebugFiles/chassis1DisplayValues.ini

namespace chassis {

float velTarget = 0;
float posTarget = 0;

SCurveMotionProfile::Constraints profileConstraints{0.25, 2.0, 10.0};  // m/s, m/s/s, m/s/s/s

float wheelDiameter = 1.62;  // 1.62inches

chassisStates currState = manual;
CtrlTypes ctrlType = CURRENT;
// i don't really like this but do i care enough to change it?

filter::Kalman chassisVelFilter(0.05, 16.0, 1023.0, 0.0);

pidInstance velPidC1(pidType::velocity, 0.2, 0.000, 5.000);
pidInstance velPidC2(pidType::velocity, 0.2, 0.000, 5.000);
pidInstance posPidChassis(pidType::position, 1.5, 0, 0);

chassisMotor c1Motor(userCAN::M3508_M1_ID, velPidC1, chassisVelFilter);
chassisMotor c2Motor(userCAN::M3508_M1_ID, velPidC1, chassisVelFilter);

void task() {
    // SCurveMotionProfile movement(profileConstraints, 1); // move 1 meter

    for (;;) {
        // updateRailPosition();

        update();

        act();
        // set power to global variable here, message is actually sent with the others in the CAN task

        osDelay(5);
    }
}

void update() {
    struct userUART::chassisMsgStruct* pxChassisRxedPointer;

    if (getSwitch(switchType::left) == switchPosition::mid) {
        currState = manual;
    } else if (getSwitch(switchType::left) == switchPosition::up) {
        currState = manual;
    } else {
        currState = notRunning;
    }

    // if (operatingType == primary) {
    //     // currState = manual;
    //     // will change later based on RC input and sensor based decision making
    // }

    // if (operatingType == secondary) {
    //     // currState = notRunning; // default state if not updated by primary board
    //     angleOutput = radToDeg(angle);

    //     if (userUART::chassisMsgQueue != NULL) {
    //         if (xQueueReceive(userUART::chassisMsgQueue, &(pxChassisRxedPointer), (TickType_t)0) == pdPASS) {
    //             if (pxChassisRxedPointer->prefix == userUART::d2dMsgTypes::chassis) {
    //                 c1Output = c1Motor.getSpeed();
    //                 currState = pxChassisRxedPointer->state;
    //                 c1Rx = pxChassisRxedPointer->m1;
    //             }
    //         }
    //     }
    // }

    // currTime = HAL_GetTick();
    // chasStateShow = currState;

    // velPidC1.setTarget(100);
}

void act() {
    switch (currState) {
        case notRunning:
            velPidC1.setTarget(0);
            velPidC2.setTarget(0);
            c1Motor.setPower(velPidC1.loop(c1Motor.getSpeed()));
            c2Motor.setPower(velPidC2.loop(c2Motor.getSpeed()));
            break;

        case manual:
            // if (operatingType == primary) {
            //    angle = atan2(getJoystick(joystickAxis::leftY), getJoystick(joystickAxis::leftX));
            //    magnitude = sqrt(pow(getJoystick(joystickAxis::leftY), 2) + pow(getJoystick(joystickAxis::leftX), 2));
            //    rcToPower(angle, magnitude, getJoystick(joystickAxis::rightX));
            //}

            // if (operatingType == secondary) {
            // c1SentPower = (c1Motor.getPower() * 16384.0f) / 100.0f;
            c1Derivative = velPidC1.getDerivative();

            if (getSwitch(switchType::right) == switchPosition::down) {
                velPidC1.setTarget(-400);
                velPidC2.setTarget(400);
            } else if (getSwitch(switchType::right) == switchPosition::mid) {
                velPidC1.setTarget(-450);
                velPidC2.setTarget(450);
            } else if (getSwitch(switchType::right) == switchPosition::up) {
                velPidC1.setTarget(-550);
                velPidC2.setTarget(550);
            }

            c1Speed = c1Motor.getSpeed();
            c2Speed = c2Motor.getSpeed();

            // c1Motor.setPower(velPidC1.getTarget());
            c1Motor.setPower(velPidC1.loop(c1Motor.getSpeed()));
            c2Motor.setPower(velPidC2.loop(c2Motor.getSpeed()));
            // }
            // this will change when we have things to put here
            break;

        case toTargetVel:
            if (operatingType == secondary) {
                velPidC1.setTarget(c1Rx);
                c1Motor.setPower(velPidC1.loop(c1Motor.getSpeed()));
            }
            break;

        case toTargetPos:
            if (operatingType == secondary) {
                posPidChassis.setTarget(c1Rx);
                chassisPosSettled = isSettled(position, railPosition, posTarget, 0.5);
                dist = fabs(railPosition - c1Rx);
                if (!isSettled(position, railPosition, c1Rx, 0.5)) {
                    // c1Motor.setPower(velPidC1.loop(c1Motor.getSpeed()));
                    c1Motor.setPower(-posPidChassis.loop(railPosition, c1Motor.getSpeed()));
                    c1SentPower = c1Motor.getPower();
                } else {
                    currState = notRunning;
                }
            }
            break;

        case yield:
            // hands control off to decisions task
            break;
    }
}

void rcToPower(double angle, double magnitude, double yaw) {
    // Computes the appropriate fraction of the wheel's motor power

    // Sine and cosine of math.h take angle in radians as input value
    // motor 1 back left
    // motor 2 front left
    // motor 3 front right
    // motor 4 back right
    float turnScalar = 0.6;
    int rpmScaler = 200;

    disp = ((abs(magnitude) + abs(yaw)) == 0) ? 0 : abs(magnitude) / (abs(magnitude) + turnScalar * abs(yaw));
    turning = ((abs(magnitude) + abs(yaw)) == 0) ? 0 : turnScalar * abs(yaw) / (abs(magnitude) + turnScalar * abs(yaw));
    // disp = 1 - abs(turning);
    //  disp and turning represent the percentage of how much the user wants to displace or turn
    //  displacement takes priority here

    float motor1Turn = yaw;
    float motor2Turn = yaw;
    float motor3Turn = yaw;
    float motor4Turn = yaw;

    float motor1Disp = (magnitude * (sin(angle) - cos(angle))) * 1;
    float motor2Disp = (magnitude * (cos(angle) + sin(angle))) * 1;
    float motor3Disp = (magnitude * (sin(angle) - cos(angle))) * -1;
    float motor4Disp = (magnitude * (cos(angle) + sin(angle))) * -1;

    motor1P = (turning * motor1Turn) + (disp * motor1Disp);
    motor2P = (turning * motor2Turn) + (disp * motor2Disp);
    motor3P = (turning * motor3Turn) + (disp * motor3Disp);
    motor4P = (turning * motor4Turn) + (disp * motor4Disp);

    // velPidC1.setTarget(motor1P * 200);
    // velPidC2.setTarget(motor2P * 200);
    // velPidC3.setTarget(motor3P * 200);
    // velPidC4.setTarget(motor4P * 200);

    // scaling max speed up to 200 rpm, can be set up to 482rpm
    motor1P *= rpmScaler;
    motor2P *= rpmScaler;
    motor3P *= rpmScaler;
    motor4P *= rpmScaler;

    sendChassisMessage(motor1P);
}

void sendChassisMessage(float m1) {
    int16_t m1S = static_cast<int16_t>(m1 * 50);
    chassisMsg[0] = 'c';
    chassisMsg[1] = static_cast<uint8_t>(currState);
    chassisMsg[2] = (m1S + 32768) >> 8;
    chassisMsg[3] = (m1S + 32768);
    chassisMsg[4] = 'e';
    HAL_UART_Transmit(&huart8, (uint8_t*)chassisMsg, sizeof(chassisMsg), 1);
}

void updateRailPosition() {
    // delta is in Radians
    float deltaChas = gimbal::calculateAngleError(c1Motor.getAngle(), lastChassisAngle) * (1.0 / 19.0) * (44.0 / 18.0) * WHEEL_RADIUS;
    lastChassisAngle = c1Motor.getAngle();
    railPosition += deltaChas;
}

void profiledMove(float distance) {
    SCurveMotionProfile movement(profileConstraints, distance);
    currState = toTargetVel;
    for (float t = 0; t <= movement.totalTime(); t += 0.01f) {  // 0.01 second, 10ms steps
        auto step = movement.stepAtTime(t);                     // you have step.velocity, step.acceleration, etc, so move ur motors

        velTarget = static_cast<float>(METERSPS_TO_RPM(step.velocity));
        c1Output = velTarget;
        sendChassisMessage(velTarget);

        osDelay(10);  // 10ms
    }
    currState = yield;
}

void positionMove(float distance) {
    posTarget = distance;
    posTargetShow = posTarget;
    currState = toTargetPos;
    sendChassisMessage(posTarget);
    // waitUntilSettled(position, railPosition, posTarget, 1);
}

void velocityMove(float velocity, float time) {
    velTarget = velocity;
    currTime = time;
    currState = toTargetVel;
    while (currTime > 0) {
        sendChassisMessage(velTarget);
        time -= 10;
        osDelay(10);
    }
    currState = notRunning;
    sendChassisMessage(0);
}

}  // namespace chassis
