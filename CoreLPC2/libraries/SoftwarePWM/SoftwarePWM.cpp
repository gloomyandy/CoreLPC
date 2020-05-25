//Author sdavi

#include "SoftwarePWM.h"
#include "SoftwarePWMTimer.h"

extern "C" void debugPrintf(const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));


SoftwarePWM::SoftwarePWM(Pin softPWMPin)
{
    SetFrequency(1); //default to 1Hz
    
    pwmRunning = false;
    pin = softPWMPin;
    pinMode(pin, OUTPUT_LOW);
    chan = -1;
}

void SoftwarePWM::Enable()
{
    debugPrintf("Calling enable\n");
    chan = softwarePWMTimer.enable(pin, period, period);
    debugPrintf("Enable returns %d\n", chan);
    if (chan >= 0)
    {
        pinMode(pin, OUTPUT_LOW);
        pwmRunning = true;
    }
}

void SoftwarePWM::Disable()
{
    debugPrintf("Calling disable chan %d\n", chan);
    if (chan >= 0) 
        softwarePWMTimer.disable(chan);
    pinMode(pin, OUTPUT_LOW);
    pwmRunning = false;
}

//Sets the freqneucy in Hz
void SoftwarePWM::SetFrequency(uint16_t freq)
{
    frequency = freq;
    //find the period in us
    period = 1000000/freq;
    onTime = 0;    
}


void SoftwarePWM::SetDutyCycle(float duty)
{
    uint32_t ot = (uint32_t) ((float)(period * duty));
    if(ot > period) ot = period;
    if (onTime != ot)
    {
        onTime = ot; //update the Duty
        if (chan >= 0) 
        {
            if (onTime == 0)
                softwarePWMTimer.adjustOnOffTime(chan, period, period, 0, 0);
            else if (onTime == period)
                softwarePWMTimer.adjustOnOffTime(chan, period, period, 1, 1);
            else
                softwarePWMTimer.adjustOnOffTime(chan, onTime, period - onTime, 1, 0);
        }
    }
}

void SoftwarePWM::Check()
{

}

