#include "subsystems/chassis.hpp"
#include "information/can_protocol.hpp"
#include "information/pid.hpp"
#include "information/pwm_protocol.hpp"
#include "information/rc_protocol.h"
#include "init.hpp"
#include <arm_math.h>

float rX;
float rY;
float lX;
float lY;
float switch1;
float switch2;
float angle;
float angleOutput;
float magnitude;
float c1Output;
float turning;
float disp;
float motor1P;
float motor2P;
float motor3P;
float motor4P;

namespace chassis {

chassisStates currState = notRunning;
CtrlTypes ctrlType = CURRENT;
// i don't really like this but do i care enough to change it?

pidInstance velPidC1(pidType::velocity, 0.7, 0.0, 0.0);
pidInstance velPidC2(pidType::velocity, 0.7, 0.0, 0.0);
pidInstance velPidC3(pidType::velocity, 0.7, 0.0, 0.0);
pidInstance velPidC4(pidType::velocity, 0.7, 0.0, 0.0);

chassisMotor c1Motor(userCAN::M3508_M1_ID, velPidC1);
chassisMotor c2Motor(userCAN::M3508_M2_ID, velPidC2);
chassisMotor c3Motor(userCAN::M3508_M3_ID, velPidC3);
chassisMotor c4Motor(userCAN::M3508_M4_ID, velPidC4);

void task() {
    for (;;) {
        update();

        act();
        // set power to global variable here, message is actually sent with the others in the CAN task

        osDelay(2);
    }
}

void update() {
    if (true) {
        currState = notRunning;
        // will change later based on RC input and sensor based decision making
    }

    rX = getJoystick(joystickAxis::rightX) / 660;
    rY = getJoystick(joystickAxis::rightY) / 660;
    lX = getJoystick(joystickAxis::leftX) / 660;
    lY = getJoystick(joystickAxis::leftY) / 660;

    switch1 = (rcDataStruct.rc.s[0]);
    switch2 = (rcDataStruct.rc.s[1]);

    angle = atan2(lY, lX);
    angleOutput = (angle / (float)PI) * 180;

    magnitude = sqrt(pow(lY, 2) + pow(lX, 2));

    c1Output = static_cast<double>(c1Motor.getFeedback()->rotor_speed) / 19.0;

    velPidC1.setCurrInput(static_cast<float>(c1Motor.getFeedback()->rotor_speed) / 19);
    velPidC2.setCurrInput(static_cast<float>(c2Motor.getFeedback()->rotor_speed) / 19);
    velPidC3.setCurrInput(static_cast<float>(c3Motor.getFeedback()->rotor_speed) / 19);
    velPidC4.setCurrInput(static_cast<float>(c4Motor.getFeedback()->rotor_speed) / 19);

    // if button pressed on controller, change state to "followgimbal" or something
}

void act() {
    switch (currState) {
        case notRunning:
            c1Motor.setPower(0);
            c2Motor.setPower(0);
            c3Motor.setPower(0);
            c4Motor.setPower(0);
            break;

        case followGimbal:
            c1Motor.setPower(0);
            c2Motor.setPower(0);
            c3Motor.setPower(0);
            c4Motor.setPower(0);
            // obviously this will change when we have things to put here
            break;

        case manual:
            rcToPower(angle, magnitude, rX);
            velPidC1.loop();
            velPidC2.loop();
            velPidC3.loop();
            velPidC4.loop();
            // double power1 = velPidC1.getTarget();
            // double power2 = velPidC2.getTarget();
            // double power3 = velPidC3.getTarget();
            // double power4 = velPidC4.getTarget();
            c1Motor.setPower(velPidC1.getTarget());
            c2Motor.setPower(velPidC2.getTarget());
            c3Motor.setPower(velPidC3.getTarget());
            c4Motor.setPower(velPidC4.getTarget());
            // if current control, power will be set in the CAN task
            // this will change when we have things to put here
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

    disp = ((abs(magnitude) + abs(yaw)) == 0) ? 0 : abs(magnitude) / (abs(magnitude) + turnScalar * abs(yaw));
    turning = ((abs(magnitude) + abs(yaw)) == 0) ? 0 : turnScalar * abs(yaw) / (abs(magnitude) + turnScalar * abs(yaw));
    //disp = 1 - abs(turning);
    // disp and turning represent the percentage of how much the user wants to displace or turn
    // displacement takes priority here

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

    velPidC1.setTarget(motor1P * 200);
    velPidC2.setTarget(motor2P * 200);
    velPidC3.setTarget(motor3P * 200);
    velPidC4.setTarget(motor4P * 200);
    // scaling max speed up to 200 rpm, can be set up to 482rpm
}

}  // namespace chassis
