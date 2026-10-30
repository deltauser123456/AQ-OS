#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)

static size_t cursor_row = 0;
static size_t cursor_col = 0;
static uint8_t terminal_color = 0x0F;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void scroll_if_needed(void) {
    if (cursor_row < VGA_HEIGHT) return;
    for (size_t row = 1; row < VGA_HEIGHT; row++) {
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] = VGA_MEMORY[row * VGA_WIDTH + col];
        }
    }
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)terminal_color << 8));
    for (size_t col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }
    cursor_row = VGA_HEIGHT - 1;
}

static void put_char(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
                (uint16_t)(' ' | ((uint16_t)terminal_color << 8));
        }
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)((uint8_t)c | ((uint16_t)terminal_color << 8));
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    scroll_if_needed();
    update_cursor();
}

static void print(const char* text) {
    while (*text) put_char(*text++);
}

static int strings_equal(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static const char keymap[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4',
    [0x06]='5', [0x07]='6', [0x08]='7', [0x09]='8',
    [0x0A]='9', [0x0B]='0', [0x0C]='-', [0x0D]='=',
    [0x0E]='\b', [0x0F]='\t',
    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r',
    [0x14]='t', [0x15]='y', [0x16]='u', [0x17]='i',
    [0x18]='o', [0x19]='p', [0x1A]='[', [0x1B]=']',
    [0x1C]='\n',
    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f',
    [0x22]='g', [0x23]='h', [0x24]='j', [0x25]='k',
    [0x26]='l', [0x27]=';', [0x28]='\'', [0x29]='`',
    [0x2B]='\\',
    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v',
    [0x30]='b', [0x31]='n', [0x32]='m', [0x33]=',',
    [0x34]='.', [0x35]='/', [0x39]=' '
};

static char command[80];
static size_t command_length = 0;

static void show_prompt(void) {
    print("\nAQ> ");
}

static void run_command(void) {
    command[command_length] = '\0';
    if (strings_equal(command, "help")) {
        print("\nCommands: help, about, version, clear");
    } else if (strings_equal(command, "about")) {
        print("\nAQ OS is a tiny experimental 32-bit operating system.");
    } else if (strings_equal(command, "version")) {
        print("\nAQ OS version 0.1");
    } else if (strings_equal(command, "clear")) {
        // Clear the display without erasing the new prompt.
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            VGA_MEMORY[i] = (uint16_t)(' ' | ((uint16_t)terminal_color << 8));
        }
        cursor_row = 0;
        cursor_col = 0;
        update_cursor();
    } else if (command_length != 0) {
        print("\nUnknown command. Type help.");
    }
    command_length = 0;
    show_prompt();
}

static void keyboard_poll(void) {
    if ((inb(0x64) & 1) == 0) return;
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) return;
    if (scancode >= 128) return;

    char c = keymap[scancode];
    if (!c) return;

    if (c == '\n') {
        put_char('\n');
        run_command();
    } else if (c == '\b') {
        if (command_length > 0) {
            command_length--;
            put_char('\b');
        }
    } else if (c >= 32 && c <= 126 && command_length < sizeof(command) - 1) {
        command[command_length++] = c;
        put_char(c);
    }
}

void kernel_main(void) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (uint16_t)(' ' | ((uint16_t)terminal_color << 8));
    }
    cursor_row = 0;
    cursor_col = 0;
    update_cursor();

    print("========================================");
    print("\n             AQ OS 0.1");
    print("\n          32-bit i386 kernel");
    print("\n========================================");
    print("\nBooted successfully!");
    print("\nType help to see available commands.");
    show_prompt();

    for (;;) keyboard_poll();
}
