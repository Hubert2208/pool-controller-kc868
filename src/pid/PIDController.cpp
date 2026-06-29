#include "PIDController.h"

PIDController::PIDController()
    : _kp(1.0f)
    , _ki(0.1f)
    , _kd(0.05f)
    , _setpoint(0.0f)
    , _outputMin(0.0f)
    , _outputMax(100.0f)
    , _integral(0.0f)
    , _lastInput(0.0f)
    , _pTerm(0.0f)
    , _iTerm(0.0f)
    , _dTerm(0.0f)
    , _enabled(true)
    , _firstRun(true)
{
}

PIDController::PIDController(float kp, float ki, float kd)
    : _kp(kp)
    , _ki(ki)
    , _kd(kd)
    , _setpoint(0.0f)
    , _outputMin(0.0f)
    , _outputMax(100.0f)
    , _integral(0.0f)
    , _lastInput(0.0f)
    , _pTerm(0.0f)
    , _iTerm(0.0f)
    , _dTerm(0.0f)
    , _enabled(true)
    , _firstRun(true)
{
}

void PIDController::setTunings(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;

    // Reset integral if Ki is 0
    if (_ki == 0.0f) {
        _integral = 0.0f;
    }
}

void PIDController::setSetpoint(float sp) {
    _setpoint = sp;
}

void PIDController::setOutputLimits(float min, float max) {
    if (min >= max) return;
    _outputMin = min;
    _outputMax = max;

    // Clamp integral to new limits
    if (_integral > _outputMax) _integral = _outputMax;
    if (_integral < _outputMin) _integral = _outputMin;
}

float PIDController::compute(float input, float dtSec) {
    if (!_enabled) {
        return 0.0f;
    }

    // Protect against dt = 0 or negative
    if (dtSec <= 0.0f) dtSec = 0.1f;

    // Limit dt to prevent derivative kick on long pauses
    if (dtSec > 5.0f) dtSec = 5.0f;

    float error = _setpoint - input;

    // Calculate proportional term
    _pTerm = _kp * error;

    // Calculate integral term with anti-windup clamping
    if (_firstRun) {
        _integral = 0.0f;
        _firstRun = false;
    } else {
        _integral += _ki * error * dtSec;

        // Clamp integral to prevent windup
        if (_integral > _outputMax) _integral = _outputMax;
        if (_integral < _outputMin) _integral = _outputMin;
    }
    _iTerm = _integral;

    // Calculate derivative term (on measurement, not error, to avoid derivative kick)
    float dInput = (_lastInput - input) / dtSec;
    _dTerm = _kd * dInput;

    // Sum terms
    float output = _pTerm + _iTerm + _dTerm;

    // Clamp output
    if (output > _outputMax) output = _outputMax;
    if (output < _outputMin) output = _outputMin;

    // Store for next iteration
    _lastInput = input;

    return output;
}

void PIDController::reset() {
    _integral = 0.0f;
    _lastInput = 0.0f;
    _pTerm = 0.0f;
    _iTerm = 0.0f;
    _dTerm = 0.0f;
    _firstRun = true;
}