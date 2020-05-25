//Author: sdavi

#ifndef SOFTWAREPWM_H
#define SOFTWAREPWM_H

#include "Core.h"
#include "SoftwarePWMTimer.h"

class SoftwarePWM
{
public:
    SoftwarePWM(Pin softPWMPin);
    
    void Enable();
    void Disable();

    void SetFrequency(uint16_t freq);
    void SetDutyCycle(float duty);

    bool IsRunning() {return pwmRunning;}
    void Check();

    
    Pin GetPin() const {return pin;}
    uint16_t GetFrequency() const {return frequency;}

private:
    bool pwmRunning;
    Pin pin;
    int chan;

    uint16_t frequency;
    volatile uint32_t period;
    volatile uint32_t onTime;
    };



#endif
