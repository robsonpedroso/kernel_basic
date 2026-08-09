#ifndef _RTC_H
#define _RTC_H

// One-shot CMOS RTC read (ports 0x70/0x71). No interrupt, no ongoing drift
// correction -- fine for stamping file create/modify times in a hobby OS,
// not a real clock subsystem. Packs into a classic FAT-style 32-bit
// date/time so callers don't need six separate fields:
//   bits 31-25: year - 1980 (0-127)
//   bits 24-21: month (1-12)
//   bits 20-16: day (1-31)
//   bits 15-11: hour (0-23)
//   bits 10-5:  minute (0-59)
//   bits 4-0:   second / 2 (0-29)
unsigned int rtc_now(void);

#endif
