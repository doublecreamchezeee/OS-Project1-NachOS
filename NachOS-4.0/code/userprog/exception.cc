// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
#include "stdint.h"
#include "synchconsole.h"

#define MaxFileLength 32
#define MaxStringLength 256

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------

/*
	Increase the program counter to point the next instruction
	Copied from SC_Add
*/
void increaseProgramCounter()
{
	/* set previous programm counter (debugging only)*/
	kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

	/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
	kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(NextPCReg));

	/* set next programm counter for brach execution */
	kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(NextPCReg) + 4);
}

// Code copied from file
//"[2] Giao tiep giua HDH Nachos va chuong trinh nguoi dung.pdf"
//  Input: - User space address (int)
//  - Limit of buffer (int)
//  Output:- Buffer (char*)
//  Purpose: Copy buffer from User memory space to System memory space
char *User2System(int virtAddr, int limit)
{
	int i; // index
	int oneChar;
	char *kernelBuf = NULL;
	kernelBuf = new char[limit + 1]; // need for terminal string
	if (kernelBuf == NULL)
		return kernelBuf;
	memset(kernelBuf, 0, limit + 1);
	// printf("\n Filename u2s:");
	for (i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		// printf("%c",kernelBuf[i]);
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}

// Code copied from file
//"[2] Giao tiep giua HDH Nachos va chuong trinh nguoi dung.pdf"
// Input: - User space address (int)
// - Limit of buffer (int)
// - Buffer (char[])
// Output:- Number of bytes copied (int)
// Purpose: Copy buffer from System memory space to User memory space
int System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0)
		return -1;
	if (len == 0)
		return len;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}

#define MAX_NUM_LENGTH 11
//-2147483648 <= int32 <=2147483647
// so the maximum length of an int32 string is 11
// A character buffer to read and write int32 number
char characterBuffer[MAX_NUM_LENGTH + 1];

// Check if a character is empty space
char isEmptySpace(char ch)
{
	return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\x0b');
}

/**
 * Read characters from console and store them in characterBuffer
 * Stops when meeting an empty space character:
 *  '\n' Line Feed
 *  '\r' Carriage Return
 *  '\t' Horizontal Tab
 *  ' ' Space
 *  '\x0b' 	Vertical Tab
 *  EOF
 **/
void readCharacters()
{
	memset(characterBuffer, 0, sizeof(characterBuffer));
	char ch = kernel->synchConsoleIn->GetChar();

	// Read the first character and check
	if (ch == EOF || isEmptySpace(ch))
	{
		DEBUG(dbgSys, "Illegal character found, ascii code " << (int)ch << '\n');
		return;
	}

	int len = 0;
	// Reads until meeting an empty space or EOF
	while (!(isEmptySpace(ch) || ch == EOF))
	{
		characterBuffer[len++] = ch;
		if (len > MAX_NUM_LENGTH)
		{
			DEBUG(dbgSys, "The string is too long to fit in int32");
			return;
		}
		ch = kernel->synchConsoleIn->GetChar();
	}
}

// Read an integer from console and return
// Uses the above function to read characters from console and store them in characterBuffer
// Return 0 if meeting errors:
// - Read letters instead of number
// - integer overflow
int ReadNumFromConsole()
{
	readCharacters();

	int len = strlen(characterBuffer);
	// Return 0 if kernel didn't read any character
	if (len == 0)
		return 0;

	// 2147483648 doesn't fit in int32 so we can't store it in an int32 integer
	// Therefore, the string "-2147483648" must be compare manually
	if (strcmp(characterBuffer, "-2147483648") == 0)
		return INT32_MIN;

	bool negative = characterBuffer[0] == '-';

	int num = 0;

	for (int i = negative; i < len; ++i)
	{

		char c = characterBuffer[i];

		if (c < '0' || c > '9')
		{
			DEBUG(dbgSys, "Illegal character found " << characterBuffer << ", number expected");
			return 0;
		}
		num = num * 10 + (c - '0');
	}

	if (negative)
		num = -num;

	// If negative is true but num is positive
	// or if negative is false but num is negative
	// then integer overflow happened
	if ((negative && num > 0) || (!negative && num < 0))
	{
		DEBUG(dbgSys, "Number " << characterBuffer << " doesn't fit in int32");
		return 0;
	}

	// Num is safe to return
	return num;
}

