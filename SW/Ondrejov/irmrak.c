//******** Mrakomer ******************************************
#define VERSION "2.1"
#define ID "$Id: irmrak.c 124 2006-10-24 15:47:15Z kakl $"
//************************************************************

#include "irmrak.h"
#include <string.h>

#use rs232(baud=9600,parity=N,xmit=PIN_A6,rcv=PIN_A7,bits=8)

char VER[4]=VERSION;
char REV[50]=ID;

#define TRESHOLD  8  // nebo 9
                     // nad_diodou=H je + teplota
                     // pod_diodou=L je - teplota

#define MINUS  !C1OUT      // je kladny impuls z IR teplomeru
#define PLUS   C2OUT       // je zaporny impuls z IR teplomeru
#define HALL   PIN_A4      // Hallova sonda pro zjisteni natoceni dolu
// topeni je na RB3 (vystup PWM)

#define MAX_TEMP 10000

int port;   // stav brany B pro krokove motory
int j;      // pro synchronisaci fazi
unsigned int8  uhel;   // pocitadlo prodlevy mezi merenimi
unsigned int8  i;      // pro cyklus for
unsigned int16 nn;     // pocitadlo mereni
unsigned int32 timer;  // casovac pro topeni
unsigned int8  topit;  // na jaky vykon se ma topit?

unsigned int16 teplota;    // prectena teplota
int1 sign;                 // nad nulou / pod nulou

// --- Jeden krok krokoveho motoru ---
void krok(int n)
{
   while((n--)>0)
   {
      if (1==(j&1)) {port^=0b11000000;} else {port^=0b00110000;};
      output_B(port);
      delay_ms(50);
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
         printf("Error movement.\n\r");
         err=0;
      }
   };
   delay_ms(700);    // cas na ustaleni trubky
   output_B(0);      // vypnuti motoru
}

// --- Najeti na vychozi polohu dole ---
void nula()
{
   int n;

   for (n=0; n<7; n++)
   {
      if (!input(HALL)) return;
      krok(1);
   }
}

// --- Precti teplotu z IR teplomeru ---
void prevod()
{
   unsigned int16 t;       // cas
   unsigned int16 tt;

   t=0;
   while(!PLUS && !MINUS)
   {
      t++;
      if(t>65000)
      {
         printf("Error thermometer.\n\r");
         teplota=0;
         sign=true;
         return;
      }
   }; // ceka se na 0

   teplota=0;
   sign=false;
   for(t=0;t<=MAX_TEMP;t++)
   {
      if(PLUS || MINUS)     // ceka se na + nebo -
      {
         if(MINUS) sign=true;
         for(tt=1;tt<=MAX_TEMP;tt++)
         {
            if(!PLUS && !MINUS) {teplota=tt; break;};  // ceka se na 0
         }
         break;
      }
   }
   teplota=teplota*19/100;  // 1 stupen celsia je asi 10us
}

// --- Prevod a vystup jedne hodnoty ---
void vystup()
{
      delay_ms(200);    // pockej na ustaleni napeti po krokovani
      printf(" ");
      prevod();         // precti teplotu z teplomeru
      if(sign)
        printf("-%Lu", teplota);
      else
        printf("%Lu", teplota);
}

//------------------------------------------------
void main()
{
   setup_oscillator(OSC_4MHZ|OSC_INTRC);     // 4 MHz interni RC oscilator

   setup_adc_ports(NO_ANALOGS|VSS_VDD);
   setup_adc(ADC_OFF);
   setup_spi(FALSE);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1);  // Casovac pro PWM
   setup_timer_1(T1_DISABLED);
   setup_ccp1(CCP_OFF);
   setup_comparator(A0_VR_A1_VR);   // inicializace komparatoru
   setup_vref(VREF_HIGH|TRESHOLD);  // 32 kroku od 0.25 do 0.75 Vdd

   // nastav PWM pro topeni
   set_pwm1_duty(0);       // Spust PWM, ale zatim s trvalou 0 na vystupu
   setup_ccp1(CCP_PWM);
   setup_timer_2(T2_DIV_BY_16,100,1);  // perioda

   output_B(0);               // vypnuti motoru
   set_tris_B(0b00000111);    // faze a topeni jako vystup

   delay_ms(1000);
   printf("Mrakomer V%s (C) 2006 KAKL\n\r", VER);
   printf("%s\n\r", REV);

   topit=0;          // na zacatku netopime

   while(true)
   {
      port=0b01010000;  // vychozi nastaveni fazi pro rizeni motoru
      j=0;              // smer dolu
      dolu();           // otoc trubku do vychozi pozice dolu

      while(!kbhit())
      {
         timer--;
         if (0==timer)         // casovac, aby se marakomer neupek
         {
            topit=0;
            set_pwm1_duty(0);  // zastav topeni
            printf("H %u\n\r", topit);
         };

         if (!input(HALL)) // znovuotoceni trubky dolu, kdyby ji vitr otocil
         {
            set_pwm1_duty(0);      // zastav topeni, aby byl odber do 1A
            dolu();
            set_pwm1_duty(topit);  // spust topeni
         }
      }; // pokracuj dal, kdyz prisel po RS232 znak


      uhel=getc();   // prijmi znak
      if ((uhel>='a') && (uhel<='k')) // nastaveni topeni [a..k]=(0..100%)
      {
         topit=uhel-'a';
         topit*=10;
         timer=500000;      // cca 11s

         // ochrana proti upeceni
         prevod();
         if(sign)
         {
            if ((teplota <= 5) && (topit > 60)) topit=0; // do -5°C se da topit maximalne na 60%
            printf("H %u;G -%Lu\n\r", topit, teplota); // zobraz hodnotu topeni (<H>eating)
         }
         else
         {
            if (teplota > 10) topit=0; // kdyz je vic jak +10°C, tak netopit
            if (topit > 40) topit=0; // pokud nemrzne, tak se neda topit vic jak na 40%
            printf("H %u;G %Lu\n\r", topit, teplota); // zobraz hodnotu topeni (<H>eating)
         }
         set_pwm1_duty(topit);      // spust topeni
         continue;
      };


      if ('m'==uhel) // standardni mereni ve trech polohach
      {
         prevod();     //jeden prevod na prazdno pro synchnonisaci

         j++;    // reverz, nahoru
         nula();

         printf("G");  // mereni teploty Zeme (<G>round)
         vystup();

         krok(15);
         printf(";S45");   // mereni teploty 45° nad obzorem
         vystup();
         krok(7);
         printf(";S90");   // mereni teploty v zenitu
         vystup();
         krok(7);
         printf(";S135");  // mereni teploty 45° nad obzorem na druhou stranu
         vystup();
         printf("\n\r");

         j++;     // reverz
         dolu();

         continue;
      }


      if ((uhel>='0') && (uhel<='@')) // mereni v pozadovanem uhlu [0..;]=(0..11)
      {
         uhel-='0';
      };

      if(uhel>11) continue;   // ochrana, abysme neukroutili draty

      printf("A %u;", uhel);  // zobraz pozadovany uhel (<A>ngle)

      prevod();               // jeden prevod na prazdno pro synchnonisaci

      j++;     // reverz, nahoru
      nula();

      printf("G");  // mereni teploty Zeme (<G>round)
      vystup();

      printf(";S");  // mereni teploty pozadovanym smerem do vesmiru (<S>pace)
      krok(12);      // odkrokuj do roviny
      for(i=0; i<uhel; i++) // dale odkrokuj podle pozadovaneho uhlu
      {
         krok(2);
      };
      vystup();
      printf("\n\r");

      j++;     // reverz
      dolu();
   }
}

