#ifndef _TIMER_H
#define _TIMER_H

// Programs PIT channel 0 to fire IRQ0 at approximately frequency_hz and
// installs its handler (must be called after isr_install()).
void timer_init(unsigned int frequency_hz);
unsigned int timer_get_ticks(void);

#endif
