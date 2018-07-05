// main.c

/** INCLUDES *******************************************************/
#include "./USB/usb.h"
#include "HardwareProfile.h"
//#include "GenericTypeDefs.h"
//#include "Compiler.h"
//#include "usb_config.h"
//#include "USB/usb_device.h"

#include "bitreverse.h"
#include "emu_ft245_eeprom.h"
#include "BlasterMacros.h"

/** CONFIGURATION **************************************************/
#pragma config CPUDIV = NOCLKDIV
#pragma config USBDIV = OFF
#pragma config FOSC   = HS
#pragma config PLLEN  = ON
#pragma config FCMEN  = OFF
#pragma config IESO   = OFF
#pragma config PWRTEN = OFF
#pragma config BOREN  = OFF
#pragma config BORV   = 30
//#pragma config VREGEN = ON
#pragma config WDTEN  = OFF
#pragma config WDTPS  = 32768
#pragma config MCLRE  = OFF
#pragma config HFOFST = OFF
#pragma config STVREN = ON
#pragma config LVP    = OFF
#pragma config XINST  = OFF
#pragma config BBSIZ  = OFF
#pragma config CP0    = OFF
#pragma config CP1    = OFF
#pragma config CPB    = OFF
#pragma config WRT0   = OFF
#pragma config WRT1   = OFF
#pragma config WRTB   = OFF
#pragma config WRTC   = OFF
#pragma config EBTR0  = OFF
#pragma config EBTR1  = OFF
#pragma config EBTRB  = OFF       

/** V A R I A B L E S **********************************************/
#pragma udata USB_VARS
BYTE OutPacket[2][64];
BYTE InPacket[1][64];
extern volatile BYTE CtrlTrfData[8];
#define ctrl_trf_buf CtrlTrfData

#pragma udata access accessram
near BYTE fifo_wp,fifo_rp;		//FIFO Write Pos / Read Pos
near BYTE acc0,acc1;			//AccessRAM for FastTemporaly
near BYTE recv_byte;			//byte count received
near BYTE jtag_byte;			//byte count fot jtag mode
near BYTE aser_byte;			//byte count fot active-serial mode
near BYTE_VAL _flags;
#define read _flags.bits.b0		//simulteneous reading flag
#define received _flags.bits.b1	//OUT Packet Received flag
#ifdef ENABLE_PPB
  #define PPB _flags.bits.b7	//current ping-pong-buffer
  #define TogglePPB() PPB=!PPB 
#else
  #define PPB 0 
  #define TogglePPB()
#endif
near BYTE bufptr;

#pragma udata GP1
BYTE InFIFO[256];

#pragma udata
USB_HANDLE USBOutHandle0;
USB_HANDLE USBOutHandle1;
USB_HANDLE USBInHandle0;

/** P R I V A T E  P R O T O T Y P E S *****************************/
void USBDeviceTasks(void);
void HighIntCode();
void LowIntCode();
void USBCBSendResume(void);

/** VECTOR REMAPPING ***********************************************/
extern void _startup(void);

//Vector Remapping for Standalone Operating
#pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR(void) { _asm goto 0x1008 _endasm }

#pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR(void) { _asm goto 0x1018 _endasm }

//Vector Remapping for MCHIP Generic Bootloader
#pragma code GEN_RESET_VECTOR=0x800
void GEN_Reset(void) { _asm goto _startup _endasm }

#pragma code GEN_HIGH_INTERRUPT_VECTOR=0x808
void GEN_HighInt(void) { _asm goto HighIntCode _endasm }

#pragma code GEN_LOW_INTERRUPT_VECTOR=0x818
void GEN_LowInt(void) { _asm goto LowIntCode _endasm }

//Vector Remapping for MCHIP HID Bootloader
#pragma code HID_RESET_VECTOR=0x1000
void HID_Reset(void) { _asm goto _startup _endasm }

#pragma code HID_HIGH_INTERRUPT_VECTOR=0x1008
void HID_HighInt(void) { _asm goto HighIntCode _endasm }

#pragma code HID_LOW_INTERRUPT_VECTOR=0x1018
void HID_LowInt(void) { _asm goto LowIntCode _endasm }

#pragma code
	
