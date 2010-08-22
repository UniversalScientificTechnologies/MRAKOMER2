//******** Mrakomer2 - stepper motor control *****************
#define VERSION "3.0"  // Special version for BART
#define ID "$Id$"
//************************************************************

#include "irmrak.h"
#include <string.h>

char VER[4]=VERSION;
char REV[50]=ID;

#bit CREN = 0x18.4      // USART registers
#bit SPEN = 0x18.7
#bit OERR = 0x18.1
#bit FERR = 0x18.2

#define HALL   PIN_A4      // Hallova sonda pro zjisteni natoceni dolu
// vykonovy FET je na RB3 (vystup PWM)

int port;   // stav brany B pro krokove motory
int j;      // pro synchronisaci fazi
unsigned int8  uhel;   // prijaty znak
unsigned int8  i;      // pro cyklus for

// --- Kroky krokoveho motoru ---
void krok(int n)
{
   while((n--)>0)
   {
      if (1==(j&1)) {port^=0b11000000;} else {port^=0b00110000;};
      output_B(port);
      delay_ms(20);     // Nutno nastavit podle dynamiky systemu.
      j++;
   }
}

// --- Dojet dolu magnetem na cidlo ---
void dolu()
{
   unsigned int8 err;   // pocitadlo pro zjisteni zaseknuti otaceni

   err=0;
   while(!input(HALL))  // otoceni trubky dolu az na hall
   {
      krok(1);
      err++;
      if(40==err)       // do 40-ti kroku by se melo podarit otocit dolu
      {
         output_B(0);   // vypnuti motoru
         printf("E");   // Hlasime chybu
         reset_cpu();
      }
   };
   delay_ms(500);    // cas na ustaleni trubky
   output_B(0);      // vypnuti motoru
}

// --- Najeti na vychozi polohu dole ---
void nula()
{
   port=0b10010000;  // vychozi nastaveni fazi pro rizeni motoru
   output_B(port);
   j=1;              // smer dolu
   delay_ms(500);
}


//------------------------------------------------
void main()
{
   setup_adc_ports(NO_ANALOGS|VSS_VDD);
   setup_adc(ADC_OFF);
   setup_spi(SPI_SS_DISABLED);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1);
   setup_timer_1(T1_DISABLED);
   setup_timer_2(T2_DISABLED,0,1);
   setup_ccp1(CCP_OFF);
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
   setup_oscillator(OSC_8MHZ|OSC_INTRC);

   output_B(0);               // vypnuti motoru a topeni
   set_tris_B(0b00000111);    // faze a topeni jako vystupy  

   nula();
   dolu();           // otoc trubku do vychozi pozice dolu
   
   while(true)
   {
      CREN=0; CREN=1;               // Reinitialise USART

      while(!kbhit())
      {
         if (!input(HALL)) // znovuotoceni trubky dolu, kdyby ji vitr otocil
         {
            dolu();
         }
      }; // pokracuj dal, kdyz prisel po RS232 znak


      uhel=getc();   // prijmi znak
      if ('m'==uhel) // standardni mereni ve trech polohach
      {
         nula();
         j++;    // reverz, nahoru

         krok(18);
         printf("A");   // mereni teploty 45° nad obzorem
         delay_ms(300);
         krok(7);
         printf("B");   // mereni teploty v zenitu
         delay_ms(300);
         krok(7);
         printf("C");  // mereni teploty 45° nad obzorem na druhou stranu
         delay_ms(300);

         j++;     // reverz
         dolu();
         printf("G");  // mereni teploty Zeme (<G>round)

         continue;
      }
      
      if ('i'==uhel) {printf("I"); continue;} // Predani prikazu pro Info
      if ('h'==uhel) {printf("H"); continue;} // Predani prikazu pro Topeni  
      if ('f'==uhel) {printf("F"); continue;} // Predani prikazu pro vypnuti topeni  
      if ('x'==uhel)                          // Zjisteni verze FW
      {
         printf("Mrakomer - Motor  V%s (C) 2006 KAKL\n\r", VER);
         printf("%s\r\n", REV);
      }

      if ((uhel>='0') && (uhel<='@')) // mereni v pozadovanem uhlu [0..;]=(0..11)
      {
         uhel-='0';
      };

      if(uhel>11) continue;   // ochrana, abysme neukroutili draty

      nula();
      j++;     // reverz, nahoru

      krok(12);      // odkrokuj do roviny
      for(i=0; i<uhel; i++) // dale odkrokuj podle pozadovaneho uhlu
      {
         krok(2);
      };
      printf("S");
      delay_ms(300);

      j++;     // reverz
      dolu();
      printf("G");  // mereni teploty Zeme (<G>round)
   }
}

