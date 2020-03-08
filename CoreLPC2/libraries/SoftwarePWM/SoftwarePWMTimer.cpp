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

//SD:: Code from mbed us_ticker_api.c merged into class for Software PWM Timer

#include "SoftwarePWMTimer.h"
#include "SoftwarePWM.h"


SoftwarePWMTimer softwarePWMTimer;


#define US_TICKER_TIMER      ((LPC_RITIMER_T *)LPC_RITIMER_BASE)
#define US_TICKER_TIMER_IRQn RITIMER_IRQn
static uint32_t postscale;

SoftwarePWMTimer::SoftwarePWMTimer()
{
    head = nullptr;
    Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_RIT); //enable power and clocking
    
    US_TICKER_TIMER->CTRL = RIT_CTRL_INT; // clear interrupt bit, disable timer
    uint32_t PCLK = SystemCoreClock / 4;

    // the RIT timer does not have a prescale register so it will always operate with PCLK ticks
    // to get things into the "standard" range we use a postscale value. Note that this limits the
    // range of event times that can be used to 85 seconds, but this is not typically an issue
    // with how it is used.
    postscale = PCLK / 1000000; // default to 1MHz (1 us ticks)
    US_TICKER_TIMER->CTRL = RIT_CTRL_INT|RIT_CTRL_TEN; // clear interrupt bit, enable timer
    NVIC_EnableIRQ(US_TICKER_TIMER_IRQn);

}

uint32_t SoftwarePWMTimer::us_ticker_read()
{
    return US_TICKER_TIMER->COUNTER/postscale;
}


inline void SoftwarePWMTimer::us_ticker_set_interrupt(ticker_event_t *obj)
{
    // set match value

    // There is a chance that the timestamp may be in the past (or is about to be).
    // This can be caused by other higher priority interrupts delaying the actual
    // setting of the counters after the timestamp has been calculated/checked.
    // This can be very bad as it will mean that the next interrupt will not fire
    // until the timer wraps. We check for this situation and arrange for such
    // timestamps to be moved to the "very near" future. Ensuring that they will
    // fire.
    while (true)
    {
        // Clear any existing match
        if (US_TICKER_TIMER->CTRL & RIT_CTRL_INT)
            US_TICKER_TIMER->CTRL |= RIT_CTRL_INT;
        // set match value
        US_TICKER_TIMER->COMPVAL = obj->timestamp;
        // Is the time we have just set still in the future?
        if ((int)(obj->timestamp - US_TICKER_TIMER->COUNTER) > 0) break;
        // Move timestamp forwards and try again
        obj->timestamp = US_TICKER_TIMER->COUNTER + postscale;
    }
    NVIC_EnableIRQ(US_TICKER_TIMER_IRQn);
}

inline void SoftwarePWMTimer::us_ticker_disable_interrupt(void)
{
    NVIC_DisableIRQ(US_TICKER_TIMER_IRQn);
}

inline void SoftwarePWMTimer::us_ticker_clear_interrupt(void)
{
    if (US_TICKER_TIMER->CTRL & RIT_CTRL_INT)
        US_TICKER_TIMER->CTRL |= RIT_CTRL_INT;
}

extern "C" void RIT_IRQHandler(void) __attribute__ ((hot));
void RIT_IRQHandler(void)
{
    softwarePWMTimer.Interrupt();
}

void SoftwarePWMTimer::Interrupt()
{
    us_ticker_clear_interrupt();

    /* Go through all the pending TimerEvents */
    while (1)
    {
        if (head == NULL)
        {
            // There are no more TimerEvents left, so disable matches.
            us_ticker_disable_interrupt();
            return;
        }

        if ((int)(head->timestamp - US_TICKER_TIMER->COUNTER) <= 0)
        {
            // This event was in the past:
            //      point to the following one and execute its handler
            ticker_event_t *p = head;
            head = head->next;

            p->PWM->Interrupt(); // NOTE: the handler can set new events
        }
        else
        {
            // This event and the following ones in the list are in the future:
            //      set it as next interrupt and return
            us_ticker_set_interrupt(head);
            return;
        }
    }
}






void SoftwarePWMTimer::us_ticker_insert_event(ticker_event_t *obj, unsigned int timestamp, SoftwarePWM *softPWMObject)
{
    /* disable interrupts for the duration of the function */
    const irqflags_t flags = cpu_irq_save();

    // Scale the uS time into our clock units
    timestamp *= postscale;

    // initialise our data
    obj->timestamp = timestamp;
    obj->PWM = softPWMObject;

    /* Go through the list until we either reach the end, or find
       an element this should come before (which is possibly the
       head). */
    ticker_event_t *prev = NULL, *p = head;
    while (p != NULL)
    {
        /* check if we come before p */
        if ((int)(timestamp - p->timestamp) <= 0)
        {
            break;
        }
        /* go to the next element */
        prev = p;
        p = p->next;
    }
    /* if prev is NULL we're at the head */
    if (prev == NULL)
    {
        head = obj;
        us_ticker_set_interrupt(obj);
    }
    else
    {
        prev->next = obj;
    }
    /* if we're at the end p will be NULL, which is correct */
    obj->next = p;

    cpu_irq_restore(flags);
    
}

void SoftwarePWMTimer::us_ticker_remove_event(ticker_event_t *obj)
{
    const irqflags_t flags = cpu_irq_save();

    // remove this object from the list
    if (head == obj)
    {
        // first in the list, so just drop me
        head = obj->next;
        if (obj->next != NULL)
        {
            us_ticker_set_interrupt(head);
        }
    }
    else
    {
        // find the object before me, then drop me
        ticker_event_t* p = head;
        while (p != NULL)
        {
            if (p->next == obj)
            {
                p->next = obj->next;
                break;
            }
            p = p->next;
        }
    }

    cpu_irq_restore(flags);
}