#include <16F88.h>
#device adc=8
#fuses NOWDT,INTRC_IO, NOPUT, MCLR, NOBROWNOUT, NOLVP, NOCPD, NOWRT, NODEBUG, NOPROTECT, NOFCMEN, NOIESO
#use delay(clock=8000000)
#use rs232(baud=2400,parity=N,xmit=PIN_A6,rcv=PIN_A7,bits=8)

