/* mbed Microcontroller Library
* Copyright (c) 2006-2013 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
* This version rewritten by gloomyandy to simplify and improve performance.
* The interrupt handler now operates in a more predictable way with the time needed
* being linearly related to the number of active channels. Care has been taken to
* try to minimize the number of interrupts needed by allowing channels that have period
* this is a multiple of other channels to be synchronized. The following timing offers
* a comparison to previous versions. A simple test that involved running a simulation
* of the well known 3DBenchy was run with nozzle and bed heaters enabled. Two cooling
* fans were also active. Three versions are compared. The original implementation (1),
* an "improved" version of this code (2) and the current version provided here (3).
* 
* Version     no of interrupts  no of pulses    min time   max time
* 1           1712539           2363134         13uS       45uS
* 2           1708399           2283163         9uS        24uS
* 3           1079673           2159908         3uS         6uS 
*
* The slightly lower no of pulses required by the different versions is due to the overall
* elapsed time being shorter (approx 19mins, 18mins and 17mins).
*/

#include "SoftwarePWMTimer.h"
#include "SoftwarePWM.h"

#ifdef LPC_DEBUG
#include "MessageType.h"
#include "Platform.h"
#include "RepRap.h"
uint32_t pwmInts = 0;
uint32_t pwmCalls = 0;
uint32_t pwmMinTime = 0xffffffff;
uint32_t pwmMaxTime = 0;
uint32_t pwmAdjust = 0;
#endif

// Minimum period between interrupts - in microseconds (to prevent starving other tasks)
static constexpr uint32_t MinimumInterruptDeltaUS = 10;

static constexpr int MaxPWMPins = 8;

typedef struct {
    uint32_t nextEvent;
    uint32_t state;
    uint32_t onOffBuffer;
    Pin pin;
    uint32_t onOffTimes[2][2];
    uint32_t onOffVals[2];
    bool newTimes;
    bool enabled;
} PWMState;

static PWMState States[MaxPWMPins];
static int32_t startActive = -1;
static int32_t endActive = -1;
static uint32_t ticksPerMicrosecond;
static uint32_t minimumTicks;



SoftwarePWMTimer softwarePWMTimer;

SoftwarePWMTimer::SoftwarePWMTimer()
{

    for(int i = 0; i < MaxPWMPins; i++)
        States[i].enabled = false;
    ticksPerMicrosecond = Chip_Clock_GetPeripheralClockRate(SYSCTL_PCLK_RIT)/1000000;
    minimumTicks = ticksPerMicrosecond*MinimumInterruptDeltaUS;

    //Setup RIT
    Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_RIT); //enable power and clocking
    LPC_RITIMER->MASK = 0;
    LPC_RITIMER->COUNTER = 0;
    LPC_RITIMER->CTRL = RIT_CTRL_INT | RIT_CTRL_ENBR | RIT_CTRL_TEN;
}

static void updateActive()
{
    int32_t first = -1;
    int32_t last = -1;
    for(int i = 0; i < MaxPWMPins; i++)
        if (States[i].enabled)
        {
            last = i;
            if (first < 0) first = i;
        }
    if (last >= 0)
    {
        last += 1;
        // If timer not currently running restart it
        if (endActive < 0)
        {
            LPC_RITIMER->COMPVAL = LPC_RITIMER->COUNTER + minimumTicks; 
            NVIC_EnableIRQ(RITIMER_IRQn); 
        }
    }
    else
        NVIC_DisableIRQ(RITIMER_IRQn);
    startActive = first;
    endActive = last;
}

void syncAll()
{
    // briefy pause and then restart all active channels so that they are in sync.
    // this minimises the number of interrupts we need, to service the channels.
    LPC_RITIMER->CTRL &= ~RIT_CTRL_TEN; //Stop the timer
    // set all active channels so that on the next event they will all move to state 0
    for(int i = 0; i < MaxPWMPins; i++)
        if (States[i].enabled)
        {
            States[i].state = 1;
            States[i].nextEvent = LPC_RITIMER->COUNTER;
        }
    // Force an event
    LPC_RITIMER->COMPVAL = LPC_RITIMER->COUNTER + minimumTicks; 
    LPC_RITIMER->CTRL |= RIT_CTRL_TEN; // start the timer
} 

void SoftwarePWMTimer::disable(int chan)
{
    States[chan].enabled = false;
    updateActive();
}

