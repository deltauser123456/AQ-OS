#include <stdint.h>
#include <stddef.h>

/* AQ OS 0.3 - working desktop apps (RAM-only prototype) */
typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint8_t syms[16];
    uint32_t mmap_length, mmap_addr, drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type;
    uint8_t color_info[6];
} multiboot_info_t;

static volatile uint32_t *fb;
static uint32_t screen_w, screen_h, pitch_pixels;
static int mouse_x=320, mouse_y=240, mouse_packet[3], mouse_cycle;
static int menu_open, window_open, active_app, old_left;
static uint32_t wallpaper=0x000078A8;
static int calc_saved, calc_op, calc_new=1;
static char calc_display[16]="0";
static char note[256]="AQ OS NOTEPAD - TYPE HERE";
static int note_len=25, note_saved=1;
static int file_page=0;

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static void pixel(int x,int y,uint32_t c){if(x>=0&&y>=0&&(uint32_t)x<screen_w&&(uint32_t)y<screen_h)fb[(uint32_t)y*pitch_pixels+(uint32_t)x]=c;}
static void rect(int x,int y,int w,int h,uint32_t c){if(w<=0||h<=0)return;for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixel(xx,yy,c);}
static void outline(int x,int y,int w,int h,uint32_t c){rect(x,y,w,2,c);rect(x,y+h-2,w,2,c);rect(x,y,2,h,c);rect(x+w-2,y,2,h,c);}
static const uint8_t *glyph(char c){
#define G(ch,a,b,d,e,f,g,h) case ch:{static const uint8_t v[7]={a,b,d,e,f,g,h};return v;}
switch(c){
G('A',14,17,17,31,17,17,17) G('B',30,17,17,30,17,17,30) G('C',14,17,16,16,16,17,14)
G('D',30,17,17,17,17,17,30) G('E',31,16,16,30,16,16,31) G('F',31,16,16,30,16,16,16)
G('G',14,17,16,23,17,17,15) G('H',17,17,17,31,17,17,17) G('I',14,4,4,4,4,4,14)
G('J',7,2,2,2,18,18,12) G('K',17,18,20,24,20,18,17) G('L',16,16,16,16,16,16,31)
G('M',17,27,21,21,17,17,17) G('N',17,25,21,19,17,17,17) G('O',14,17,17,17,17,17,14)
G('P',30,17,17,30,16,16,16) G('Q',14,17,17,17,21,18,13) G('R',30,17,17,30,20,18,17)
G('S',15,16,16,14,1,1,30) G('T',31,4,4,4,4,4,4) G('U',17,17,17,17,17,17,14)
G('V',17,17,17,17,17,10,4) G('W',17,17,17,21,21,21,10) G('X',17,17,10,4,10,17,17)
G('Y',17,17,10,4,4,4,4) G('Z',31,1,2,4,8,16,31)
G('0',14,17,19,21,25,17,14) G('1',4,12,4,4,4,4,14) G('2',14,17,1,2,4,8,31)
G('3',30,1,1,14,1,1,30) G('4',2,6,10,18,31,2,2) G('5',31,16,16,30,1,1,30)
G('6',14,16,16,30,17,17,14) G('7',31,1,2,4,8,8,8) G('8',14,17,17,14,17,17,14)
G('9',14,17,17,15,1,1,14) G('-',0,0,0,31,0,0,0) G('+',0,4,4,31,4,4,0)
G('*',0,21,14,31,14,21,0) G('/',1,2,2,4,8,8,16) G('=',0,0,31,0,31,0,0)
G(':',0,4,4,0,4,4,0) G('.',0,0,0,0,0,12,12) G('!',4,4,4,4,4,0,4)
G('>',16,8,4,2,4,8,16) G('<',1,2,4,8,4,2,1) G('?',14,17,1,2,4,0,4)
G('_',0,0,0,0,0,0,31) G('(',2,4,8,8,8,4,2) G(')',8,4,2,2,2,4,8)
G('[',14,8,8,8,8,8,14) G(']',14,2,2,2,2,2,14) G('#',10,31,10,10,31,10,0)
G(' ',0,0,0,0,0,0,0)
default:{static const uint8_t z[7]={0};return z;}
}
#undef G
}
static void text(int x,int y,const char*s,uint32_t color,int scale){while(*s){char c=*s++;if(c>='a'&&c<='z')c-=32;const uint8_t*g=glyph(c);for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(g[yy]&(1u<<(4-xx)))rect(x+xx*scale,y+yy*scale,scale,scale,color);x+=6*scale;}}
static void button(int x,int y,int w,int h,const char *s){rect(x,y,w,h,0x00D4D0C8);outline(x,y,w,h,0x00FFFFFF);rect(x+2,y+h-3,w-4,1,0x00808080);text(x+7,y+(h-7)/2,s,0x00000000,1);}
static void cursor_draw(void){for(int y=0;y<16;y++)for(int x=0;x<12;x++){if(x<=y/2&&y<14)pixel(mouse_x+x,mouse_y+y,0);if(x==0||(x<=y/2&&(x==y/2||y==13)))pixel(mouse_x+x,mouse_y+y,0x00FFFFFF);}}
static void icon(int x,int y,const char*label,int kind){if(kind==0){rect(x+5,y+2,34,25,0x000080D0);rect(x+8,y+5,28,19,0x00A8DFFF);rect(x+20,y+2,2,25,0x00FFFFFF);}else if(kind==1){rect(x+5,y+4,30,24,0x00F4D56A);rect(x+7,y+9,28,22,0x00D6D6D6);outline(x+7,y+9,28,22,0x00808080);}else if(kind==2){rect(x+7,y+3,30,29,0x00E8E8E8);outline(x+7,y+3,30,29,0x00406080);rect(x+18,y+7,8,21,0x00808080);rect(x+11,y+14,22,8,0x00808080);rect(x+20,y+12,4,12,0x00FFD040);}else if(kind==3){rect(x+8,y+1,27,32,0x00D0D0D0);outline(x+8,y+1,27,32,0x00404040);rect(x+11,y+5,21,7,0x00002020);for(int i=0;i<3;i++)for(int j=0;j<3;j++)rect(x+12+j*6,y+16+i*5,4,3,0x00606060);}else{rect(x+8,y+1,27,32,0x00FFFFFF);outline(x+8,y+1,27,32,0x00406080);for(int i=0;i<4;i++)rect(x+12,y+9+i*5,18,1,0x004080C0);rect(x+12,y+5,12,2,0x00808080);}text(x-4,y+39,label,0x00FFFFFF,1);}
static void draw_calc(int x,int y){text(x+18,y+48,"CALCULATOR",0x00000080,2);rect(x+18,y+70,244,30,0x00000000);text(x+25,y+81,calc_display,0x0000FF00,2);const char*keys[16]={"7","8","9","/","4","5","6","*","1","2","3","-","C","0","=","+"};for(int i=0;i<16;i++)button(x+18+(i%4)*62,y+112+(i/4)*42,56,36,keys[i]);}
static void draw_note(int x,int y){text(x+18,y+48,"NOTEPAD",0x00000080,2);rect(x+16,y+72,340,160,0x00FFFFFF);outline(x+16,y+72,340,160,0x00808080);for(int i=0;i<note_len;i++){int row=i/48,col=i%48;text(x+22+col*6,y+80+row*12,(char[2]){note[i],0},0x00000000,1);}button(x+18,y+240,82,27,"CLEAR");button(x+110,y+240,82,27,"SAVE");text(x+205,y+250,note_saved?"SAVED IN RAM":"EDITING",0x00008000,1);}
static void draw_files(int x,int y){text(x+18,y+48,"FILE MANAGER",0x00000080,2);text(x+20,y+75,file_page==0?"THIS PC > AQ DRIVE":"AQ DRIVE > DOCUMENTS",0x00000000,1);button(x+18,y+91,100,25,"BACK");const char*items[4];if(file_page==0){items[0]="DOCUMENTS";items[1]="PICTURES";items[2]="MUSIC";items[3]="SYSTEM INFO";}else{items[0]="WELCOME TXT";items[1]="NOTES TXT";items[2]="NO FILES";items[3]="";}for(int i=0;i<4;i++){rect(x+22,y+128+i*25,310,21,0x00F0F0F0);text(x+31,y+135+i*25,items[i],0x00000080,1);}text(x+20,y+238,"DEMO FILES - DISK STORAGE NOT YET ENABLED",0x00800000,1);}
static void draw_settings(int x,int y){text(x+18,y+48,"PERSONALIZATION",0x00000080,2);text(x+20,y+80,"DESKTOP COLOR",0x00000000,1);button(x+20,y+103,90,32,"BLUE");button(x+118,y+103,90,32,"GREEN");button(x+216,y+103,90,32,"PURPLE");text(x+20,y+160,"AQ OS 0.3",0x00000080,2);text(x+20,y+185,"MOUSE: PS/2",0x00000000,1);text(x+20,y+202,"APPS STORE DATA IN RAM",0x00000000,1);}
static void draw_window(void){if(!window_open)return;int x=90,y=38,w=(int)screen_w-125,h=(int)screen_h-90;if(w<370)w=370;if(h<320)h=320;rect(x+5,y+5,w,h,0x00404040);rect(x,y,w,h,0x00C0C0C0);outline(x,y,w,h,0x00FFFFFF);rect(x+3,y+3,w-6,25,0x00000080);const char*t=active_app==0?"FILE MANAGER":active_app==1?"RECYCLE BIN":active_app==2?"SETTINGS":active_app==3?"CALCULATOR":"NOTEPAD";text(x+12,y+11,t,0x00FFFFFF,1);button(x+w-26,y+5,20,20,"X");rect(x+8,y+35,w-16,h-43,0x00FFFFFF);if(active_app==0)draw_files(x,y);else if(active_app==1){text(x+20,y+65,"RECYCLE BIN",0x00000080,2);text(x+20,y+100,"THE BIN IS EMPTY",0,1);}else if(active_app==2)draw_settings(x,y);else if(active_app==3)draw_calc(x,y);else draw_note(x,y);}
static void draw_desktop(void){rect(0,0,(int)screen_w,(int)screen_h,wallpaper);rect(0,(int)screen_h-90,(int)screen_w,56,0x003A9D36);rect(0,(int)screen_h-34,(int)screen_w,34,0x000052B8);rect(0,(int)screen_h-34,(int)screen_w,2,0x0089C5FF);text(12,10,"AQ OS 0.3",0x00FFFFFF,1);icon(20,48,"MY COMPUTER",0);icon(20,119,"RECYCLE BIN",1);icon(20,190,"SETTINGS",2);icon(20,261,"CALCULATOR",3);icon(20,332,"NOTEPAD",4);draw_window();button(5,(int)screen_h-30,88,25,"START");if(window_open)text(110,(int)screen_h-21,"APP OPEN",0x00FFFFFF,1);if(menu_open){int my=(int)screen_h-245;rect(7,my,225,207,0x00D4D0C8);outline(7,my,225,207,0x00FFFFFF);const char*names[6]={"FILE MANAGER","RECYCLE BIN","SETTINGS","CALCULATOR","NOTEPAD","SHUT DOWN"};for(int i=0;i<6;i++){rect(45,my+5+i*32,184,29,0x00FFFFFF);text(53,my+16+i*32,names[i],i==5?0x00800000:0,1);}}cursor_draw();}
static void open_app(int a){active_app=a;window_open=1;menu_open=0;}
static void calc_key(int k){if(k==12){calc_saved=calc_op=0;calc_new=1;calc_display[0]='0';calc_display[1]=0;return;}if(k==14){int v=0;for(int i=0;calc_display[i];i++)if(calc_display[i]>='0'&&calc_display[i]<='9')v=v*10+calc_display[i]-'0';if(calc_op==1)v=calc_saved+v;else if(calc_op==2)v=calc_saved-v;else if(calc_op==3)v=calc_saved*v;else if(calc_op==4)v=v?calc_saved/v:0;int n=v,idx=0;if(n<0){calc_display[idx++]='-';n=-n;}char tmp[12];int z=0;do{tmp[z++]='0'+n%10;n/=10;}while(n&&z<10);while(z)calc_display[idx++]=tmp[--z];calc_display[idx]=0;calc_op=0;calc_new=1;return;}if(k==3||k==7||k==11||k==15){calc_saved=0;for(int i=0;calc_display[i];i++)if(calc_display[i]>='0'&&calc_display[i]<='9')calc_saved=calc_saved*10+calc_display[i]-'0';calc_op=k==3?1:k==7?3:k==11?2:4;calc_new=1;return;}char d=k==13?'0':(char)('0'+(k%4==0?7-k/4*3:k%4==1?8-k/4*3:k%4==2?9-k/4*3:0));if(k==13)d='0';if(calc_new){calc_display[0]=d;calc_display[1]=0;calc_new=0;}else{int n=0;while(calc_display[n]&&n<12)n++;if(n<11){calc_display[n]=d;calc_display[n+1]=0;}}}
static void click(int x,int y){int ty=(int)screen_h-34;if(y>=ty&&x<100){menu_open=!menu_open;return;}if(menu_open){int my=(int)screen_h-245;if(x>=45&&x<230){if(y>=my+5&&y<my+34)open_app(0);else if(y>=my+37&&y<my+66)open_app(1);else if(y>=my+69&&y<my+98)open_app(2);else if(y>=my+101&&y<my+130)open_app(3);else if(y>=my+133&&y<my+162)open_app(4);else if(y>=my+165&&y<my+194)menu_open=0;}return;}if(window_open){int wx=90,wy=38,ww=(int)screen_w-125;if(x>=wx+ww-28&&y>=wy+4&&y<wy+28){window_open=0;return;}int lx=x-wx,ly=y-wy;if(active_app==3&&lx>=18&&lx<266&&ly>=112&&ly<280){int col=(lx-18)/62,row=(ly-112)/42,k=row*4+col;if(k>=0&&k<16)calc_key(k);}else if(active_app==4){if(ly>=240&&ly<270&&lx>=18&&lx<100){note_len=0;note[0]=0;note_saved=0;}else if(ly>=240&&ly<270&&lx>=110&&lx<200)note_saved=1;}else if(active_app==2&&ly>=103&&ly<135){if(lx<110)wallpaper=0x000078A8;else if(lx<208)wallpaper=0x00007830;else wallpaper=0x00503080;}else if(active_app==0&&ly>=91&&ly<118&&lx<120)file_page=0;else if(active_app==0&&ly>=128&&ly<151&&lx<180)file_page=1;return;}if(x>=15&&x<=80){if(y>=44&&y<105)open_app(0);else if(y>=115&&y<176)open_app(1);else if(y>=186&&y<247)open_app(2);else if(y>=257&&y<318)open_app(3);else if(y>=328&&y<389)open_app(4);}}
static void mouse_wait(uint8_t t){uint32_t n=100000;if(!t){while(n--&&(inb(0x64)&1)==0){}}else{while(n--&&(inb(0x64)&2)!=0){}}}
static void mouse_write(uint8_t v){mouse_wait(1);outb(0x64,0xD4);mouse_wait(1);outb(0x60,v);}
static void mouse_init(void){mouse_wait(1);outb(0x64,0xA8);mouse_wait(1);outb(0x64,0x20);mouse_wait(0);uint8_t s=inb(0x60);s|=2;s&=(uint8_t)~0x20;mouse_wait(1);outb(0x64,0x60);mouse_wait(1);outb(0x60,s);mouse_write(0xF6);mouse_wait(0);(void)inb(0x60);mouse_write(0xF4);mouse_wait(0);(void)inb(0x60);}
static char keychar(uint8_t s){static const char map[58]={0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';', '\'', '`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};if(s>=58)return 0;return map[s];}
static void keyboard_poll(void){uint8_t status=inb(0x64);if(!(status&1)||(status&0x20))return;uint8_t s=inb(0x60);if(s&0x80)return;if(active_app!=4||!window_open)return;char c=keychar(s);if(c=='\b'){if(note_len>0)note[--note_len]=0;note_saved=0;}else if(c=='\n'){if(note_len<254){note[note_len++]=' ';note[note_len]=0;note_saved=0;}}else if(c>=32&&c<=126&&note_len<254){note[note_len++]=c;note[note_len]=0;note_saved=0;}draw_desktop();}
static void mouse_poll(void){if((inb(0x64)&0x21)!=0x21)return;uint8_t b=inb(0x60);if(mouse_cycle==0){if(!(b&8))return;mouse_packet[0]=b;mouse_cycle=1;}else if(mouse_cycle==1){mouse_packet[1]=b;mouse_cycle=2;}else{mouse_packet[2]=b;mouse_cycle=0;mouse_x+=(int8_t)mouse_packet[1];mouse_y-=(int8_t)mouse_packet[2];if(mouse_x<0)mouse_x=0;if(mouse_y<0)mouse_y=0;if(mouse_x>=(int)screen_w)mouse_x=(int)screen_w-1;if(mouse_y>=(int)screen_h)mouse_y=(int)screen_h-1;int left=mouse_packet[0]&1;if(left&&!old_left)click(mouse_x,mouse_y);old_left=left;draw_desktop();}}
void kernel_main(uint32_t magic,multiboot_info_t*mbi){if(magic!=0x2BADB002||!(mbi->flags&(1u<<12))||mbi->framebuffer_bpp!=32||mbi->framebuffer_type!=1){volatile uint16_t*v=(volatile uint16_t*)0xB8000;const char*m="AQ OS: FRAMEBUFFER ERROR";for(int i=0;m[i];i++)v[i]=(uint16_t)(0x0F00|m[i]);for(;;)__asm__ volatile("hlt");}fb=(volatile uint32_t*)(uintptr_t)mbi->framebuffer_addr;screen_w=mbi->framebuffer_width;screen_h=mbi->framebuffer_height;pitch_pixels=mbi->framebuffer_pitch/4;mouse_x=(int)screen_w/2;mouse_y=(int)screen_h/2;draw_desktop();mouse_init();for(;;){keyboard_poll();mouse_poll();}}
