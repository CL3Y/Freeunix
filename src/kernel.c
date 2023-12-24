#include "kernel.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "io_ports.h"
#include <stdint.h>
#include "framebuffer.h"
#include "multiboot.h"
#include "stdint-gcc.h"
#include "ctypes.h"
#include "qemu.h"
#include "romfont.h"
#include <string.h>


#define BRAND_QEMU  1
#define BRAND_VBOX  2

#define MAX_FILENAME_LENGTH 20
#define MAX_FILE_COUNT 100
#define MAX_FILE_CONTENT_LENGTH 100

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    int size;
    char content[MAX_FILE_CONTENT_LENGTH];
} File;

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    int file_count;
    File files[MAX_FILE_COUNT];
} Directory;

Directory root_directory;

// Custom strcpy function for your file system
void custom_strcpy(char *dest, const char *src) {
    while (*src != '\0') {
        *dest = *src;
        src++;
        dest++;
    }
    *dest = '\0'; // Null-terminate the destination string
}

void initFileSystem() {
    custom_strcpy(root_directory.name, "root");
    root_directory.file_count = 0;
}

void createFile(char *name, char *content) {
    if (root_directory.file_count < MAX_FILE_COUNT) {
        File newFile;
        custom_strcpy(newFile.name, name);
        newFile.size = strlen(content);
        custom_strcpy(newFile.content, content);
        newFile.content[MAX_FILE_CONTENT_LENGTH - 1] = '\0';

        root_directory.files[root_directory.file_count++] = newFile;
        printf("File '%s' created successfully.\n", name);
    } else {
        printf("File system full. Cannot create more files.\n");
    }
}

void listFiles() {
    printf("Files in root directory:\n");
    for (int i = 0; i < root_directory.file_count; ++i) {
        printf("- %s, %s\n", root_directory.files[i].name, root_directory.files[i].content);
    }
}


void __cpuid(uint32 type, uint32 *eax, uint32 *ebx, uint32 *ecx, uint32 *edx) {
    asm volatile("cpuid"
                : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                : "0"(type)); // put the type into eax
}

int cpuid_info(int print) {
    uint32 brand[12];
    uint32 eax, ebx, ecx, edx;
    uint32 type;

    memset(brand, 0, sizeof(brand));
    __cpuid(0x80000002, (uint32 *)brand+0x0, (uint32 *)brand+0x1, (uint32 *)brand+0x2, (uint32 *)brand+0x3);
    __cpuid(0x80000003, (uint32 *)brand+0x4, (uint32 *)brand+0x5, (uint32 *)brand+0x6, (uint32 *)brand+0x7);
    __cpuid(0x80000004, (uint32 *)brand+0x8, (uint32 *)brand+0x9, (uint32 *)brand+0xa, (uint32 *)brand+0xb);

    if (print) {
        printf("Brand: %s\n", brand);
        for(type = 0; type < 4; type++) {
            __cpuid(type, &eax, &ebx, &ecx, &edx);
            printf("type:0x%x, eax:0x%x, ebx:0x%x, ecx:0x%x, edx:0x%x\n", type, eax, ebx, ecx, edx);
        }
    }

    if (strstr(brand, "QEMU") != NULL)
        return BRAND_QEMU;

    return BRAND_VBOX;
}


BOOL is_echo(char *b) {
    if((b[0]=='e')&&(b[1]=='c')&&(b[2]=='h')&&(b[3]=='o'))
        if(b[4]==' '||b[4]=='\0')
            return TRUE;
    return FALSE;
}

void shutdown() {
    int brand = cpuid_info(0);
    // QEMU
    if (brand == BRAND_QEMU)
        outports(0x604, 0x2000);
    else
        // VirtualBox
        outports(0x4004, 0x3400);
}

void login_and_run() {
    char buffer[255];
    const char *shell = "Password> ";
    gdt_init();
    idt_init();

    console_init(COLOR_WHITE, COLOR_BLUE);
    keyboard_init();

    printf("Login Needed To Continue...\n");
    printf("Default Password: 'RetroUnix'.\n");

    while(1) {
        printf(shell);
        memset(buffer, 0, sizeof(buffer));
        getstr_bound(buffer, strlen(shell));

        if (strlen(buffer) == 0)
            continue;
        if(strcmp(buffer, "RetroUnix") == 0) {
            main_loop();
        }
        else if(strcmp(buffer, "shutdown") == 0) {
            shutdown();
        } 
        else {
            printf("invalid command: %s\n", buffer);
        }

    }
}

void main_loop() {
    char buffer[255];
    const char *shell = "RetroUnix-SHELL@root$ ";

    gdt_init();
    idt_init();
    initFileSystem();

    console_init(COLOR_WHITE, COLOR_BLUE);
    keyboard_init();

    printf("starting terminal...\n");
    printf("INIT RetroUnix.....\n");
    printf("Kernel Ready (Type 'help' to see list of supported commands)\n\n");

    while(1) {
        printf(shell);
        memset(buffer, 0, sizeof(buffer));
        getstr_bound(buffer, strlen(shell));
        // Draw the box using framebuffer functions
       // Draw the box using framebuffer functions

        if (strlen(buffer) == 0)
            continue;
        if(strcmp(buffer, "cpuid") == 0) {
            cpuid_info(1);
        } 
        else if(strcmp(buffer, "help") == 0) {
            printf("RetroUnix Operating System & Tiny OS Terminal\n");
            printf("Commands:\n\n help\n cpuid\n clear\n mkfile(Create a basic file, like touch command)\n ls(List all files and their contents in DIR)\n whoami\n echo\n shutdown\n\n");
        }
        else if(strcmp(buffer, "mkfile") == 0) {
            createFile("newfile.txt", "Sample content for the new file.");
        }
        else if(strcmp(buffer, "ls") == 0) {
            listFiles();
        }
        else if(strcmp(buffer, "whoami") == 0) {
            printf("RetroUnix Operating System User:\n");
            printf("User: ROOT; (Sudo Privileges: TRUE)\n");
            printf("CPUID INFO: \n\n");
            cpuid_info(1);
        }
        else if(strcmp(buffer, "clear") == 0) {
            console_clear(COLOR_WHITE, COLOR_BLUE);
        } 
        else if(is_echo(buffer)) {
            printf("%s\n", buffer + 5);
        } 
        else if(strcmp(buffer, "shutdown") == 0) {
            shutdown();
        } else {
            printf("invalid command: %s\n", buffer);
        }
    }
}

void kmain(){
    //FUNCTION THAT RUNS THE OS
    login_and_run();
}