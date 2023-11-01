#include "syscall.h"

#define CONSOLE_INPUT 0
#define CONSOLE_OUTPUT 1

int len(char *str)
{
    int len = 0;
    while (*str != '\0')
    {
        str++;
        len++;
    }
    return len;
}

int main() {
    int id;
    char buffer[256];

    id = Open("text.txt", 1);
    Read(buffer,256, id);
    Write(buffer, len(buffer),CONSOLE_OUTPUT);
    Close(id);

    Halt();
}