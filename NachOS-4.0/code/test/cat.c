#include "syscall.h"

int main()
{
    char fileName[] = "text.txt";
    int length, id;
    int check;

    id = Open(fileName, 1);
    if (id != -1)
    {
        print("Read\n");
        Read(fileName, length, id);
    }
    else
        PrintString("Open file failed\n");

    Close(id);

    Halt();
}