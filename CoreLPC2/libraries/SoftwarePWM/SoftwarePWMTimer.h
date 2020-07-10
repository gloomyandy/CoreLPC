//Author: sdavi


#ifndef SOFTWAREPWMTIMER_H
#define SOFTWAREPWMTIMER_H

class SoftwarePWM; //Fwd decl

class SoftwarePWMTimer
{
public:
    SoftwarePWMTimer() noexcept;
    void EnableChannel(SoftwarePWM *sChannel) noexcept;
    void DisableChannel(SoftwarePWM *sChannel) noexcept;
    void Interrupt() noexcept;

private:

    
};

extern SoftwarePWMTimer softwarePWMTimer;

#ifdef LPC_DEBUG
extern uint32_t pwmInts;
extern uint32_t pwmCalls;
extern uint32_t pwmMinTime;
extern uint32_t pwmMaxTime;
extern uint32_t pwmAdjust;
#endif

#endif
