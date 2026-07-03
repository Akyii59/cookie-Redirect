#pragma once
#include "pch.h"

namespace Cookie
{
    namespace PE
    {
        inline uint64_t ImageBase = *(uint64_t*)(__readgsqword(0x60) + 0x10);

        inline IMAGE_SECTION_HEADER* GetSection(const char* name)
        {
            auto dos = (IMAGE_DOS_HEADER*)ImageBase;
            auto nt = (IMAGE_NT_HEADERS*)(ImageBase + dos->e_lfanew);
            auto first = IMAGE_FIRST_SECTION(nt);

            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
            {
                auto sect = &first[i];
                if (strncmp((const char*)sect->Name, name, 8) == 0)
                    return sect;
            }
            return nullptr;
        }

        inline uint64_t GetBase(const wchar_t* mod)
        {
            if (mod) return (uint64_t)GetModuleHandleW(mod);
            return ImageBase;
        }
    }

    namespace Patch
    {
        inline uint64_t FindPattern(const uint8_t* bytes, const char* mask, uint32_t size, bool inRdata = false)
        {
            auto sect = PE::GetSection(inRdata ? ".rdata" : ".text");
            if (!sect) return 0;

            auto start = (uint8_t*)(PE::ImageBase + sect->VirtualAddress);
            auto len = sect->Misc.VirtualSize;
            auto maskLen = strlen(mask);

            for (uint32_t i = 0; i <= len - size; i++)
            {
                bool match = true;
                for (uint32_t j = 0; j < size; j++)
                {
                    if (mask[j] == 'x' && start[i + j] != bytes[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return (uint64_t)(start + i);
            }
            return 0;
        }

        inline uint64_t FindStringRef(const void* str, size_t strLen)
        {
            auto textSect = PE::GetSection(".text");
            auto rdataSect = PE::GetSection(".rdata");
            if (!textSect || !rdataSect) return 0;

            auto textStart = (uint8_t*)(PE::ImageBase + textSect->VirtualAddress);
            auto textSize = textSect->Misc.VirtualSize;
            auto rdataStart = (uint8_t*)(PE::ImageBase + rdataSect->VirtualAddress);
            auto rdataEnd = rdataStart + rdataSect->Misc.VirtualSize;

            __m128i t = _mm_set1_epi8((char)0x8D);

            for (uint32_t i = 0; i < textSize - (textSize % 16); i += 16)
            {
                auto data = _mm_load_si128((const __m128i*)(textStart + i));
                int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(data, t));
                if (!mask) continue;

                for (int q = 0; q < 16; q++)
                {
                    if (!(mask & (1 << q))) continue;

                    if ((textStart[i + q - 1] & 0xFB) == 0x48)
                    {
                        auto target = (uint64_t)(&textStart[i + q] + 6) + *(int32_t*)(&textStart[i + q] + 2);
                        if (target >= (uint64_t)rdataStart && target < (uint64_t)rdataEnd)
                        {
                            if (memcmp(str, (void*)target, strLen) == 0)
                                return (uint64_t)(&textStart[i + q - 1]);
                        }
                    }
                }
            }
            return 0;
        }

        inline uint64_t FindStringRef(const wchar_t* str)
        {
            return FindStringRef(str, (wcslen(str) + 1) * 2);
        }

        inline uint64_t FindStringRef(const char* str)
        {
            return FindStringRef(str, strlen(str) + 1);
        }

        inline bool CheckBytes(uint64_t base, int ind, const uint8_t* bytes, size_t count, bool upwards = false)
        {
            auto addr = (uint8_t*)(upwards ? base - ind : base + ind);
            for (size_t i = 0; i < count; i++)
                if (addr[i] != bytes[i])
                    return false;
            return true;
        }

        inline bool CheckBytes(uint64_t base, int ind, std::initializer_list<uint8_t> bytes, bool upwards = false)
        {
            auto addr = (uint8_t*)(upwards ? base - ind : base + ind);
            size_t i = 0;
            for (auto b : bytes)
                if (addr[i++] != b)
                    return false;
            return true;
        }
    }
}
