#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <nds/arm9/console.h>

#include "flux.h"


dword fluxcolors[]=
{
    0x003040|INVISIBLE,	// COL_BAKGND=   SYSCOL(1), 
    0x404040,           // COL_WINDOW=   SYSCOL(2), 
    0xFFFFFF,           // COL_TEXT=     SYSCOL(3), 
    0x606060,           // COL_ITEM=     SYSCOL(4), 
    0x707070,           // COL_ITEMHI=   SYSCOL(5), 
    0x505050,           // COL_ITEMLO=   SYSCOL(6), 
    0xF0F0F0,           // COL_ITEMTEXT= SYSCOL(7),
    0x686868,           // COL_TITLE=    SYSCOL(8),
    0xFFFFFF,           // COL_FRAMEHI=  SYSCOL(9),
    0x000000            // COL_FRAMELO=  SYSCOL(10),
};

bool do_texalpha= false;

int main(void)
{
    touchPosition touch;

    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);	//sub bg 0 will be used to print text
    vramSetBankC(VRAM_C_SUB_BG);

    SUB_BG0_CR = BG_MAP_BASE(31);

    BG_PALETTE_SUB[255] = RGB15(31,31,31);	//by default font will be rendered with color 255

    //consoleInit() is a lot more fluxible but this gets you up and running quick
    consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);
    
/////////////
    powerON(POWER_ALL);

    // set mode 0, enable BG0 and set it to 3D
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);

    // irqs are nice
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // initialize gl
    glInit();
    
    // setup the rear plane
    glClearColor(2, 4, 3, 16);
    glClearDepth(GL_MAX_DEPTH);
    
    // this should work the same as the normal gl call
    glViewPort(0,0, 255,191);
    
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    
    glMatrixMode(GL_PROJECTION);
    
    glLoadIdentity();
    //~ glOrtho(0, 1, 192.0/256.0, 0, -10, 10);
    glOrthof32(0, 1<<12, (192<<12)/256, 0, -10<<12, 10<<12);
    
    REG_POWERCNT^= POWER_SWAP_LCDS;
/////////////


    flux_init();
    create_frame_groups();
    create_button_groups();
    create_titleframe();
    
    dmaCopy(fluxcolors, syscol_table, sizeof(fluxcolors));
    
    dword rc1= create_rect(NOPARENT, 20,20, 128,96, 0x808080);
    dword rc= create_rect(NOPARENT, 10,10, 128,96, 0x404040|TRANSL_3);
    dword frm= clone_frame("titleframe", rc);
    clone_frame("titleframe", rc1);
    //~ clone_group("button", rc, 10,30, 30,14, ALIGN_LEFT|ALIGN_TOP);

    create_rect(rc, 10,10, 10,10, 0xFFFFFF|TRANSL_1);
    create_rect(rc, 20,10, 10,10, 0xFFFFFF|TRANSL_2);
    create_rect(rc, 30,10, 10,10, 0xFFFFFF|TRANSL_3);
    create_rect(rc, 10,15, 30,10, 0xFFFFFF|TRANSL_1);
    
    create_text(rc, 10,30, 40,40, "Text! Space!\nNewline!", 0xFFFFFF, FONT_DEFAULT);
    
    bool redraw_all= true;
    bool touch_wasdown= false;
    int lasttouch_x, lasttouch_y;
    while(1)
    {
	scanKeys();
    	u32 keysheld= keysHeld();
	u32 keysdown= keysDown();
	touch= touchReadXY();
	
	if(keysdown&KEY_A)
	{
	    redraw_all^= 1;
	    iprintf("redraw each frame: %s\n", redraw_all? "on": "off");
	}
	
	if(keysdown&KEY_B)
	{
	    do_texalpha^= 1;
	    iprintf("texture alpha: %s\n", do_texalpha? "on": "off");
	}
	
	if(touch.x || touch.y)
	{
	    flux_mouse_event(touch.px, touch.py, 1);
	    lasttouch_x= touch.px, lasttouch_y= touch.py;
	    touch_wasdown= true;
	}
	else
	{
	    if(touch_wasdown) flux_mouse_event(lasttouch_x, lasttouch_y, 0);
	    touch_wasdown= false;
	}
	    

	flux_tick();
	if(redraw_all)
	{
	    redraw_rect(&viewport);
	    update_rect(&viewport);
	}
	
	swiWaitForVBlank();
    }

    return 0;
}
