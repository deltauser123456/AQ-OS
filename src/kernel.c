#include <stdint.h>
#include <stddef.h>

/* AQ OS 0.2 - first graphical desktop */
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
static int mouse_x = 320, mouse_y = 240;
static int mouse_packet[3], mouse_cycle;
static int menu_open, window_open;
static int active_app;
static int old_left;

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" : : "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

static void pixel(int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && (uint32_t)x < screen_w && (uint32_t)y < screen_h)
        fb[(uint32_t)y * pitch_pixels + (uint32_t)x] = c;
}
static void rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) pixel(xx, yy, c);
}
static void outline(int x, int y, int w, int h, uint32_t c) {
    rect(x,y,w,2,c); rect(x,y+h-2,w,2,c); rect(x,y,2,h,c); rect(x+w-2,y,2,h,c);
}

/* Compact 5x7 font: uppercase letters, digits and basic punctuation. */
static const uint8_t *glyph(char c) {
#define G(ch,a,b,d,e,f,g,h) case ch: { static const uint8_t v[7]={a,b,d,e,f,g,h}; return v; }
    switch (c) {
    G('A',14,17,17,31,17,17,17) G('B',30,17,17,30,17,17,30)
    G('C',14,17,16,16,16,17,14) G('D',30,17,17,17,17,17,30)
    G('E',31,16,16,30,16,16,31) G('F',31,16,16,30,16,16,16)
    G('G',14,17,16,23,17,17,15) G('H',17,17,17,31,17,17,17)
    G('I',14,4,4,4,4,4,14) G('J',7,2,2,2,18,18,12)
    G('K',17,18,20,24,20,18,17) G('L',16,16,16,16,16,16,31)
    G('M',17,27,21,21,17,17,17) G('N',17,25,21,19,17,17,17)
    G('O',14,17,17,17,17,17,14) G('P',30,17,17,30,16,16,16)
    G('Q',14,17,17,17,21,18,13) G('R',30,17,17,30,20,18,17)
    G('S',15,16,16,14,1,1,30) G('T',31,4,4,4,4,4,4)
    G('U',17,17,17,17,17,17,14) G('V',17,17,17,17,17,10,4)
    G('W',17,17,17,21,21,21,10) G('X',17,17,10,4,10,17,17)
    G('Y',17,17,10,4,4,4,4) G('Z',31,1,2,4,8,16,31)
    G('0',14,17,19,21,25,17,14) G('1',4,12,4,4,4,4,14)
    G('2',14,17,1,2,4,8,31) G('3',30,1,1,14,1,1,30)
    G('4',2,6,10,18,31,2,2) G('5',31,16,16,30,1,1,30)
    G('6',14,16,16,30,17,17,14) G('7',31,1,2,4,8,8,8)
    G('8',14,17,17,14,17,17,14) G('9',14,17,17,15,1,1,14)
    G('-',0,0,0,31,0,0,0) G(':',0,4,4,0,4,4,0)
    G('.',0,0,0,0,0,12,12) G('/',1,2,2,4,8,8,16)
    G('!',4,4,4,4,4,0,4) G('>',16,8,4,2,4,8,16)
    G('<',1,2,4,8,4,2,1) G('?',14,17,1,2,4,0,4)
    default: { static const uint8_t blank[7]={0,0,0,0,0,0,0}; return blank; }
    }
#undef G
}
static void text(int x, int y, const char *s, uint32_t color, int scale) {
    while (*s) {
        char c = *s++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        const uint8_t *g = glyph(c);
        for (int yy=0; yy<7; yy++)
            for (int xx=0; xx<5; xx++)
                if (g[yy] & (1u << (4-xx)))
                    rect(x+xx*scale,y+yy*scale,scale,scale,color);
        x += 6*scale;
    }
}
static void cursor_draw(void) {
    for (int y=0;y<16;y++) for (int x=0;x<12;x++) {
        if (x <= y/2 && y < 14) pixel(mouse_x+x,mouse_y+y,0x00000000);
        if (x == 0 || (x <= y/2 && (x == y/2 || y == 13)))
            pixel(mouse_x+x,mouse_y+y,0x00FFFFFF);
    }
}
static void draw_icon(int x, int y, int kind, const char *label) {
    if (kind == 0) {
        rect(x+5,y+2,34,25,0x000080D0); rect(x+8,y+5,28,19,0x00A8DFFF);
        rect(x+19,y+27,6,5,0x00D0D0D0); rect(x+13,y+32,18,3,0x00D0D0D0);
    } else if (kind == 1) {
        rect(x+7,y+7,30,25,0x00D6D6D6); rect(x+5,y+4,30,24,0x00F4D56A);
        rect(x+5,y+4,13,5,0x00FFE69A);
    } else {
        rect(x+10,y+3,24,27,0x00DDEEFF); outline(x+10,y+3,24,27,0x00406080);
        rect(x+14,y+10,16,2,0x00406080); rect(x+14,y+16,16,2,0x00406080);
    }
    text(x-1,y+39,label,0x00FFFFFF,1);
}
static void draw_window(void) {
    if (!window_open) return;
    int x=105,y=65,w=(int)screen_w-155,h=(int)screen_h-135;
    if (w<300) w=300; if (h<180) h=180;
    rect(x+5,y+5,w,h,0x00404040);
    rect(x,y,w,h,0x00C0C0C0); outline(x,y,w,h,0x00FFFFFF);
    rect(x+3,y+3,w-6,25,0x00000080);
    text(x+12,y+11,active_app==0?"MY COMPUTER":active_app==1?"RECYCLE BIN":"AQ SETTINGS",0x00FFFFFF,1);
    rect(x+w-25,y+6,17,17,0x00C0C0C0); outline(x+w-25,y+6,17,17,0x00FFFFFF);
    text(x+w-21,y+10,"X",0x00800000,1);
    rect(x+8,y+35,w-16,h-45,0x00FFFFFF);
    if (active_app==0) {
        text(x+22,y+55,"AQ OS FILES",0x00000080,2);
        text(x+22,y+90,"WELCOME TO YOUR COMPUTER",0x00000000,1);
        text(x+22,y+112,"THIS IS AN EARLY DESKTOP BUILD",0x00000000,1);
    } else if (active_app==1) {
        text(x+22,y+55,"RECYCLE BIN",0x00000080,2);
        text(x+22,y+90,"THE BIN IS EMPTY",0x00000000,1);
    } else {
        text(x+22,y+55,"AQ CONTROL PANEL",0x00000080,2);
        text(x+22,y+90,"SETTINGS WILL ARRIVE IN A FUTURE BUILD",0x00000000,1);
    }
}
static void draw_desktop(void) {
    rect(0,0,(int)screen_w,(int)screen_h,0x000078A8);
    /* Simple XP-like blue sky and green rolling hill */
    rect(0,(int)screen_h-115,(int)screen_w,75,0x003A9D36);
    rect(0,(int)screen_h-40,(int)screen_w,40,0x002B7D25);
    rect(0,0,(int)screen_w,28,0x00005AA8);
    text(12,10,"AQ OS",0x00FFFFFF,1);
    draw_icon(20,52,0,"MY COMPUTER");
    draw_icon(20,125,1,"RECYCLE BIN");
    draw_icon(20,198,2,"SETTINGS");
    draw_window();
    /* XP-style taskbar */
    rect(0,(int)screen_h-34,(int)screen_w,34,0x000052B8);
    rect(0,(int)screen_h-34,(int)screen_w,2,0x0089C5FF);
    rect(5,(int)screen_h-30,88,25,0x00008035);
    outline(5,(int)screen_h-30,88,25,0x00B7E6A0);
    text(19,(int)screen_h-22,"START",0x00FFFFFF,2);
    if (window_open) {
        rect(103,(int)screen_h-29,125,23,0x000070B0);
        text(112,(int)screen_h-22,"AQ WINDOW",0x00FFFFFF,1);
    }
    text((int)screen_w-70,(int)screen_h-21,"AQ 02",0x00FFFFFF,1);
    if (menu_open) {
        int my=(int)screen_h-245;
        rect(7,my,225,207,0x00D4D0C8); outline(7,my,225,207,0x00FFFFFF);
        rect(10,my+3,34,201,0x00000080);
        text(17,my+15,"AQ",0x00FFFFFF,2);
        rect(45,my+5,184,30,0x00FFFFFF); text(54,my+16,"MY COMPUTER",0x00000000,1);
        rect(45,my+38,184,30,0x00FFFFFF); text(54,my+49,"RECYCLE BIN",0x00000000,1);
        rect(45,my+71,184,30,0x00FFFFFF); text(54,my+82,"SETTINGS",0x00000000,1);
        rect(45,my+104,184,30,0x00FFFFFF); text(54,my+115,"AQ TERMINAL",0x00000000,1);
        rect(45,my+145,184,2,0x00808080);
        text(54,my+165,"SHUT DOWN",0x00800000,1);
    }
    cursor_draw();
}
static void mouse_wait(uint8_t type) {
    uint32_t timeout=100000;
    if (type==0) { while (timeout-- && (inb(0x64)&1)==0) {} }
    else { while (timeout-- && (inb(0x64)&2)!=0) {} }
}
static void mouse_write(uint8_t value) {
    mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,value);
}
static void mouse_init(void) {
    mouse_wait(1); outb(0x64,0xA8);
    mouse_wait(1); outb(0x64,0x20); mouse_wait(0);
    uint8_t status=inb(0x60); status|=2; status&=(uint8_t)~0x20;
    mouse_wait(1); outb(0x64,0x60); mouse_wait(1); outb(0x60,status);
    mouse_write(0xF6); mouse_wait(0); (void)inb(0x60);
    mouse_write(0xF4); mouse_wait(0); (void)inb(0x60);
}
static void handle_click(int x,int y) {
    int task_y=(int)screen_h-34;
    if (y>=task_y && x<100) { menu_open=!menu_open; return; }
    if (menu_open) {
        int my=(int)screen_h-245;
        if (x>=45 && x<=229) {
            if (y>=my+5 && y<my+35) { active_app=0; window_open=1; }
            else if (y>=my+38 && y<my+68) { active_app=1; window_open=1; }
            else if (y>=my+71 && y<my+101) { active_app=2; window_open=1; }
            else if (y>=my+104 && y<my+134) { active_app=2; window_open=1; }
        }
        menu_open=0; return;
    }
    if (window_open) {
        int wx=105, wy=65, ww=(int)screen_w-155;
        if (x>=wx+ww-27 && y>=wy+4 && y<=wy+27) window_open=0;
        return;
    }
    if (x>=15 && x<=70) {
        if (y>=48 && y<=105) { active_app=0; window_open=1; }
        else if (y>=121 && y<=178) { active_app=1; window_open=1; }
        else if (y>=194 && y<=251) { active_app=2; window_open=1; }
    }
}
static void mouse_poll(void) {
    if ((inb(0x64)&0x21)!=0x21) return;
    uint8_t b=inb(0x60);
    if (mouse_cycle==0) { if (!(b&8)) return; mouse_packet[0]=b; mouse_cycle=1; }
    else if (mouse_cycle==1) { mouse_packet[1]=b; mouse_cycle=2; }
    else {
        mouse_packet[2]=b; mouse_cycle=0;
        int dx=(int8_t)mouse_packet[1], dy=(int8_t)mouse_packet[2];
        mouse_x+=dx; mouse_y-=dy;
        if (mouse_x<0) mouse_x=0; if (mouse_y<0) mouse_y=0;
        if (mouse_x>=(int)screen_w) mouse_x=(int)screen_w-1;
        if (mouse_y>=(int)screen_h) mouse_y=(int)screen_h-1;
        int left=mouse_packet[0]&1;
        if (left && !old_left) handle_click(mouse_x,mouse_y);
        old_left=left;
        draw_desktop();
    }
}
static void vga_puts(volatile uint16_t *v, int *pos, const char *s) {
    while (*s) v[(*pos)++] = (uint16_t)(0x0F00u | (uint8_t)*s++);
}
static void vga_hex(volatile uint16_t *v, int *pos, uint32_t n) {
    const char *digits="0123456789ABCDEF";
    for (int shift=28; shift>=0; shift-=4)
        v[(*pos)++] = (uint16_t)(0x0F00u | (uint8_t)digits[(n>>shift)&15]);
}
void kernel_main(uint32_t magic, multiboot_info_t *mbi) {
    if (magic!=0x2BADB002 || !(mbi->flags & (1u<<12)) ||
        mbi->framebuffer_bpp!=32 || mbi->framebuffer_type!=1) {
        volatile uint16_t *vga=(volatile uint16_t*)0xB8000;
        int p=0;
        vga_puts(vga,&p,"AQ OS BOOT DIAGNOSTICS");
        p=80;
        vga_puts(vga,&p,"MAGIC: "); vga_hex(vga,&p,magic);
        p=160;
        vga_puts(vga,&p,"FLAGS: "); vga_hex(vga,&p,mbi->flags);
        p=240;
        vga_puts(vga,&p,"FRAMEBUFFER ADDRESS LOW: "); vga_hex(vga,&p,(uint32_t)mbi->framebuffer_addr);
        p=320;
        vga_puts(vga,&p,"WIDTH: "); vga_hex(vga,&p,mbi->framebuffer_width);
        vga_puts(vga,&p," HEIGHT: "); vga_hex(vga,&p,mbi->framebuffer_height);
        p=400;
        vga_puts(vga,&p,"BPP: "); vga_hex(vga,&p,mbi->framebuffer_bpp);
        vga_puts(vga,&p," TYPE: "); vga_hex(vga,&p,mbi->framebuffer_type);
        p=480;
        vga_puts(vga,&p,"EXPECTED MAGIC 2BADB002, FLAG BIT 12, BPP 20, TYPE 01");
        for (;;) __asm__ volatile("hlt");
    }
    fb=(volatile uint32_t*)(uintptr_t)mbi->framebuffer_addr;
    screen_w=mbi->framebuffer_width; screen_h=mbi->framebuffer_height;
    pitch_pixels=mbi->framebuffer_pitch/4;
    mouse_x=(int)screen_w/2; mouse_y=(int)screen_h/2;
    draw_desktop();
    mouse_init();
    for (;;) mouse_poll();
}