#pragma interrupt HighIntCode
void HighIntCode() {
    static unsigned char cnt;
    
    
		//Check which interrupt flag caused the interrupt.
		//Service the interrupt, Clear the interrupt flag, Etc.
        #if defined(USB_INTERRUPT)
	    USBDeviceTasks();
        #endif

        
        if (PIR1bits.TMR1IF) {
            cnt++;
            
            if (cnt > 22){
                LATCbits.LATC2 ^= 1;
                cnt = 0;
            }
            PIR1bits.TMR1IF = 0; 
        } 
        
}	//This return will be a "retfie fast", since this is in a #pragma interrupt section 

#pragma interruptlow LowIntCode
void LowIntCode() {
		//Check which interrupt flag caused the interrupt.
		//Service the interrupt, Clear the interrupt flag, Etc. 
    
	
}	//This return will be a "retfie", since this is in a #pragma interruptlow section 

/** DECLARATIONS ***************************************************/
#pragma code

//for debug use - measure cycle counts
void test(){
	BYTE *p;
	bufptr=64;
	fifo_wp=0;
	fifo_rp=0;
	acc0=0x53;
	acc1=bitreverse[SSPBUF];	//10cy
	enqueue(acc0);		//9cy
	enqueue(acc1);		//9cy
	JTAG_Write(acc0);		//56cy
	JTAG_RW(acc0,acc1);	//73cy
	ASer_RW(acc0,acc1);	//73cy
	dequeue(InPacket[0]+2,fifo_used());	// 116cy@2byte
	fifo_wp=0;for(acc0=0;1;acc0++){enqueue(acc0);if(acc0==255)break;}
	fifo_wp=100; fifo_rp=190;
	dequeue(InPacket[0]+2,62);	//818cy@62byte
	dequeue(InPacket[0]+2,62);	//890cy@62byte
	acc1=bitreverse[OutPacket[0][bufptr++]];	//17cy
	enqueue(bitreverse[SSPBUF]);	//20cy
	Nop();
}