// Print an integer to console
// Uses synchConsoleOut->PutChar to print every digit
void PrintNumToConsole(int num)
{
	// Print out '0' if num is 0
	if (num == 0)
		return kernel->synchConsoleOut->PutChar('0');

	// Because 2147483648 doesn't fit in int32, we must print -2147483648 (INT32_MIN) manually
	if (num == INT32_MIN)
	{
		kernel->synchConsoleOut->PutChar('-');
		for (int i = 0; i < 10; ++i)
			kernel->synchConsoleOut->PutChar("2147483648"[i]);
		return;
	}
	// If num < 0, print '-' and reverse the sign of num
	if (num < 0)
	{
		kernel->synchConsoleOut->PutChar('-');
		num = -num;
	}

	int n = 0;
	// Store num's digits in characterBuffer
	while (num)
	{
		characterBuffer[n++] = num % 10;
		num /= 10;
	}
	// Print to console
	for (int i = n - 1; i >= 0; --i)
		kernel->synchConsoleOut->PutChar(characterBuffer[i] + '0');
}

// Read and return a character from console
char ReadCharFromConsole()
{
	return kernel->synchConsoleIn->GetChar();
}

// Print a character to console
void PrintCharToConsole(char ch)
{
	kernel->synchConsoleOut->PutChar(ch);
}

// Return a random positive integer between 1 and INT32_MAX (inclusive)
int GetRandomNumber()
{
	// Call rand from stdlib to create a random number
	int num = rand();
	// GetRandomNumber must return a positive integer
	if (num == 0)
		++num;
	return num;
}

// Read and return a string from console
// Stop when reaching max length or meeting '\n'
char *ReadStringFromConsole(int len)
{
	char *str;
	str = new char[len + 1];
	for (int i = 0; i < len; ++i)
	{
		str[i] = kernel->synchConsoleIn->GetChar();
		// If str[i] = '\n' then assign str[i] = '\0' and return the string
		if (str[i] == '\n')
		{
			str[i] = '\0';
			return str;
		}
	}
	str[len] = '\0';
	return str;
}

// Print a string to console
// Stop when reaching maxLen or meeting '\0'
void PrintStringToConsole(char *str, int maxLen)
{
	int i = 0;
	while (str[i] != '\0' && i < maxLen)
	{
		kernel->synchConsoleOut->PutChar(str[i]);
		++i;
	}
}

