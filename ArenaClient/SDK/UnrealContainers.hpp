#pragma once

#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>

namespace UC
{

template<typename ArrayType>
class TArray
{
public:
    ArrayType* Data;
    int32_t Count;
    int32_t Max;

public:
    TArray() : Data(nullptr), Count(0), Max(0) {}

    int32_t Num() const { return Count; }

    ArrayType& operator[](int32_t i) { return Data[i]; }
    const ArrayType& operator[](int32_t i) const { return Data[i]; }

    ArrayType& Add(const ArrayType& Item)
    {
        if (Count >= Max)
        {
            int32_t NewMax = (Max == 0) ? 4 : Max * 2;
            ArrayType* NewData = new ArrayType[NewMax];
            if (Data)
            {
                for (int32_t i = 0; i < Count; ++i)
                    NewData[i] = std::move(Data[i]);
                delete[] Data;
            }
            Data = NewData;
            Max = NewMax;
        }
        Data[Count] = Item;
        return Data[Count++];
    }
};

class FString : public TArray<wchar_t>
{
public:
    FString() = default;

    FString(const wchar_t* Str)
    {
        int32_t Len = static_cast<int32_t>(wcslen(Str));
        if (Len == 0) return;
        Data = new wchar_t[Len + 1];
        Count = Len;
        Max = Len + 1;
        memcpy(Data, Str, (Len + 1) * sizeof(wchar_t));
    }

    const wchar_t* ToCString() const { return Data ? Data : L""; }
    std::wstring ToWString() const { return Data ? std::wstring(Data) : std::wstring(); }
    std::string ToString() const
    {
        if (!Data) return {};
        int Len = WideCharToMultiByte(CP_UTF8, 0, Data, Count, nullptr, 0, nullptr, nullptr);
        std::string Result(Len, 0);
        WideCharToMultiByte(CP_UTF8, 0, Data, Count, Result.data(), Len, nullptr, nullptr);
        return Result;
    }

    operator const wchar_t*() const { return ToCString(); }
};

using UEAllocatedString = std::string;
using UEAllocatedWString = std::wstring;

class FAllocatedString : public FString
{
public:
    FAllocatedString(int32_t Size = 1024)
    {
        Data = new wchar_t[Size];
        Count = 0;
        Max = Size;
        Data[0] = 0;
    }

    ~FAllocatedString() { delete[] Data; }

    void Clear()
    {
        Count = 0;
        if (Data) Data[0] = 0;
    }

    std::string ToString() const
    {
        if (!Data) return {};
        int Len = WideCharToMultiByte(CP_UTF8, 0, Data, Count, nullptr, 0, nullptr, nullptr);
        std::string Result(Len, 0);
        WideCharToMultiByte(CP_UTF8, 0, Data, Count, Result.data(), Len, nullptr, nullptr);
        return Result;
    }

    std::wstring ToWString() const { return Data ? std::wstring(Data) : std::wstring(); }
};

}
