#include <msp430.h>
#include <stdlib.h>


#define TIMER_SEC_VALUE 200
//#define TOGGLE_10SEC_VALUE TOGGLE_1SEC_VALUE * 10

volatile unsigned int timer_exit = 0;


void Init_UART();
unsigned int ADC_Result[3];                                    // 8-bit ADC conversion result array
unsigned char counter;                                        // Counts all of ADC pins
void Software_Trim();                                        // Software Trim to get the best DCOFTRIM value
#define MCLK_FREQ_MHZ 8                                     // MCLK = 8MHz
#define MAX_Digits 10
void SendADC();                                             // Sending the ADC result via UART
int* DivideByDigit(int ADC_val, int* sizeofDigits);           // Dividing the ADC results into the digits.


int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                                   // Stop WDT

   // Configure UART
    Init_UART();

    // Configure ADC A0~2 pins
    P1SEL0 |= BIT0 + BIT1 + BIT2;
    P1SEL1 |= BIT0 + BIT1 + BIT2;

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Configure ADC
    ADCCTL0 |= ADCSHT_2 | ADCON;                                // 16ADCclks, ADC ON
    ADCCTL1 |= ADCSHP | ADCCONSEQ_3;                 // ADC clock MODCLK, sampling timer, TA1.1B trig.,repeat sequence
    ADCCTL2 &= ~ADCRES;                                         // clear ADCRES in ADCCTL
    ADCCTL2 |= ADCRES_2;                                        // 12-bit conversion results
    ADCMCTL0 |= ADCINCH_2 | ADCSREF_1;                          // A0~2(EoS); Vref=2V
    ADCIE |= ADCIE0;                                            // Enable ADC conv complete interrupt



    __bis_SR_register(SCG0);                      // disable FLL
        CSCTL3 |= SELREF__REFOCLK;               // Set REFO as FLL reference source
        CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1 | DCORSEL_3;// DCOFTRIM=3, DCO Range = 8MHz
        CSCTL2 = FLLD_0 + 243;                  // DCODIV = 8MHz
        __delay_cycles(3);
        __bic_SR_register(SCG0);                // enable FLL
        Software_Trim();                        // Software Trim to get the best DCOFTRIM value

        CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK; // set default REFO(~32768Hz) as ACLK source, ACLK = 32768Hz
                                                 // default DCODIV as MCLK and SMCLK source


    // Configure reference
    PMMCTL0_H = PMMPW_H;                                      // Unlock the PMM registers
    PMMCTL2 |= INTREFEN | REFVSEL_1;                          // Enable internal 2V reference
    __delay_cycles(400);                                      // Delay for reference settling


    TA0CCTL0 |= CCIE;                             // TACCR0 interrupt enabled
    TA0CCR0 = 50000;
    TA0CTL |= TASSEL__SMCLK | MC__UP | ID_3;     // SMCLK, up mode  1Mhz

       __bis_SR_register(GIE);

       // ADCCTL0 |= ADCENC | ADCSC;

    while(1)
    {
        //ADCCTL0 |= ADCENC | ADCSC;
        __no_operation();                                       // Only for debugger

    }
}


// ADC interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC_VECTOR))) ADC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(ADCIV,ADCIV_ADCIFG))
    {
        case ADCIV_NONE:
            break;
        case ADCIV_ADCOVIFG:
            break;
        case ADCIV_ADCTOVIFG:
            break;
        case ADCIV_ADCHIIFG:
            break;
        case ADCIV_ADCLOIFG:
            break;
        case ADCIV_ADCINIFG:
            break;
        case ADCIV_ADCIFG:
            ADC_Result[counter] = ADCMEM0;
            int i = 0;
            int flag = 0;


            if(counter == 0)
            {
                if(flag == 0) {


                    SendADC(ADC_Result[counter]);
                    __no_operation();                               // Only for debugger
                    counter = 2;
                    flag = 1;

                }
                                                                //set break point here

            }
            else
            {
                //for ( i = 0; counter >= i; i++){            // Sending each ADC value to the SendADC function

                    SendADC(ADC_Result[counter]);

                 //}

                counter--;
            }
            break;
        default:
            break;
    }
}



// Timer A0 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A (void)
#else
#error Compiler not supported!
#endif
{
    __bic_SR_register(GIE);

            timer_exit++;
            // Activate Timer for 1 seconds
            if (timer_exit == TIMER_SEC_VALUE) {
                ADCCTL0 |= ADCENC | ADCSC;                           // Sampling and conversion start
                timer_exit = 0;
            }

    __bis_SR_register(GIE);

}


void SendADC(int ADC_Result){                  // send the adc array to this function.
                                                // This function must return the adc values.

    int sizeofDigits;
    int asciiArray[MAX_Digits];
    int i = 0;


        int* ADC_Digits_Val = DivideByDigit(ADC_Result, &sizeofDigits);          // sending the ADC values to digit division function

      // Calculating Ascii values of the Digits of each ADC value
           for ( i = 0; i < sizeofDigits; i++) {

               asciiArray[i] = ADC_Digits_Val[i] + 48;

               // Sending all digits of one value via UART

               while(!(UCA0IFG&UCTXIFG));                              // from UART
               UCA0TXBUF = asciiArray[i];

           }

           while(!(UCA0IFG&UCTXIFG));                              // from UART
           UCA0TXBUF = '\n';

}