void main(void) {
	//IO Initialize
	ANSEL  = 0;
	ANSELH = 0;
	SLRCON = 0;

	//Port direction setup
	TRISA=0;
	TRISB=0;
	TRISC=0;
	TTDO=1;
	TADO=1;

	//Initial value setup
	OUTP=0b00011010;

	//Peripheral setup
	T0CON=0b10000000;		//On,Fcy/2
	INTCONbits.TMR0IE=0;	//Polling
	#ifdef USE_SPI
	SSPSTAT=0b01000000;		//SMP=0,CKE=1;
	#ifdef SPI_3MHz
	SSPCON1=0b00000001;		//Fspi=3MHz
	#endif
	#ifdef SPI_12MHz
	SSPCON1=0b00000000;		//Fspi=12MHz
	#endif
	ChangePIO();
	#endif	
  
    //Setup clock for fast clock (50 KHz)
    T2CON = 0b00000101; //TMR2ON, FCY/4
    CCP1CONbits.P1M = 0b00;
    CCP1CONbits.CCP1M = 0b1100; //PMW mode
    
    //50KHz output on RC5, set as output
    TRISCbits.TRISC5 = 0;
    PR2 = 60;
    CCPR1L = 30;
    PSTRCON = 0b00000001;
    
    //Setup clock for slow clock (1 Hz)
    //
    TMR1H = 0x00;
    TMR1L = 0x00;
    T1CONbits.T1CKPS1 = 1;
    T1CONbits.T1CKPS0 = 0;
    PIE1bits.TMR1IE = 1;  
    T1CONbits.TMR1ON = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

    TRISCbits.RC2 = 0;   
    LATCbits.LATC2 = 0;   
    
	//Program Initialize
	InPacket[0][0]=0x31;
	InPacket[0][1]=0x60;
	eeprom_init();
    USBDeviceInit();
    #if defined(USB_INTERRUPT)
    USBDeviceAttach();
    #endif

//	test();

    while(1) {
        #if defined(USB_POLLING)
        USBDeviceTasks();
        #endif
		
	    if((USBDeviceState < CONFIGURED_STATE)||(USBSuspendControl==1)) {
			//Something to do in not connected to host.
			LACT=0;
		}else{
			//Check and transmit queued data
			if(!USBHandleBusy(USBInHandle0)){
				acc0=fifo_used();
				if(62<=acc0){		//send filled packet to host
					dequeue(InPacket[0]+2,62);
					mUSBTxOnePacket(USBInHandle0,1,InPacket[0],2+62);
					INTCONbits.TMR0IF=1;	//schedule to send indicator			
				}else if(acc0){			//send non-filled packet to host
					dequeue(InPacket[0]+2,acc0);
					mUSBTxOnePacket(USBInHandle0,1,InPacket[0],2+acc0);
					INTCONbits.TMR0IF=1;	//schedule sending indicator
				}else if(INTCONbits.TMR0IF){	//send packet indicator
					TMR0H=5536>>8;	//(65536-5536)/(12000/2)=10ms
					TMR0L=5536&0xFF;
					INTCONbits.TMR0IF=0;
					mUSBTxOnePacket(USBInHandle0,1,InPacket[0],2);
				}
			}
	         //Check to see if data has arrive.
			if(!recv_byte){
				if(PPB==0){	//even packet or no ping-pong
					if(!USBHandleBusy(USBOutHandle0)){
						recv_byte=USBHandleGetLength(USBOutHandle0);
						//p=USBHandleGetAddr(USBOutHandle0);
						bufptr=0;
						received=1;
					}
				}else{		//odd packet
					if(!USBHandleBusy(USBOutHandle1)){
						recv_byte=USBHandleGetLength(USBOutHandle1);
						//p=USBHandleGetAddr(USBOutHandle1);
						bufptr=64;
						received=1;
					}
				}
			}
			if(fifo_able()<recv_byte) continue;	//FIFO Full check
			if(recv_byte){
				LACT=0;
				do{
					if(jtag_byte){
						#ifdef USE_SPI
						ChangeSPI();
						acc0=bitreverse[OutPacket[0][bufptr++]];
						#endif
						if(!read){
							do{
								#ifdef USE_SPI
								SSPBUF=acc0;
								acc0=bitreverse[OutPacket[0][bufptr++]];
								SPI_Wait();
								#else
								acc0=OutPacket[0][bufptr++];
								JTAG_Write(acc0);
								#endif
								jtag_byte--;
								recv_byte--;
							}while(jtag_byte&&recv_byte);
						}else{
							do{
								#ifdef USE_SPI
								SSPBUF=acc0;
								acc0=bitreverse[OutPacket[0][bufptr++]];
								SPI_Wait();
								acc1=bitreverse[SSPBUF];
								enqueue(acc1);
								#else
								acc0=OutPacket[0][bufptr++];
								JTAG_RW(acc0,acc1);
								enqueue(acc1);
								#endif
								jtag_byte--;
								recv_byte--;
							}while(jtag_byte&&recv_byte);
						}
						#ifdef USE_SPI
						bufptr--;
						ChangePIO();
						#endif
					}else if(aser_byte){
						if(!read){
							#ifdef USE_SPI
							ChangeSPI();
							acc0=bitreverse[OutPacket[0][bufptr++]];
							#endif
							do{
								#ifdef USE_SPI
								SSPBUF=acc0;
								acc0=bitreverse[OutPacket[0][bufptr++]];
								SPI_Wait();
								#else
								acc0=OutPacket[0][bufptr++];
								JTAG_Write(acc0);
								#endif
								aser_byte--;
								recv_byte--;
							}while(aser_byte&&recv_byte);
							#ifdef USE_SPI
							bufptr--;
							ChangePIO();
							#endif
						}else{
							do{
								acc0=OutPacket[0][bufptr++];
								ASer_RW(acc0,acc1);
								enqueue(acc1);
								aser_byte--;
								recv_byte--;
							}while(aser_byte&&recv_byte);
						}
					}else{
						do{
							acc0=OutPacket[0][bufptr++];
							bitcopy(bitmask(acc0,6),read);
							if(bitmask(acc0,7)){		//EnterSerialMode
								LTCK=0;		//bug fix
								if(OUTP&0x8){	//nCS=1:JTAG
									jtag_byte=acc0&0x3F;
								}else{		//nCS=0:ActiveSerial
									aser_byte=acc0&0x3F;
								}
								recv_byte--;
								break;
							}else{			//BitBangMode
								OUTP=acc0;
								if(read){
									acc1=0;
									if(PADO) acc1|=0x02;
									if(PTDO) acc1|=0x01;
									enqueue(acc1);
								}
								recv_byte--;
							}
						}while(recv_byte);
					}
				}while(recv_byte);
			}
			if(received&&(!recv_byte)){
				LACT=1;
				if(PPB==0){
					mUSBRxOnePacket(USBOutHandle0,2,OutPacket[0],64);
					TogglePPB();
				}else{
					mUSBRxOnePacket(USBOutHandle1,2,OutPacket[1],64);
					TogglePPB();
				}
				received=0;
			}
		}
    }//end while
}//end main

