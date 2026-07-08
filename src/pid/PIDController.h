#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>

class PIDController {
public:
    PIDController();
    PIDController(float kp, float ki, float kd);

    // Configure PID tunings
    void setTunings(float kp, float ki, float kd);

    // Set the target value
    void setSetpoint(float sp);

    // Set reverse acting mode (true = output rises when input > setpoint)
    void setReverseActing(bool reverse) { _reverseActing = reverse; }

    // Set output clamping range
    void setOutputLimits(float min, float max);

    // Compute output for given input and time step (seconds)
    // Returns the controller output
    float compute(float input, float dtSec);

    // Reset integral term and previous error
    void reset();

    // Diagnostic accessors
    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }
    float getP() const { return _pTerm; }
    float getI() const { return _iTerm; }
    float getD() const { return _dTerm; }
    float getSetpoint() const { return _setpoint; }
    float getIntegral() const { return _integral; }
    float getLastInput() const { return _lastInput; }

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool e) { _enabled = e; if (!e) reset(); }

private:
    float _kp, _ki, _kd;
    float _setpoint;
    float _outputMin, _outputMax;
    float _integral;
    float _lastInput;
    float _pTerm, _iTerm, _dTerm;
    bool _enabled;
    bool _reverseActing;
    bool _firstRun;
};

#endif // PID_CONTROLLER_H