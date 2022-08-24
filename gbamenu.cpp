#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <string.h>

#define PerPage 13  //每页显示多少个游戏
#define FindMB 4     //每隔4mb找一个游戏
#define MAX_ITEMS 64     //最多18个游戏 
#define NAME_LEN 16     //名字16个字夫


//! Put function in IWRAM.
#define IWRAM_CODE __attribute__((section(".iwram"), long_call))
__attribute__ ((section(".devkitadv.config"))) int __gba_multiboot;// MULTIBOOT


typedef struct{
    char name[NAME_LEN + 1];
    u8 Addr;
}BootItem;

BootItem item[MAX_ITEMS];

int Page = 0;
int Select = 0;
int gameCnt = 0;
u8 sramBackup[3];
char *title = (char*)0x80000A0;
char *logo = (char*)0x8000004;


inline void backupSram(){
    sramBackup[0] = *(vu8*)0x0E000002;//Bank 选择信号
    sramBackup[1] = *(vu8*)0x0E000003;
    sramBackup[2] = *(vu8*)0x0E000004;
    return;
}


inline void restoreSram(){
    *(vu8*)0x0E000002 = sramBackup[0];
    *(vu8*)0x0E000003 = sramBackup[1];
    *(vu8*)0x0E000004 = sramBackup[2];
    return;
}

/*//gbabf代码
*(vu8*)0x0A000002 = addr.byte[3];
*(vu8*)0x0A000003 = addr.byte[2];
*(vu8*)0x0A000004 = addr.byte[1];
*/

IWRAM_CODE void gotoChipOffset(u8 Addr,char Lock){
    
    u32 chipAddr = (Addr/32 * 0x10000000) + (0x4000C0 + (Addr & 31) * 0x020202);
    union{
		u32 addr;
		u8 byte[4];
	}add;
	add.addr = chipAddr;
	u16 data = *(vu16*)(0xBD|0x8000000);

    *(vu8*)0x0E000002 = add.byte[3];
    *(vu8*)0x0E000003 = add.byte[2];
    *(vu8*)0x0E000004 = add.byte[1];
    if(Lock){
        *(vu8*)0x0E000003 = add.byte[2] | 0x80;
    }
    
	int timeout = 0x1000;
	while(timeout && (*(vu16*)(0xBD|0x8000000)) == data)timeout--;
    
    if(Lock){//Backup is done at findGames()
        restoreSram();//还原
        __asm volatile ("swi 0x26":::);
		__asm("SWI 0");
    }
    return;
}


static const unsigned char nintendo_logo[] =
{
	0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
	0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
	0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
	0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
	0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
	0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
	0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
	0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
	0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
	0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07,
};


IWRAM_CODE void findGames(){

    u16 Addr;
    for(Addr = 0 ;Addr < MAX_ITEMS*FindMB; Addr += FindMB){
        gotoChipOffset(Addr,0);
        if(memcmp(nintendo_logo, logo, sizeof(nintendo_logo)) == 0){
			strncpy(item[gameCnt].name, title, NAME_LEN);
            item[gameCnt].Addr = Addr;
            gameCnt++;
        }
    }
    gotoChipOffset(0,0);//返回menu
    restoreSram();
    return;
}


void Redraw(){
	
	printf("\e[2J========GBA Multi Menu========\n");
	for(int i = 0; i < PerPage; i++){
		int x = Page*PerPage + i + 1;//+1解决初始0位置不算游戏
		int y = (Page*PerPage + i + 1)*FindMB;//+1解决初始0位置不算游戏
		printf("\n%c %02u.%12s %dMB",(i == Select) ? '>' : ' ', x, item[x].name, y);
	}
	printf("\n\n==============================");
    printf("\nPage %d",Page);//第几页
}



int main(void) {
    
    irqInit();
	irqEnable(IRQ_VBLANK);
	consoleInit( 0 , 4 , 0, NULL , 0 , 15);

	BG_COLORS[0]=RGB8(33,171,243);//背景颜色
	BG_COLORS[241]=RGB5(31,31,31);//文字颜色

	SetMode(MODE_0 | BG0_ON);
	VBlankIntrWait();//添加以启动
	backupSram();
    findGames();
    Redraw();
     while(1) {
		VBlankIntrWait();
		int keys = keysDown();
		scanKeys();
        if(keys & KEY_RIGHT) {
            Page++;
            Select=0;
            Page = (Page >= MAX_ITEMS/PerPage) ? MAX_ITEMS/PerPage : Page;
            Redraw();
        }
        if(keys & KEY_LEFT) {
            Page--;
            Select=0;
			Page = (Page < 0) ? 0 : Page;
            Redraw();
        }
        if(keys & KEY_UP) {
            Select--;
			Select = (Select < 0) ? 0 : Select;
            Redraw();
        }
        if(keys & KEY_DOWN) {
            Select++;
			Select = (Select >= PerPage) ? PerPage-1 : Select;
            Redraw();
        }
        if(keys & KEY_A) {
            gotoChipOffset(item[PerPage*Page+Select+1].Addr,1);//+1实现第2个游戏开始，让出第一个菜单选择
        }
    }
	return 0;
}
