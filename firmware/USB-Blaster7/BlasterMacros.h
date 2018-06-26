#ifndef BLASTER_MACROS_H
#define BLASTER_MACROS_H

/******** Settings ********/

//Enable ping-pong-buffer
#define ENABLE_PPB

//Use SPI Module
#define USE_SPI

//SPI Freq Setting
#define SPI_12MHz
//#define SPI_3MHz

//output port definition
#define OUTP LATC
#define LTDI LATCbits.LATC4		//TDI
#define LTCK LATCbits.LATC0		//TCK
#define LACT LATBbits.LATB7		//Activitty LED

//SPI,PIO output tris definition
#ifdef USE_SPI
#define TSDO TRISCbits.TRISC7	//SPI:TDI
#define TTDI TRISCbits.TRISC4	//PIO:TDI
#define TSCK TRISBbits.TRISB6	//SPI:TCK
#define TTCK TRISCbits.TRISC0	//PIO:TCK
#endif

//input tris definitions for PIO
#define TTDO TRISBbits.TRISB4	//JTAG:TDO
#define TADO TRISBbits.TRISB5	//AS:ASDO

//input port definitions
#define PTDO PORTBbits.RB4		//JTAG:TDO
#define PADO PORTBbits.RB5		//AS:ASDO


/******** Macros ********/

#define ChangeSPI() { TTDI=1; TTCK=1; SSPCON1bits.SSPEN=1; TSCK=0; TSDO=0;}
#define ChangePIO() { TSDO=1; TSCK=1; SSPCON1bits.SSPEN=0; TTCK=0; TTDI=0;}

#ifdef ENABLE_PPB
  #if USB_PING_PONG_MODE==USB_PING_PONG__FULL_PING_PONG
  #elif USB_PING_PONG_MODE==USB_PING_PONG__ALL_BUT_EP0
  #else
    #error "Could not enable ping-pong-buffer!!"
  #endif
#endif

#define bitcopy(in,out) {if(in) (out)=1; if(!(in)) (out)=0;}
#define bitmask(byte,i) ((byte)&(1<<(i)))

//JTAG operation macro functions
#define tck() {LTCK=1; Nop(); LTCK=0;}

// 7cycles/bit -> 1.71MHz
#define JTAG_Write(a) {\
	bitcopy(a&0x01,LTDI); tck();\
	bitcopy(a&0x02,LTDI); tck();\
	bitcopy(a&0x04,LTDI); tck();\
	bitcopy(a&0x08,LTDI); tck();\
	bitcopy(a&0x10,LTDI); tck();\
	bitcopy(a&0x20,LTDI); tck();\
	bitcopy(a&0x40,LTDI); tck();\
	bitcopy(a&0x80,LTDI); tck();\
}

// 9cycles/bit -> 1.33MHz
#define JTAG_RW(a,ret) {\
	ret=0;\
	bitcopy(a&0x01,LTDI); if(PTDO) ret|=0x01; tck();\
	bitcopy(a&0x02,LTDI); if(PTDO) ret|=0x02; tck();\
	bitcopy(a&0x04,LTDI); if(PTDO) ret|=0x04; tck();\
	bitcopy(a&0x08,LTDI); if(PTDO) ret|=0x08; tck();\
	bitcopy(a&0x10,LTDI); if(PTDO) ret|=0x10; tck();\
	bitcopy(a&0x20,LTDI); if(PTDO) ret|=0x20; tck();\
	bitcopy(a&0x40,LTDI); if(PTDO) ret|=0x40; tck();\
	bitcopy(a&0x80,LTDI); if(PTDO) ret|=0x80; tck();\
}

// 9cycles/bit -> 1.33MHz
#define ASer_RW(a,ret) {\
	ret=0;\
	bitcopy(a&0x01,LTDI); if(PADO) ret|=0x01; tck();\
	bitcopy(a&0x02,LTDI); if(PADO) ret|=0x02; tck();\
	bitcopy(a&0x04,LTDI); if(PADO) ret|=0x04; tck();\
	bitcopy(a&0x08,LTDI); if(PADO) ret|=0x08; tck();\
	bitcopy(a&0x10,LTDI); if(PADO) ret|=0x10; tck();\
	bitcopy(a&0x20,LTDI); if(PADO) ret|=0x20; tck();\
	bitcopy(a&0x40,LTDI); if(PADO) ret|=0x40; tck();\
	bitcopy(a&0x80,LTDI); if(PADO) ret|=0x80; tck();\
}

#ifdef SPI_12MHz
	#define SPI_Wait()
#else
	#define SPI_Wait() {while(!SSPSTATbits.BF){};}
#endif

//In FIFO Functions
#define enqueue(a){\
	InFIFO[fifo_wp]=(a);\
	fifo_wp++;\
}

#define dequeue(a,c){\
	if(fifo_rp<=(255-(c)+1)){\
		memcpy((void*)(a),(void*)&InFIFO[fifo_rp],c);\
		fifo_rp+=(c);\
	}else{\
		acc1=255-fifo_rp+1;\
		memcpy((void*)(a),(void*)&InFIFO[fifo_rp],acc1);\
		memcpy((void*)((a)+acc1),(void*)InFIFO,(c)-acc1);\
		fifo_rp+=(c);\
	}\
}

#define fifo_used() (fifo_wp-fifo_rp)
#define fifo_able() (fifo_rp-fifo_wp-1)

// USB communication macro functions
//(same as same name functions but fast)
extern volatile BDT_ENTRY *pBDTEntryOut[USB_MAX_EP_NUMBER+1];
extern volatile BDT_ENTRY *pBDTEntryIn[USB_MAX_EP_NUMBER+1];
#if (USB_PING_PONG_MODE == USB_PING_PONG__NO_PING_PONG)
	#define USB_NEXT_PING_PONG 0x0000
#elif (USB_PING_PONG_MODE == USB_PING_PONG__EP0_OUT_ONLY)
    #define USB_NEXT_PING_PONG 0x0000
#elif (USB_PING_PONG_MODE == USB_PING_PONG__FULL_PING_PONG)
	#define USB_NEXT_PING_PONG 0x0004
#elif (USB_PING_PONG_MODE == USB_PING_PONG__ALL_BUT_EP0)
	#define USB_NEXT_PING_PONG 0x0004
#else
	#error "No ping pong mode defined."
#endif
#define PBE(h) ((volatile BDT_ENTRY*)(h))
#define mUSBRxOnePacket(h,ep,adr,cnt) {\
	h = (USB_HANDLE)(pBDTEntryOut[ep]);\
	PBE(h)->ADR = (WORD)adr;\
	PBE(h)->CNT = cnt;\
	PBE(h)->STAT.Val &= _DTSMASK;\
	PBE(h)->STAT.Val |= _USIE | _DTSEN;\
	((BYTE_VAL*)&pBDTEntryOut[ep])->Val ^= USB_NEXT_PING_PONG;\
}
#define mUSBTxOnePacket(h,ep,adr,cnt) {\
	h = (USB_HANDLE)(pBDTEntryIn[ep]);\
	PBE(h)->ADR = (WORD)adr;\
	PBE(h)->CNT = cnt;\
	PBE(h)->STAT.Val &= _DTSMASK;\
	PBE(h)->STAT.Val |= _USIE | _DTSEN;\
	((BYTE_VAL*)&pBDTEntryIn[ep])->Val ^= USB_NEXT_PING_PONG;\
}

#endif