int* DivideByDigit(int ADC_val, int* sizeofDigits) {


    int tempNum;
    int* ADC_Digit_val;
    int digitNumber = 0;
    int i = 0;


    tempNum = ADC_val;


    // Calculating digit number of the integer.

    while (tempNum != 0) {
        digitNumber++;
        tempNum /= 10;
    }

    //Memory allocation.
    ADC_Digit_val = (int*)malloc(digitNumber * sizeof(int));
      if (ADC_Digit_val == 0) {
          while(!(UCA0IFG&UCTXIFG));
          UCA0TXBUF = 48;
          //exit(1);
      }

    // Divide the integer into digits.

    for ( i = digitNumber - 1; i >= 0; i--) {
        ADC_Digit_val[i] = ADC_val % 10;
        ADC_val /= 10;
    }

       // If ADC_val is negative.

       if (ADC_val < 0) {
           while(!(UCA0IFG&UCTXIFG));
           UCA0TXBUF = '-';
           //__bis_SR_register(LPM0_bits | GIE);                     // Enter LPM0 w/ interrupts
         }


       *sizeofDigits = digitNumber; // Sending array's size.
       return ADC_Digit_val; // Sending array's address.

}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCA0IV,USCI_UART_UCTXCPTIFG))
  {
    case USCI_NONE: break;
    case USCI_UART_UCRXIFG:
      __no_operation();
      break;
    case USCI_UART_UCTXIFG: break;
    case USCI_UART_UCSTTIFG: break;
    case USCI_UART_UCTXCPTIFG: break;
    default: break;
  }
}



void Software_Trim()
{
    unsigned int oldDcoTap = 0xffff;
    unsigned int newDcoTap = 0xffff;
    unsigned int newDcoDelta = 0xffff;
    unsigned int bestDcoDelta = 0xffff;
    unsigned int csCtl0Copy = 0;
    unsigned int csCtl1Copy = 0;
    unsigned int csCtl0Read = 0;
    unsigned int csCtl1Read = 0;
    unsigned int dcoFreqTrim = 3;
    unsigned char endLoop = 0;

    do
    {
        CSCTL0 = 0x100;                         // DCO Tap = 256
        do
        {
            CSCTL7 &= ~DCOFFG;                  // Clear DCO fault flag
        }while (CSCTL7 & DCOFFG);               // Test DCO fault flag

        __delay_cycles((unsigned int)3000 * MCLK_FREQ_MHZ);// Wait FLL lock status (FLLUNLOCK) to be stable
                                                           // Suggest to wait 24 cycles of divided FLL reference clock
        while((CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)) && ((CSCTL7 & DCOFFG) == 0));

        csCtl0Read = CSCTL0;                   // Read CSCTL0
        csCtl1Read = CSCTL1;                   // Read CSCTL1

        oldDcoTap = newDcoTap;                 // Record DCOTAP value of last time
        newDcoTap = csCtl0Read & 0x01ff;       // Get DCOTAP value of this time
        dcoFreqTrim = (csCtl1Read & 0x0070)>>4;// Get DCOFTRIM value

        if(newDcoTap < 256)                    // DCOTAP < 256
        {
            newDcoDelta = 256 - newDcoTap;     // Delta value between DCPTAP and 256
            if((oldDcoTap != 0xffff) && (oldDcoTap >= 256)) // DCOTAP cross 256
                endLoop = 1;                   // Stop while loop
            else
            {
                dcoFreqTrim--;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }
        else                                   // DCOTAP >= 256
        {
            newDcoDelta = newDcoTap - 256;     // Delta value between DCPTAP and 256
            if(oldDcoTap < 256)                // DCOTAP cross 256
                endLoop = 1;                   // Stop while loop
            else
            {
                dcoFreqTrim++;
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }

        if(newDcoDelta < bestDcoDelta)         // Record DCOTAP closest to 256
        {
            csCtl0Copy = csCtl0Read;
            csCtl1Copy = csCtl1Read;
            bestDcoDelta = newDcoDelta;
        }

    }while(endLoop == 0);                      // Poll until endLoop == 1

    CSCTL0 = csCtl0Copy;                       // Reload locked DCOTAP
    CSCTL1 = csCtl1Copy;                       // Reload locked DCOFTRIM
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)); // Poll until FLL is locked
}



void Init_UART() {

     // Configure UART pins
     P5SEL0 |= BIT1 | BIT2;                    // set 2-UART pin as second function
     SYSCFG3|=USCIA0RMP;                       //Set the remapping source
     // Configure UART
     UCA0CTLW0 |= UCSWRST;
     UCA0CTLW0 |= UCSSEL__SMCLK;

     // Baud Rate calculation
     // 8000000/(16*9600) = 52.083
     // Fractional portion = 0.083
     // User's Guide Table 17-4: UCBRSx = 0x49
     // UCBRFx = int ( (52.083-52)*16) = 1
     UCA0BR0 = 52;                             // 8000000/16/9600
     UCA0BR1 = 0x00;
     UCA0MCTLW = 0x4900 | UCOS16 | UCBRF_1;

     UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
     UCA0IE |= UCRXIE;                         // Enable USCI_A0 RX interrupt


     __bis_SR_register(GIE);          //Global interrupt enable


}