void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
		// Xử lý các exceptions được liệt kê trong machine / machine.h
		// no exception sẽ trả quyền điều khiển về HĐH
	case NoException:
		DEBUG(dbgSys, "No exception.\n");
		return;
		// Hầu hết các exception trong này là run - time errors
		// khi các exception này xảy ra thì user program không thể được phục hồi
		// HĐH hiển thị ra một thông báo lỗi và Halt hệ thống
	case PageFaultException:
		DEBUG(dbgSys, "No valid translation found.\n");
		cerr << "No valid translation found. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case ReadOnlyException:
		DEBUG(dbgSys, "Write attempted to page marked read-only.\n");
		cerr << "Write attempted to page marked read-only. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case BusErrorException:
		DEBUG(dbgSys, "Translation resulted in an invalid physical address.\n");
		cerr << "Translation resulted in an invalid physical address. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case AddressErrorException:
		DEBUG(dbgSys, "Unaligned reference or one that was beyond the end of the address space.\n");
		cerr << "Unaligned reference or one that was beyond the end of the address space. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case OverflowException:
		DEBUG(dbgSys, "Integer overflow in add or sub.\n");
		cerr << "Integer overflow in add or sub. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case IllegalInstrException:
		DEBUG(dbgSys, "Unimplemented or reserved instr.\n");
		cerr << "Unimplemented or reserved instr. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case NumExceptionTypes:
		DEBUG(dbgSys, "NumExceptionTypes.\n");
		cerr << "NumExceptionTypes. ExceptionType " << which << '\n';
		SysHalt();

		ASSERTNOTREACHED();
		break;
	case SyscallException:
		DEBUG(dbgSys, "GO here!.\n");
		switch (type)
		{
		case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

			SysHalt();

			ASSERTNOTREACHED();
			break;
		case SC_Add:
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							/* int op2 */ (int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);

			/* Modify return point */
			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		case SC_Create:
		{
			int virtAddr;
			char *filename;
			DEBUG(dbgSys, "\n SC_Create call ...");
			DEBUG(dbgSys, "\n Reading virtual address of filename");
			// Lấy tham số tên tập tin từ thanh ghi r4
			virtAddr = kernel->machine->ReadRegister(4);
			DEBUG(dbgSys, "\n Reading filename.");
			// MaxFileLength là = 32
			filename = User2System(virtAddr, MaxFileLength + 1);
			if (filename == NULL)
			{
				printf("\n Not enough memory in system");
				DEBUG(dbgSys, "\n Not enough memory in system");
				kernel->machine->WriteRegister(2, -1); // trả về lỗi cho chương
				// trình người dùng
				delete filename;
				return;
			}
			DEBUG(dbgSys, "\n Finish reading filename.");
			// DEBUG(dbgSys,"\n File name : '"<<filename<<"'");
			//  Create file with size = 0
			//  Dùng đối tượng fileSystem của lớp OpenFile để tạo file,
			//  việc tạo file này là sử dụng các thủ tục tạo file của hệ điều
			//  hành Linux, chúng ta không quản ly trực tiếp các block trên
			//  đĩa cứng cấp phát cho file, việc quản ly các block của file
			//  trên ổ đĩa là một đồ án khác
			if (!kernel->fileSystem->Create(filename))
			{
				printf("\n Error create file '%s'", filename);
				kernel->machine->WriteRegister(2, -1);
				delete filename;
				return;
			}
			kernel->machine->WriteRegister(2, 0); // trả về cho chương trình
												  // người dùng thành công
			delete filename;
			break;
		}

		case SC_Open:
		{
			// Input: arg1: Dia chi cua chuoi name, arg2: type
			// Output: Tra ve OpenFileID neu thanh, -1 neu loi
			// Chuc nang: Tra ve ID cua file.

			// OpenFileID Open(char *name, int type)
			DEBUG(dbgSys, "GO Open!\n");
			int virtAddr = kernel->machine->ReadRegister(4); // Lay dia chi cua tham so name tu thanh ghi so 4
			int type = kernel->machine->ReadRegister(5);	 // Lay tham so type tu thanh ghi so 5
			char *filename;
			filename = User2System(virtAddr, MaxFileLength); // Copy chuoi tu vung nho User Space sang System Space voi bo dem name dai MaxFileLength
			// Kiem tra xem OS con mo dc file khong

			// update 4/1/2018
			int freeSlot = kernel->fileSystem->FindFreeSlot();
			if (freeSlot != -1) // Chi xu li khi con slot trong
			{
				if (type == 0 || type == 1) // chi xu li khi type = 0 hoac 1
				{
					DEBUG(dbgSys, "GO ENd 1!\n");
					kernel->fileSystem->openf[freeSlot] = kernel->fileSystem->Open(filename, type);
					DEBUG(dbgSys, "Check " << kernel->fileSystem->openf[freeSlot] << endl);
					if (kernel->fileSystem->openf[freeSlot] != NULL) // Mo file thanh cong
					{
						DEBUG(dbgSys, "\nOpened file");
						kernel->machine->WriteRegister(2, freeSlot); // tra ve OpenFileID
						DEBUG(dbgSys, "\n" << freeSlot);
						delete[] filename;
						increaseProgramCounter();
						return;
						ASSERTNOTREACHED();
						break;
					}
				}
				else if (type == 2) // xu li stdin voi type quy uoc la 2
				{
					DEBUG(dbgSys, "GO ENd 3!\n");
					kernel->machine->WriteRegister(2, 0); // tra ve OpenFileID
					delete[] filename;
					increaseProgramCounter();
					return;
					ASSERTNOTREACHED();
					break;
				}
				else // xu li stdout voi type quy uoc la 3
				{
					DEBUG(dbgSys, "GO ENd 4!\n");
					kernel->machine->WriteRegister(2, 1); // tra ve OpenFileID
					delete[] filename;
					increaseProgramCounter();
					return;
					ASSERTNOTREACHED();
					break;
				}
			}
			kernel->machine->WriteRegister(2, -1); // Khong mo duoc file return -1
			DEBUG(dbgSys, "GO ENd!\n");
			delete[] filename;
			increaseProgramCounter();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Close:
		{
			// int id = kernel->machine->ReadRegister(4);
			// kernel->machine->WriteRegister(2, kernel->fileSystem->Close(id));
			// increaseProgramCounter();

			// return;
			// ASSERTNOTREACHED();
			// break;

			// Input id cua file(OpenFileID)
			// Output: 0: thanh cong, -1 that bai
			int fid = kernel->machine->ReadRegister(4); // Lay id cua file tu thanh ghi so 4
			if (fid >= 0 && fid <= 14)					// Chi xu li khi fid nam trong [0, 14]
			{
				if (kernel->fileSystem->openf[fid]) // neu mo file thanh cong
				{
					delete kernel->fileSystem->openf[fid]; // Xoa vung nho luu tru file
					kernel->fileSystem->openf[fid] = NULL; // Gan vung nho NULL
					kernel->machine->WriteRegister(2, 0);
					break;
				}
			}
			DEBUG(dbgSys, "Closed file!\n");
			kernel->machine->WriteRegister(2, -1);
			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ReadNum:
		{
			// Đọc số từ console vào num
			int num = ReadNumFromConsole();
			// Ghi vào thanh ghi số 2
			kernel->machine->WriteRegister(2, num);

			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_PrintNum:
		{
			// Lấy tham số cần in từ thanh ghi r4
			int num = kernel->machine->ReadRegister(4);
			// In ra console
			PrintNumToConsole(num);

			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ReadChar:
		{
			// Đọc character từ console và ghi vào thanh ghi số 2
			char character = ReadCharFromConsole();
			kernel->machine->WriteRegister(2, (int)character);

			increaseProgramCounter();
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_PrintChar:
		{
			// Lấy tham số cần in từ thanh ghi r4
			char character = (char)kernel->machine->ReadRegister(4);
			// In ra console
			PrintCharToConsole(character);

			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ReadString:
		{
			// Đọc địa chỉ string từ thanh ghi r4
			int userString = kernel->machine->ReadRegister(4);
			// Đọc chiều dài string từ thanh ghi r5
			int len = kernel->machine->ReadRegister(5);

			if (len > MaxStringLength || len < 1)
			{
				DEBUG(dbgSys, "String length must be between 1 and " << MaxStringLength << " (inclusive)\n");
				SysHalt();
			}
			DEBUG(dbgSys, "String length: " << len);

			char *systemString = ReadStringFromConsole(len);
			// Chuyển dữ liệu từ kernel space qua userspace
			System2User(userString, MaxStringLength, systemString);
			delete[] systemString;

			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_PrintString:
		{
			// Đọc địa chỉ string từ thanh ghi r4
			int userString = kernel->machine->ReadRegister(4);

			// Chuyển dữ liệu từ userspace qua kernelspace
			char *systemString = User2System(userString, MaxStringLength);

			// In ra console
			PrintStringToConsole(systemString, MaxStringLength);
			delete[] systemString;

			increaseProgramCounter();

			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Read:
		{
			DEBUG(dbgSys, "Go read file!\n");
			int virtAddr = kernel->machine->ReadRegister(4);
			int charcount = kernel->machine->ReadRegister(5);
			OpenFileId ID = kernel->machine->ReadRegister(6);

			int bytesSysRead = -1;
			DEBUG(dbgSys, "Read " << charcount << " chars from file " << ID << "\n");

			if ((ID < 0) || (ID > 19))
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: ID is not in table file descriptor\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			if (kernel->fileSystem->openf[ID] == NULL)
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: File is not existed\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			if (ID == SysConsoleOutput)
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: ConsoleOutput\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			int oldPos = kernel->fileSystem->openf[ID]->GetCurrentPos(); // Lay vi tri hien tai
			char *buff;
			buff = User2System(virtAddr, charcount); // Copy chuoi tu vung nho User Space sang System Space
			if (ID == SysConsoleInput)				 // nguoi dung nhap tu console
			{
				bytesSysRead = kernel->synchConsoleIn->GetString(buff, charcount); // so bytes thuc su doc duoc
			}

			// Neu doc duoc file binh thuong
			if (kernel->fileSystem->openf[ID]->Read(buff, charcount) > 0)
			{
				// so bytes doc duoc
				int newPos = kernel->fileSystem->openf[ID]->GetCurrentPos();
				bytesSysRead = abs(oldPos - newPos);
			}

			kernel->machine->WriteRegister(2, bytesSysRead);

			DEBUG(dbgSys, "BytesSysRead " << bytesSysRead << "\n");
			System2User(virtAddr, charcount, buff); // Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer dai charCount
			delete buff;

			increaseProgramCounter();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Write:
		{
			DEBUG(dbgSys, "Go write file:\n");
			int virtAddr = kernel->machine->ReadRegister(4);
			int charcount = kernel->machine->ReadRegister(5);
			int ID = kernel->machine->ReadRegister(6);

			int bytesWrite = -1;
			DEBUG(dbgSys, "Write " << charcount << " chars to file " << ID << "\n");

			if ((ID < 0) || (ID > 19))
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: ID is not in table file descriptor\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			if (kernel->fileSystem->openf[ID] == NULL)
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: File is not existed\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			if (ID == SysConsoleInput)
			{
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "E: ConsoleInput\n");
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			int oldPos = kernel->fileSystem->openf[ID]->GetCurrentPos(); // Lay vi tri hien tai
			char *buff;
			buff = User2System(virtAddr, charcount); // Copy chuoi tu vung nho User Space sang System Space
			if (ID == SysConsoleOutput)				 // ghi ra console
			{
				bytesWrite = kernel->synchConsoleOut->PutString(buff, charcount); // so bytes thuc su ghi duoc
			}

			// Neu ghi duoc file binh thuong
			if (kernel->fileSystem->openf[ID]->Write(buff, charcount) > 0)
			{
				// so bytes doc duoc
				int newPos = kernel->fileSystem->openf[ID]->GetCurrentPos();
				bytesWrite = oldPos - newPos;
			}

			kernel->machine->WriteRegister(2, bytesWrite);

			System2User(virtAddr, charcount, buff); // Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer dai charCount
			delete buff;

			increaseProgramCounter();
			DEBUG(dbgSys, "\nFile is written!\n");
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_Seek:
		{
			DEBUG(dbgSys, "Go Seek:\n");
			int Pos = kernel->machine->ReadRegister(4);
			int ID = kernel->machine->ReadRegister(5);

			if ((ID == SysConsoleInput) || (ID == SysConsoleOutput) // ID la 0 hoac 1
				|| kernel->fileSystem->openf[ID] == NULL)			// Chua co trong table
			{
				DEBUG(dbgSys, "Can not Seek in console\n");
				kernel->machine->WriteRegister(2, -1);
				increaseProgramCounter();
				return;
				ASSERTNOTREACHED();
				break;
			}

			if (Pos == -1) // tro ve cuoi file
				Pos = kernel->fileSystem->openf[ID]->Length();

			// kiem tra vi tri pos co hop le
			if (Pos < 0 || Pos > kernel->fileSystem->openf[ID]->Length())
			{
				DEBUG(dbgSys, "Can not Seek\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else
			{
				kernel->fileSystem->openf[ID]->Seek(Pos);
				kernel->machine->WriteRegister(2, Pos);
			}

			increaseProgramCounter();
			return;
			ASSERTNOTREACHED();
			break;
		}

		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}
		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
	DEBUG(dbgSys, "hehe\n");
	ASSERTNOTREACHED();
}