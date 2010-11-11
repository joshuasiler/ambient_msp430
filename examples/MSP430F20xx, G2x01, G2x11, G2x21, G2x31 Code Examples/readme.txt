All F20xx code examples (except SD16 code examples) are directly reusable on the G2x01, G2x11, G2x21, & G2x31 devices with only the change in the header file.
i.e. change from:
#include "msp430x20x2.h" to "msp430g2x01.h"

Please beware that different variants of the G2xx1 devices have different peripheral sets. Therefore certain peripheral examples are only applicable to certain devices, as follows.
ADC10 code examples usable on G2x31 devices
COMP_A+ code examples reusable on G2x11 devices
USI code examples reusable on G2x21 devices