// ************** USB Callback Functions ****************************
void USBCBSuspend(void) {}
void USBCBWakeFromSuspend(void){}
void USBCB_SOF_Handler(void){}
void USBCBErrorHandler(void){}

extern volatile CTRL_TRF_SETUP SetupPkt;
void USBCBCheckOtherReq(void){
	//FT245 emulation
	BYTE index;
	if(SetupPkt.RequestType==2){	//Vendor request
		if(SetupPkt.DataDir==0){	//0utput
			//Responce by sending zero-length packet
			//I don't know if this way is right, but working:)
			USBEP0SendRAMPtr(ctrl_trf_buf,0,USB_EP0_INCLUDE_ZERO);
			return;
		}
		if(SetupPkt.bRequest==0x90){
			index=(SetupPkt.wIndex<<1)&0x7E;
			ctrl_trf_buf[0]=eeprom_read(index);
			ctrl_trf_buf[1]=eeprom_read(index+1);
		}else{
			ctrl_trf_buf[0]=0x36;
			ctrl_trf_buf[1]=0x83;
		}
		USBEP0SendRAMPtr(ctrl_trf_buf,2,USB_EP0_INCLUDE_ZERO);
	}
}

void USBCBStdSetDscHandler(void){}

void USBCBInitEP(void){
    USBEnableEndpoint(1,USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    USBEnableEndpoint(2,USB_OUT_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
	USBOutHandle0=0;
	USBOutHandle1=0;
	USBInHandle0=0;
	fifo_wp=0;
	fifo_rp=0;
	recv_byte=0;
    jtag_byte=0;
    aser_byte=0;
	//TODO: Buffer pointer reset
}

void USBCBSendResume(void){
    static WORD delay_count;
    if(USBGetRemoteWakeupStatus() == TRUE){
        if(USBIsBusSuspended() == TRUE){
            USBMaskInterrupts();
            USBCBWakeFromSuspend();
            USBSuspendControl = 0; 
            USBBusIsSuspended = FALSE;
            delay_count = 3600U;        
            do{ delay_count--; }while(delay_count);
            USBResumeControl = 1;       // Start RESUME signaling
            delay_count = 1800U;        // Set RESUME line for 1-13 ms
            do{ delay_count--; }while(delay_count);
            USBResumeControl = 0;       //Finished driving resume signalling
            USBUnmaskInterrupts();
        }
    }
}

#if defined(ENABLE_EP0_DATA_RECEIVED_CALLBACK)
void USBCBEP0DataReceived(void){}
#endif

BOOL USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, WORD size){
    switch(event)
    {
        case EVENT_TRANSFER:
            //Add application specific callback task or callback function here if desired.
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED: 
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        case EVENT_TRANSFER_TERMINATED:
            //Add application specific callback task or callback function here if desired.
            //The EVENT_TRANSFER_TERMINATED event occurs when the host performs a CLEAR
            //FEATURE (endpoint halt) request on an application endpoint which was 
            //previously armed (UOWN was = 1).  Here would be a good place to:
            //1.  Determine which endpoint the transaction that just got terminated was 
            //      on, by checking the handle value in the *pdata.
            //2.  Re-arm the endpoint if desired (typically would be the case for OUT 
            //      endpoints).
            break;
        default:
            break;
    }      
    return TRUE; 
}

/** EOF main.c *************************************************/
