#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "config.h"
#include "uart.h"
#include "capsensor.h"
#include "switch.h"
#include "power.h"

// PIC16F690 Configuration Bit Settings

// 'C' source line config statements

// CONFIG
#pragma config FOSC = INTRCIO   // Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA4/OSC2/CLKOUT pin, I/O function on RA5/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled and can be enabled by SWDTEN bit of the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR pin function is I/O
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Selection bits (BOR enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal External Switchover mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is enabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

/**
 * JP1-CR: C=column R=row where 12v=A7, Vdd=B1, Vss=B2 and so on
 * In brackets [behavior] as observed in the original firmware
 * RF = RF connector on main board
 * J1 = J1 connector on PIC board
 * 
 * 
 * 
 */

void main(void) {
    // Early inits (before osc is settled)
    switch_preinit();
    power_preinit();

    // Tris config (1=in/analog, 0=out)
    
    TRISA       = 0b11111101;
    // RA0 (19) I   -------1 <--> JP1-B7, ??? [4 ms high-Z pulse on start, then 0v]
    // RA1 (18) O   ------0- ---> RELAY1-S
    // RA2 (17) I   -----1-- T0CKI <--- RC4 (6) C2OUT
    // RA3 ( 4) I   ----1--- MCLR only [high-Z]
    // RA4 ( 3) I   ---1---- <--- AC POWER HEARTBEAT <--- JP1-A3
    // RA5 ( 2) I   --1----- <--> JP1-A1 <--> R23 (unpopulated), ??? [3v when red, 0v when blue]
    // unimpl   x   xx------
    
    TRISB       = 0b10111111;
    // unimpl   x   ----xxxx
    // RB4 (13) I   ---1---- <--> JP1-B3 <---> RF5, O if soft-uart TX
    // RB5 (12) I   --1----- USART RX
    // RB6 (11) O   -0------ ---> LED1 (0=blue, 1=red)
    // RB7 (10) I   1------- USART TX <--> JP1-A2 <--> R24 (unpopulated). Yes, must be input
    
    TRISC       = 0b00101011;
    // RC0 (16) I   -------1 ---> BUZZER (if any) [4 ms high-Z pulse on start, then 0v]
    // RC1 (15) I   ------1- C12IN1- (negative feedback of C2), CAP READ 2 (2-gang only)
    // RC2 (14) O   -----0-- ---> RELAY2-S (2-gang only)
    // RC3 ( 7) I   ----1--- C12IN3- (negative feedback of C2), CAP READ 1
    // RC4 ( 6) O   ---0---- C2OUT (uses C2 as a astable multivibrator)
    // RC5 ( 5) O   --0----- ---> LED2 (0=blue, 1=red) (2-gang only)
    // RC6 ( 8) O   -1------ ---> RELAY2-R (2-gang only)
    // RC7 ( 9) O   0------- ---> RELAY1-R

    // All pins digital
    ANSEL       = 0b00000000;
    ANSELH      = 0b00000000;

    // Initial pin values
    PORTA       = 0b00000000;
    PORTB       = 0b00000000;
    PORTC       = 0b00000000;
    
    // Timer 1 ON (1 us resolution at Fosc = 4 MHz)
    TMR1    = 0;
    TMR1ON  = 1;
    
    // Wait until osc is stable
    while (HTS == 0) ;

    switch_init();
    uart_init();
    capsensor_init();

    uint16_t t0 = 0;
    
    for (;;) {
#ifdef ACCEPT_CMDS_VIA_UART
        if (uart_data_ready()) {
            uint8_t c = uart_read();
            printf("\n%c RECV\r\n", c);
            if (c == 'i') switch_on();
            if (c == 'o') switch_off();
            if (c == ' ') switch_toggle();
        }
#endif        
       
        if (power_read() == POWER_OUTAGE) {
            // Switch off only if we're on so as not to energize the coil
            // pointlessly, which would drain the cap and cause voltage
            // drops everywhere.
            if (switch_status == SWITCH_ON) {
                switch_off();
            }
        }

        if (TMR1 - t0 > TIME_BETWEEN_READS) {
            t0 += TIME_BETWEEN_READS;
            // Keep reading the sensors even after a power outage for
            // debug purposes.
            if (capsensor_is_button_pressed() && power_status == POWER_OK) {
                switch_toggle();
            }
#ifdef DEBUG
            printf("%u,%u,%u,%u,%u,%u\r\n", 
                    capsensor_rolling_avg, 
                    capsensor_frozen_avg, 
                    capsensor_freq,
                    capsensor_status,
                    power_status,
                    switch_status);
#endif    
        }
    }
}
