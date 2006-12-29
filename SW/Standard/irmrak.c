//******** Mrakomer **********
#define VERSION "2.0"
#define ID "$Id: irmrak.c 124 2006-10-24 15:47:15Z kakl $"
//****************************

#include "irmrak.h"
#include <string.h>

#use rs232(baud=9600,parity=N,xmit=PIN_A6,bits=8)

#define  TXo PIN_A3           // To the transmitter modulator
#include "AX25.c"             // Podprogram pro prenos telemetrie
char AXstring[50];            // Buffer pro prenos telemetrie
char G[5];
char S45[5];
char S90[5];
char S135[5];
char VER[4]=VERSION;

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
unsigned int16 n, prodleva;   // pocitadlo prodlevy mezi merenimi
unsigned int16 nn;            // pocitadlo mereni
unsigned int8  topit;         // na jaky vykon se ma topit?

unsigned int16 teplota;    // prectena teplota
int1 sign;                 // nad nulou / pod nulou

void krok(int n)
{
   while((n--)>0)
   {
      if (1==(j&1)) {port^=0b11000000;} else {port^=0b00110000;};
      output_B(port);
      delay_ms(40);
      j++;
   }
}

void dolu()
{
   unsigned int8 err;            // pocitadlo pro zjisteni zaseknuti otaceni

   err=0;
   while(!input(HALL))  // otoceni trubky dolu az na hall
   {
      krok(1);
      err++;
      if(40==err)       // do 40-ti kroku by se melo podarit otocit dolu
      {
         output_B(0);      // vypnuti motoru
         sprintf(AXstring,"Error movement.");
         SendPacket(&AXstring[0]);
         printf("%s\n\r", AXstring);
         err=0;
      }
   };
   delay_ms(700);    // cas na ustaleni trubky
   output_B(0);      // vypnuti motoru
}

void prevod()
{
   unsigned int16 t; // cas
   unsigned int16 tt;

   t=0;
   while(!PLUS && !MINUS)
   {
      t++;
      if(t>65000)
      {
         sprintf(AXstring,"Error thermometer.");
         SendPacket(&AXstring[0]);
         printf("%s\n\r", AXstring);
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

void main()
{
   unsigned int8 ble;

   setup_oscillator(OSC_4MHZ|OSC_INTRC);     // 4 MHz interni RC oscilator

   setup_adc_ports(NO_ANALOGS|VSS_VDD);
   setup_adc(ADC_OFF);
   setup_spi(FALSE);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1);  // Casovac pro PWM
   setup_timer_1(T1_DISABLED);
   setup_ccp1(CCP_OFF);
   setup_comparator(A0_VR_A1_VR);   // inicializace komparatoru
   setup_vref(VREF_HIGH|TRESHOLD);        // 32 kroku od 0.25 do 0.75 Vdd

   // nastav PWM pro topeni
   set_pwm1_duty(0);       // Spust PWM, ale zatim s trvalou 0 na vystupu
   setup_ccp1(CCP_PWM);
   setup_timer_2(T2_DIV_BY_16,100,1);  // perioda

   output_B(0);               // vypnuti motoru
  
   sprintf(AXstring,"Mrakomer V%s (C) 2006 KAKL", VER);
   SendPacket(&AXstring[0]);
   printf("%s\n\r", AXstring);


   if(input(HALL))   // pokud na zacatku byla trubka dolu,
   {                 // vysilej pro nastaveni prijimace rychle za sebou 10min
      for(n=0; n<(10*60); n++)
      {
         prevod();
         if(sign)
           sprintf(AXstring,"Kalibrace: n %Lu, t -%Lu", n, teplota);
         else
           sprintf(AXstring,"Kalibrace: n %Lu, t %Lu", n, teplota);

         delay_ms(1000);               // 1s
         SendPacket(&AXstring[0]);
         printf("%s\n\r", AXstring);
         if(!input(HALL)) break;       // pokud nekdo hne trubkou, zacni merit
      }
   }

   port=0b01010000;  // vychozi nastaveni fazi pro rizeni motoru
   j=0;              // smer dolu
   dolu();           // otoc trubku do vychozi pozice dolu
   nn=0;             // pocitadlo packetu

   while(true)
   {
      set_tris_B(0b00000111); // faze a topeni jako vystup
      set_pwm1_duty(0);       // netopit
      port=0b01010000;        // vychozi nastaveni fazi pro rizeni motoru
      j=1;                    // smer, nahoru

      delay_ms(1000);         // pockej pro ustaleni napeti po vypnuti topeni
      prevod();               // jeden prevod na prazdno pro synchnonisaci

      prevod();      // zem
      if(sign)
      {
        sprintf(G,"-%Lu", teplota);
        topit=10;
        if (teplota>5) topit=20;
        if (teplota>10) topit=50;
        if (teplota>20) topit=80;
        if (teplota>30) topit=100;
      }
      else
      {
        sprintf(G,"%Lu", teplota);
        topit=0;
      }

      krok(2);          // sjeti z koncaku
      delay_ms(1000);
      krok(17);         // 45 stupnu
      delay_ms(200);
      prevod();
      if(sign)
        sprintf(S45,"-%Lu", teplota);
      else
        sprintf(S45,"%Lu", teplota);

      strcpy(S90,"-");
      if (sign && (teplota>29))
      {
         krok(6);       // 90 stupnu
         delay_ms(300);
         prevod();
         if(sign)
           sprintf(S90,"-%Lu", teplota);
         else
           sprintf(S90,"%Lu", teplota);
      }

      strcpy(S135,"-");
      if (sign && (teplota>29))
      {
         krok(6);       // 135 stupnu
         delay_ms(300);
         prevod();
         if(sign)
           sprintf(S135,"-%Lu", teplota);
         else
           sprintf(S135,"%Lu", teplota);
      }

      j++;           // reverz
      dolu();        // znovu otoc trubku dolu do vychozi pozice

      // odeslani vysledku mereni
      sprintf(AXstring,
         "n %Lu;G %s;S45 %s;S90 %s;S135 %s", nn, G, S45, S90, S135);
      SendPacket(&AXstring[0]);
      printf("%s\n\r", AXstring);

      if (sign && (teplota>29)) // pauza do dalsiho mereni
      {
         prodleva=60; // 1 min
         nn++;
      }
      else
      {
         prodleva=600; // 10 min pri teplote atmosfery vyzsi jak -30 stupnu
         nn+=10;
      }

      if (nn>9999) nn=0;      // pocitadlo mereni modulo 10000

      set_pwm1_duty(topit);      // spust topeni
      for(n=prodleva; n>0; n--)  // cekej na dalsi mereni
      {
         delay_ms(1000);
         if (!input(HALL)) // znovuotoceni trubky dolu, kdyby ji vitr otocil
         {
            set_pwm1_duty(0);      // zastav topeni, aby byl odber do 1A
            dolu();
            set_pwm1_duty(topit);  // spust topeni
         }
      }
   }
}