int SoftwarePWMTimer::enable(Pin pin, uint32_t onTime, uint32_t offTime)
{
    // find a free slot
    for(int i = 0; i < MaxPWMPins; i++)
        if (!States[i].enabled)
        {
            PWMState& s = States[i];
            s.pin = pin;
            s.onOffTimes[0][0] = offTime*ticksPerMicrosecond;
            s.onOffTimes[0][1] = onTime*ticksPerMicrosecond;
            s.onOffVals[0] = s.onOffVals[1] = 0;
            s.onOffBuffer = 0;
            s.state = 0;
            pinMode(pin, OUTPUT_LOW);
            s.nextEvent = LPC_RITIMER->COUNTER + offTime;
            s.newTimes = false;
            s.enabled = true;
            updateActive();  
            syncAll();        
            return i;
        }
    return -1;
}

void SoftwarePWMTimer::adjustOnOffTime(int chan, uint32_t onTime, uint32_t offTime, uint32_t onVal, uint32_t offVal)
{
    PWMState& s = States[chan];
    uint32_t buffer = s.onOffBuffer ^ 1;
    s.onOffTimes[buffer][0] = offTime*ticksPerMicrosecond;
    s.onOffTimes[buffer][1] = onTime*ticksPerMicrosecond;
    s.onOffVals[0] = offVal;
    s.onOffVals[1] = onVal;
    s.newTimes = true;
}

extern "C" void RIT_IRQHandler(void) __attribute__ ((hot));
void RIT_IRQHandler(void)
{ 
#ifdef LPC_DEBUG
    pwmInts++;
    const uint32_t startTime = LPC_TIMER0->TC;
#endif 
    LPC_RITIMER->CTRL |= RIT_CTRL_INT; // Clear Interrupt
    uint32_t next = 0x7fffffff;
    uint32_t now = LPC_RITIMER->COUNTER;
    for(int i = startActive; i < endActive; i++)
        if (States[i].enabled)
        {
            PWMState& s = States[i];
            int32_t delta = (s.nextEvent - now);
            if (delta <= 0)
            {
                // time has expired, move to next state
                s.state ^= 1;
                const uint32_t newState = s.state;
                GPIO_PinWrite(s.pin, s.onOffVals[newState]);
                // do we need to switch to a new set of timing parameters?
                if (s.newTimes && newState == 0)
                {
                    s.onOffBuffer ^= 1;
                    s.newTimes = false;
                }
                // adjust next time by any drift to keep things in sync
                delta += s.onOffTimes[s.onOffBuffer][newState];
                // don't allow correction to go too far!
                if (delta < 0)
                    delta = s.onOffTimes[s.onOffBuffer][newState];
                s.nextEvent = now + delta;
#ifdef LPC_DEBUG
                pwmCalls++;
#endif
            }
            // at this point delta >= 0
            if ((uint32_t)delta < next)
                next = (uint32_t)delta;
        }
    // setup the timer for the nearest event time
    next = now + next;
    const uint32_t minTime = LPC_RITIMER->COUNTER + minimumTicks;
    if ((int)(next - minTime) <= 0)
    {
        next = minTime;
#ifdef LPC_DEBUG
        pwmAdjust++;
#endif
    }
    LPC_RITIMER->COMPVAL = next;

#ifdef LPC_DEBUG
    const uint32_t dt = LPC_TIMER0->TC - startTime;
    if (dt < pwmMinTime)
        pwmMinTime = dt;
    else if (dt > pwmMaxTime)
        pwmMaxTime = dt;
#endif
}

#ifdef LPC_DEBUG
void SoftwarePWMTimer::Diagnostics(MessageType mtype)
{
    reprap.GetPlatform().MessageF(mtype, "Interrupts: %u; Calls %u; fastest: %uuS; slowest %uuS adjusted %u\n", (unsigned)pwmInts, (unsigned)pwmCalls, (unsigned)pwmMinTime, (unsigned)pwmMaxTime, (unsigned)pwmAdjust);
    pwmMinTime = UINT32_MAX;
    pwmMaxTime = 0;
    pwmInts = 0;
    pwmCalls = 0;
    pwmAdjust = 0;

    reprap.GetPlatform().MessageF(mtype, "PWM Channels\n");

    /* disable interrupts for the duration of the function */
    LPC_RITIMER->CTRL &= ~RIT_CTRL_TEN; //Stop the timer
    uint32_t now = LPC_RITIMER->COUNTER;
    for(int i = 0; i < MaxPWMPins; i++)
    {
        if (States[i].enabled)
            reprap.GetPlatform().MessageF(mtype, "state %d next %u on %u off %u pin %d.%d\n", i, States[i].nextEvent - now, States[i].onOffTimes[States[i].onOffBuffer][1], States[i].onOffTimes[States[i].onOffBuffer][0], (States[i].pin >> 5), (States[i].pin & 0x1f) );
    }
    LPC_RITIMER->CTRL |= RIT_CTRL_TEN;
}
#endif