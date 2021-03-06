
#include <EtherCard.h>
#include <IPAddress.h>
#include <SoftwareSerial.h>

#include "Wire.h"
#include "BlinkM_funcs.h"

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer

// ethernet interface ip address
static byte mac[] = { 0xBE, 0x78, 0x88, 0x03, 0xF3, 0xAC };
static byte remote[] = { 255, 255, 255, 255 };

#define localPort 3425
#define blinkm_addr 0x00

SoftwareSerial* serial;

#define EOS  0x00
#define SOH  0x01
#define STX  0x02
#define ETX  0x03
#define EOT  0x04
#define ENQ  0x05
#define ACK  0x06
#define BEL  0x07
#define LF   0x0A
#define CR   0x0D

#define teleinfo__buffer_size 220


/**
PTEC possibles values
- TH.. => Toutes les Heures.
- HC.. => Heures Creuses.
- HP.. => Heures Pleines.
- HN.. => Heures Normales.
- PM.. => Heures de Pointe Mobile.
- HCJB => Heures Creuses Jours Bleus.
- HCJW => Heures Creuses Jours Blancs (White).
- HCJR => Heures Creuses Jours Rouges.
- HPJB => Heures Pleines Jours Bleus.
- HPJW => Heures Pleines Jours Blancs (White).
- HPJR => Heures Pleines Jours Rouges. 
**/
char teleinfo__PTEC[5] = "";
char teleinfo__buffer[teleinfo__buffer_size] = "";

bool teleinfo__read()
{
    unsigned char i = 0;
    unsigned char incommingByte = ETX;
    
    // empty teleinfo buffer
    teleinfo__buffer[0] = 0;

    // Wait next datagram start
    while (incommingByte != STX) {
        // "Start Text" STX (002 h) is the beginning of the frame
        if (serial->available()) incommingByte = serial->read() & 0x7F;
    }
    
    while (incommingByte != ETX) {
        if (serial->available()) {
            incommingByte = serial->read() & 0x7F;
            teleinfo__buffer[i] = incommingByte;
            
            // optional -- Store the PTEC value
            if(i > 8 && 
              teleinfo__buffer[i-8] == 'P' &&
              teleinfo__buffer[i-7] == 'T' &&
              teleinfo__buffer[i-6] == 'E' &&
              teleinfo__buffer[i-5] == 'C' ) {
                memcpy( teleinfo__PTEC, &teleinfo__buffer[i-3], 4 );
                teleinfo__PTEC[4] = '\0';
            }
            
            i++;
        }
        
        // check for buffer overflow
        if (i >= teleinfo__buffer_size) {
            // teleinfo__buffer may be invalid and/or 
            // incomplete and should be ignored
            
            return false;
        }
    }
    
    return true;
}

void setup() {
    // immediately disable watchdog timer so set will not get interrupted

    wdt_disable();

    // I often do serial i/o at startup to allow the user to make config changes of
    // various constants. This is often using fgets which will wait for user input.
    // any such 'slow' activity needs to be completed before enabling the watchdog timer.

    // the following forces a pause before enabling WDT. This gives the IDE a chance to
    // call the bootloader in case something dumb happens during development and the WDT
    // resets the MCU too quickly. Once the code is solid, remove this.

    delay(2L * 1000L);

    BlinkM_beginWithPower();  
    BlinkM_stopScript( blinkm_addr ); 
    BlinkM_off( blinkm_addr );
    
    ether.begin(sizeof Ethernet::buffer, mac, 10);
    ether.staticSetup(ip, gw, gw, subnet);

    if(!ether.dhcpSetup())
    {
       wdt_enable(WDTO_8S);
       wdt_reset();
       BlinkM_playScript( blinkm_addr, 3, 0, 0 );
       // Blink Red until wdt reboot the controller 
       while(true){}
    }

    // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
    // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
    // loop rebooting.
    // The timeouts I'm most likely to use are:
    // WDTO_1S
    // WDTO_2S
    // WDTO_4S
    // WDTO_8S
    wdt_enable(WDTO_8S);
  
    // wait until arp request to gateway is finished
    while (ether.clientWaitingGw())
      ether.packetLoop(ether.packetReceive());

    BlinkM_setRGB(blinkm_addr, 0x00, 0x10, 0x00); // Green
    wdt_reset();
}

char payload[teleinfo__buffer_size] = "";

void loop() {
    ether.packetLoop(ether.packetReceive());
    
    if(!serial) serial = new SoftwareSerial(7, 6);
    serial->begin(1200);
   
    if(teleinfo__read()) {
      
        if(strcmp(teleinfo__PTEC, "HC..") == 0) {
            // heure creuse
            BlinkM_setRGB(blinkm_addr, 0x00, 0x00, 0x10); // blue
        }
        else {
            // Heure pleine
            BlinkM_setRGB(blinkm_addr, 0x10, 0x00, 0x00); // red
        }
        
        payload[0] = 0;
        strcpy(payload, teleinfo__buffer);
        ether.sendUdp(payload, sizeof(payload), 1234, remote, 3425);
        
        wdt_disable();
        delay(10);
        wdt_enable(WDTO_8S);
    }

    wdt_reset();
}
