#pragma once
#include "pch.h"
#include "Scan.h"

namespace Cookie
{
    inline uint64_t FMemory__Realloc = 0;

    struct FMemory
    {
        static void* InternalRealloc(void* ptr, int64_t size, uint32_t align)
        {
            return ((void* (*)(void*, int64_t, uint32_t))FMemory__Realloc)(ptr, size, align);
        }

        template<typename T = void>
        static T* Realloc(void* ptr, uint64_t size, int32_t alignment = 0)
        {
            return (T*)InternalRealloc(ptr, size, alignment);
        }

        template<typename T>
        static T* ReallocForType(void* ptr, uint64_t count, int32_t size = sizeof(T))
        {
            return (T*)InternalRealloc(ptr, count * size, alignof(T));
        }

        template<typename T = void>
        static T* Malloc(uint64_t size, int32_t alignment = 0)
        {
            return Realloc<T>(nullptr, size, alignment);
        }

        template<typename T>
        static T* MallocForType(uint64_t count, int32_t size = sizeof(T))
        {
            return ReallocForType<T>(nullptr, count, size);
        }

        static void Free(void* ptr)
        {
            Realloc(ptr, 0, 0);
        }

        template<typename T>
        static void FreeForType(T* ptr)
        {
            ReallocForType<T>(ptr, 0);
        }
    };

    struct FString
    {
        wchar_t* Buffer;
        uint32_t Count;
        uint32_t Capacity;
        static const size_t NotFound = -1;

        FString()
        {
            Buffer = nullptr;
            Count = Capacity = 0;
        }

        FString(const char* input)
        {
            if (input)
            {
                Capacity = Count = (uint32_t)strlen(input) + 1;
                Buffer = FMemory::MallocForType<wchar_t>(Count);
                size_t conv = 0;
                mbstowcs_s(&conv, Buffer, Count, input, _TRUNCATE);
            }
            else
            {
                Buffer = nullptr;
                Count = Capacity = 0;
            }
        }

        FString(wchar_t* input)
        {
            if (input)
            {
                Capacity = Count = (uint32_t)wcslen(input) + 1;
                Buffer = FMemory::MallocForType<wchar_t>(Count);
                __movsb((PBYTE)Buffer, (const PBYTE)input, Count * 2);
            }
            else
            {
                Buffer = nullptr;
                Count = Capacity = 0;
            }
        }

        FString(const wchar_t* input)
        {
            if (input)
            {
                Capacity = Count = (uint32_t)wcslen(input) + 1;
                Buffer = FMemory::MallocForType<wchar_t>(Count);
                __movsb((PBYTE)Buffer, (const PBYTE)input, Count * 2);
            }
            else
            {
                Buffer = nullptr;
                Count = Capacity = 0;
            }
        }

        FString(uint32_t len)
        {
            Capacity = Count = len + 1;
            Buffer = FMemory::MallocForType<wchar_t>(Count);
        }

        FString& operator=(const FString& other)
        {
            if (this != &other)
            {
                if (Buffer) FMemory::Free(Buffer);
                if (other.Buffer)
                {
                    Count = Capacity = other.Count;
                    Buffer = FMemory::MallocForType<wchar_t>(Count);
                    __movsb((PBYTE)Buffer, (const PBYTE)other.Buffer, Count * 2);
                }
                else
                {
                    Buffer = nullptr;
                    Count = Capacity = 0;
                }
            }
            return *this;
        }

        FString Substr(size_t offset, size_t count = -1)
        {
            if (count == -1)
                count = (size_t)Count - offset - 1;
            else if (count > (size_t)Count)
                count = (size_t)Count - offset;

            FString n((uint32_t)count);
            __movsb((PBYTE)n.Buffer, (const PBYTE)(Buffer + offset), count * 2);
            n.Buffer[count] = 0;
            return n;
        }

        size_t Find(wchar_t ch) const
        {
            for (uint32_t i = 0; i < Count; i++)
                if (Buffer[i] == ch)
                    return i;
            return NotFound;
        }

        size_t Find(const wchar_t* sub) const
        {
            auto subLen = wcslen(sub);
            for (uint32_t i = 0; i + subLen <= (size_t)Count; i++)
            {
                bool match = true;
                for (size_t j = 0; j < subLen; j++)
                {
                    if (Buffer[i + j] != sub[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match) return i;
            }
            return NotFound;
        }

        bool Contains(wchar_t ch) const
        {
            return Find(ch) != NotFound;
        }

        bool Contains(const wchar_t* sub) const
        {
            return Find(sub) != NotFound;
        }

        bool StartsWith(const wchar_t* text) const
        {
            auto len = wcslen(text);
            for (size_t i = 0; i < len; i++)
                if (Buffer[i] != text[i])
                    return false;
            return true;
        }

        bool EndsWith(const wchar_t* text) const
        {
            auto len = wcslen(text);
            auto start = (size_t)Count - 1 - len;
            for (size_t i = 0; i < len; i++)
                if (Buffer[start + i] != text[i])
                    return false;
            return true;
        }

        size_t FindFirst(wchar_t ch) const { return Find(ch); }

        void Dealloc()
        {
            if (Buffer) FMemory::Free(Buffer);
            Buffer = nullptr;
            Count = Capacity = 0;
        }
    };

    struct FCurlHttpRequest
    {
        void** VTable;
        static inline int64_t SetURLIdx;

        void InitializeURLIndex();
        FString GetURL();
        void RedirectRequest(bool bEOS);
    };

    extern bool (*ProcessRequestOG)(FCurlHttpRequest*);
    extern bool (*ProcessRequestOG_EOS)(FCurlHttpRequest*);
    extern int g_MemLeakCount;
}
