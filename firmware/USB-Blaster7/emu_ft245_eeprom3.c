#include "./USB/usb.h"

#include "emu_ft245_eeprom.h"

#define SD1P 20
#define SD2P (SD1P+USB_SD_Ptr[1][0])
#define SD3P (SD2P+USB_SD_Ptr[2][0])
#define SD4P (SD3P+USB_SD_Ptr[3][0])

extern ROM USB_DEVICE_DESCRIPTOR device_dsc;
extern ROM BYTE configDescriptor1[];
extern ROM BYTE *ROM USB_SD_Ptr[];

static WORD_VAL checksum;

BYTE eeprom_read(BYTE a){
	if(a== 2) return ((ROM BYTE*)&device_dsc.idVendor)[0];	//vid,
	if(a== 3) return ((ROM BYTE*)&device_dsc.idVendor)[1];
	if(a== 4) return ((ROM BYTE*)&device_dsc.idProduct)[0];	//pid,
	if(a== 5) return ((ROM BYTE*)&device_dsc.idProduct)[1];
	if(a== 6) return ((ROM BYTE*)&device_dsc.bcdDevice)[0];	//ver
	if(a== 7) return ((ROM BYTE*)&device_dsc.bcdDevice)[1];
	if(a== 8) return configDescriptor1[7];	//attr,
	if(a== 9) return configDescriptor1[8];	//pow
	if(a==10) return 0x1C;
	if(a==12) return ((ROM BYTE*)&device_dsc.bcdUSB)[0];
	if(a==13) return ((ROM BYTE*)&device_dsc.bcdUSB)[1];
	if(a==14) return 0x80+SD1P;			//0x80+&sd001
	if(a==15) return USB_SD_Ptr[1][0];	//sizeof(sd001)
	if(a==16) return 0x80+SD2P;			//0x80+&sd002
	if(a==17) return USB_SD_Ptr[2][0];	//sizeof(sd002)
	if(a==18) return 0x80+SD3P;			//0x80+&sd003
	if(a==19) return USB_SD_Ptr[3][0];	//sizeof(sd003)
	if(SD1P<=a&&a<SD2P) return USB_SD_Ptr[1][a-SD1P];
	if(SD2P<=a&&a<SD3P) return USB_SD_Ptr[2][a-SD2P];
	if(SD3P<=a&&a<SD4P) return USB_SD_Ptr[3][a-SD3P];
	if(SD4P<=a&&a<SD4P+4) return ((ROM BYTE*)&device_dsc.iManufacturer)[a-SD4P];
	if(a==126) return (checksum.byte.LB);
	if(a==127) return (checksum.byte.HB);
	return 0; 
}

void eeprom_init(void){
	BYTE i;
	checksum.Val=0xAAAA;
	for(i=0;i<126;i+=2){
		checksum.byte.LB^=eeprom_read(i);
		checksum.byte.HB^=eeprom_read(i+1);
		if(checksum.Val&0x8000) checksum.Val=(checksum.Val<<1)|1;
		else checksum.Val=(checksum.Val<<1);
	}
}
