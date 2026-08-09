#ifndef _PIC_H
#define _PIC_H

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC1_OFFSET 0x20  // IRQ0-7   -> IDT vectors 0x20-0x27
#define PIC2_OFFSET 0x28  // IRQ8-15  -> IDT vectors 0x28-0x2F

void pic_remap(void);
void pic_send_eoi(unsigned char irq);
void pic_clear_mask(unsigned char irq);

#endif
