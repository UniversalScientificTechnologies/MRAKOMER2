/**** IR Mrakomer - special version for BART ****/
#define VERSION "2.2"
#define ID "$Id: irmrak4.c 1306 2009-01-17 12:25:40Z kakl $"

#include "irmrak4.h"

#bit CREN = 0x18.4      // USART registers
#bit SPEN = 0x18.7
#bit OERR = 0x18.1
#bit FERR = 0x18.2

#include <string.h>

#CASE    // Case sensitive compiler

#define  MAXHEAT     20       // Doba po kterou se topi v [s]
#define  HEATING     PIN_A2   // Heating for defrosting

char  VER[4]=VERSION;   // Buffer for concatenate of a version string

int8  heat;    // Status variables


void delay(int16 cycles)         // Vlastni pauza, zbylo to z ovladani fototranzistoru z MM4
{
   int16 i;

   for(i=0; i<cycles; i++) {delay_us(100);}
}

void welcome(void)               // Welcome message
{
   char  REV[50]=ID;       // Buffer for concatenate of a version string

   if (REV[strlen(REV)-1]=='$') REV[strlen(REV)-1]=0;
   printf("\n\r\n\r# Mrakomer %s (C) 2007-2010 KAKL\n\r",VER);   // Welcome message
   printf("#%s\n\r",&REV[4]);
   printf("#\n\r");
   printf("# h    - Switch On Heating for 20s.\n\r");
   printf("# f    - Freezing. Switch Off Heating.\n\r");
   printf("# i    - Print this Information.\n\r");
   printf("# 0..9 - Single measure at given angle.\n\r");
   printf("# m    - Measure at three space points.\n\r");
   printf("#\n\r");
   printf("$<Angle> <Ambient Temperature> <Space Temperature> ... <H> <Heating>");
   printf("\n\r\n\r");
//---WDT
   restart_wdt();
}


#include "smb.c"                 // System Management Bus driver


// Read sensor's RAM
// Returns temperature in °K
int16 ReadTemp(int8 addr, int8 select)
{
   unsigned char arr[6];         // Buffer for the sent bytes
   int8 crc;                     // Readed CRC
   int16 temp;                   // Readed temperature

   addr<<=1;

   SMB_STOP_bit();               //If slave send NACK stop comunication
   SMB_START_bit();              //Start condition
   SMB_TX_byte(addr);
   SMB_TX_byte(RAM_Access|select);
   SMB_START_bit();              //Repeated Start condition
   SMB_TX_byte(addr);
   arr[2]=SMB_RX_byte(ACK);      //Read low data,master must send ACK
   arr[1]=SMB_RX_byte(ACK);      //Read high data,master must send ACK
   temp=make16(arr[1],arr[2]);
   crc=SMB_RX_byte(NACK);        //Read PEC byte, master must send NACK
   SMB_STOP_bit();               //Stop condition

   arr[5]=addr;
   arr[4]=RAM_Access|select;
   arr[3]=addr;
   arr[0]=0;
   if (crc != PEC_calculation(arr)) temp=0; // Calculate and check CRC

   return temp;
}


/*-------------------------------- MAIN --------------------------------------*/
void main()
{
   unsigned int16 timer, temp, tempa;
   signed int16 ta, to;

   output_low(HEATING);                 // Heating off

   delay_ms(500);
   restart_wdt();

   heat=0;
   timer=0;
   
   welcome();

   tempa=ReadTemp(SA, RAM_Tamb);       // Dummy read
   temp=ReadTemp(SA, RAM_Tobj1);

   delay_ms(500);

   while(TRUE)    // Main Loop
   {
      char  ch;      

//---WDT
      restart_wdt();

      if(kbhit()) 
      {
         ch=getc();                    // Precti znak od radice motoru
         CREN=0; CREN=1;               // Reinitialise USART

         tempa=ReadTemp(SA, RAM_Tamb);       // Read temperatures from sensor
         temp=ReadTemp(SA, RAM_Tobj1);
   
         ta=tempa*2-27315;    // °K -> °C
         to=temp*2-27315;

         switch (ch)
         {
            case 'H':
               heat=MAXHEAT;           // Need heating
               break;

            case 'F':
               heat=0;                 // Freeze
               break;

            case 'I':
               welcome();              // Information about version, etc...
               break;                  
         }
         printf("%c %Ld %Ld ", ch, ta, to);
         if (('A'!=ch)&&('B'!=ch)&&('C'!=ch)&&('S'!=ch)) printf("H %u\r\n", heat);  // Vzdycky se konci natocenim na Ground
      }
      delay_ms(1);
      if (timer>0) {timer--;} else {timer=1000;}   
      if (heat>0) {if(1000==timer) heat--; output_high(HEATING);} else { output_low(HEATING); }
   }
}

