#include "includes.h"

Memory::Memory()
	:
	read(*this),
	write(*this),
	pattern(*this)
{
}

//Blocking constructor...
Memory::Memory(wchar_t * GameName)
	:
	read(*this),
	write(*this),
	pattern(*this)
{
	this->wcsGameName = GameName;
	Init();
}

Memory::~Memory()
{
}

void Memory::Init()
{
	if (wcsGameName)
	{
		while (pID == NULL)
			GetProcID();

		if (!OpenProc())
		{
			std::cerr << "ERROR: Could not open process." << std::endl;
		}
	}
	else
	{
		std::cerr << "ERROR: Game name has not been specified." << std::endl;
	}
}

void Memory::Init(wchar_t * GameName)
{
	this->wcsGameName = GameName;
	Init();
}

void Memory::GetProcID()
{
	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Process32First(hSnap, &pe))
	{
		if (!wcscmp(pe.szExeFile, wcsGameName))
		{
			this->pID = pe.th32ProcessID;
		}
		else
		{
			while (Process32Next(hSnap, &pe))
			{
				if (!wcscmp(pe.szExeFile, wcsGameName))
				{
					this->pID = pe.th32ProcessID;
					break;
				}
			}
		}
	}
}

bool Memory::OpenProc()
{
	if (pID == NULL)
	{
		return false;
	}
	else
	{
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
		if (!hProcess)
			return false;
	}
	return true;
}

uintptr_t Memory::EvaluatePointer(uintptr_t pBase, UINT * offsets, UCHAR count)
{
	uintptr_t evAddress;

	if (ReadProcessMemory(hProcess, (void*)pBase, &evAddress, sizeof(uintptr_t), nullptr))
	{
		if (count > 1)
		{
			for (UCHAR x = 0; x < count - 1; x++)
			{
				if (!ReadProcessMemory(hProcess, (void*)(evAddress + offsets[x]), &evAddress, sizeof(uintptr_t), nullptr))
					return 0;
			}
		}
	}
	else
		return 0;

	return (evAddress + offsets[count - 1]);
}

Memory::Read::Read(Memory & mem)
	:
	mem(mem)
{
}

BOOL Memory::Read::ReadInt(uintptr_t pAddress, int & pReadBuff)
{
	return ReadProcessMemory(mem.hProcess, (void*)pAddress, &pReadBuff, sizeof(int), nullptr);
}

BOOL Memory::Read::ReadInt64(uintptr_t pAddress, int64_t & pReadBuff)
{
	return ReadProcessMemory(mem.hProcess, (void*)pAddress, &pReadBuff, sizeof(int64_t), nullptr);
}

BOOL Memory::Read::ReadFloat(uintptr_t pAddress, float & pReadBuff)
{
	return ReadProcessMemory(mem.hProcess, (void*)pAddress, &pReadBuff, sizeof(float), nullptr);
}

BOOL Memory::Read::ReadDouble(uintptr_t pAddress, double & pReadBuff)
{
	return ReadProcessMemory(mem.hProcess, (void*)pAddress, &pReadBuff, sizeof(double), nullptr);
}

BOOL Memory::Read::ReadBytes(uintptr_t pAddress, byte * pReadBuff, SIZE_T szSize)
{
	return ReadProcessMemory(mem.hProcess, (void*)pAddress, pReadBuff, szSize, nullptr);
}

Memory::Write::Write(Memory & mem)
	:
	mem(mem)
{	
}

BOOL Memory::Write::WriteInt(uintptr_t pAddress, int & pWriteBuff)
{
	return WriteProcessMemory(mem.hProcess, (void*)pAddress, &pWriteBuff, sizeof(int), nullptr);
}
		
BOOL Memory::Write::WriteInt64(uintptr_t pAddress, int64_t & pWriteBuff)
{
	return WriteProcessMemory(mem.hProcess, (void*)pAddress, &pWriteBuff, sizeof(int64_t), nullptr);
}
			
BOOL Memory::Write::WriteFloat(uintptr_t pAddress, float & pWriteBuff)
{
	return WriteProcessMemory(mem.hProcess, (void*)pAddress, &pWriteBuff, sizeof(float), nullptr);
}

BOOL Memory::Write::WriteDouble(uintptr_t pAddress, double & pWriteBuff)
{
	return WriteProcessMemory(mem.hProcess, (void*)pAddress, &pWriteBuff, sizeof(double), nullptr);
}

BOOL Memory::Write::WriteBytes(uintptr_t pAddress, byte * pByteArray, SIZE_T szSize)
{
	if (pByteArray == nullptr)
	{
		byte * nops = new byte[szSize];
		for (UINT x = 0; x < szSize; x++)
			nops[x] = 0x90;

		return WriteProcessMemory(mem.hProcess, (void*)pAddress, nops, szSize, nullptr);
	}
	else
		return WriteProcessMemory(mem.hProcess, (void*)pAddress, pByteArray, szSize, nullptr);
}

Memory::Pattern::Pattern(Memory & mem)
	:
	mem(mem)
{
}

bool Memory::Pattern::CheckPattern(char * bArray, char * pattern, char * mask, UINT szSize, UINT & patternOffset)
{
	UINT pLen = strlen(mask);
	for (UINT x = 0; x < szSize; x++)
	{
		bool bFound = true;
		for (UINT y = 0; y < pLen; y++)
		{
			if (mask[y] != '?' && pattern[y] != bArray[x + y])
			{
				bFound = false;
				break;
			}
		}
		if (bFound)
		{
			patternOffset = x;
			return true;
		}
	}

	return false;
}

uintptr_t Memory::Pattern::Scan(uintptr_t pStart, UINT uiBegin, UINT uiEnd, char * pattern, char * mask)
{
	UINT patternOffset = 0;
	char * bArray;
	bool bFound = false;
	while (uiBegin < uiEnd)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQueryEx(mem.hProcess, (void*)uiBegin, &mbi, sizeof(MEMORY_BASIC_INFORMATION)))
			return 0;

		bArray = new char[mbi.RegionSize];

		DWORD dwOldProtection;

		if (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS)
		{
			VirtualProtectEx(mem.hProcess, (void*)mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &dwOldProtection);
			if (!mem.read.ReadBytes((uintptr_t)mbi.BaseAddress, (byte*)bArray, mbi.RegionSize))
				std::cerr << "Failed to read memory" << std::endl;
			VirtualProtectEx(mem.hProcess, (void*)mbi.BaseAddress, mbi.RegionSize, dwOldProtection, nullptr);
			if (CheckPattern(bArray, pattern, mask, mbi.RegionSize, patternOffset))
			{
				bFound = true;
				break;
			}
		}
		uiBegin += mbi.RegionSize;		
	}
	delete[] bArray;
	if (!bFound) return uintptr_t(0);
	return uintptr_t(uiBegin + patternOffset);
}
