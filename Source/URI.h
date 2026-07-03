#pragma once
#include "Engine.h"

namespace Cookie
{
    struct URL
    {
        FString Scheme;
        FString Separator;
        FString Host;
        FString Port;
        FString Path;
        FString Query;

        URL(FString& raw)
        {
            auto protoEnd = raw.Find(L':');
            Scheme = raw.Substr(0, protoEnd);

            int sepLen = (raw.Buffer[protoEnd + 1] == '/' && raw.Buffer[protoEnd + 2] == '/') ? 3 : 1;
            Separator = raw.Substr(protoEnd, sepLen);

            auto domainPortStart = raw.Substr(protoEnd + sepLen);
            auto pathEnd = domainPortStart.FindFirst(L'/');
            auto domainPort = domainPortStart.Substr(0, pathEnd);
            auto pathStart = domainPortStart.Substr(pathEnd);
            domainPortStart.Dealloc();

            auto portOff = domainPort.FindFirst(L':');
            Host = domainPort.Substr(0, portOff);
            if (portOff != FString::NotFound)
                Port = domainPort.Substr(portOff);
            domainPort.Dealloc();

            auto queryOff = pathStart.FindFirst(L'?');
            Path = pathStart.Substr(0, queryOff);
            if (queryOff != FString::NotFound)
                Query = pathStart.Substr(queryOff);
            pathStart.Dealloc();
        }

        void SetHost(FString& host)
        {
            auto protoEnd = host.Find(L':');
            Scheme = host.Substr(0, protoEnd);

            int sepLen = (host.Buffer[protoEnd + 1] == '/' && host.Buffer[protoEnd + 2] == '/') ? 3 : 1;
            Separator.Dealloc();
            Separator = host.Substr(protoEnd, sepLen);

            auto domainPortStart = host.Substr(protoEnd + sepLen);
            auto pathEnd = domainPortStart.FindFirst(L'/');
            auto domainPort = domainPortStart.Substr(0, pathEnd);
            domainPortStart.Dealloc();

            auto portOff = domainPort.FindFirst(L':');
            Host.Dealloc();
            Host = domainPort.Substr(0, portOff);
            if (portOff != FString::NotFound)
            {
                Port.Dealloc();
                Port = domainPort.Substr(portOff);
            }
            domainPort.Dealloc();
        }

        FString Build()
        {
            int totalLen = (int)(Scheme.Count - 1) + (int)(Separator.Count - 1) + (int)(Host.Count - 1)
                + (Port.Buffer ? (int)(Port.Count - 1) : 0) + (int)(Path.Count - 1)
                + (Query.Buffer ? (int)(Query.Count - 1) : 0);
            FString out((uint32_t)totalLen);

            __movsb((PBYTE)out.Buffer, (const PBYTE)Scheme.Buffer, Scheme.Count * 2);
            __movsb((PBYTE)(out.Buffer + wcslen(out.Buffer)), (const PBYTE)Separator.Buffer, Separator.Count * 2);
            __movsb((PBYTE)(out.Buffer + wcslen(out.Buffer)), (const PBYTE)Host.Buffer, Host.Count * 2);
            if (Port.Buffer)
                __movsb((PBYTE)(out.Buffer + wcslen(out.Buffer)), (const PBYTE)Port.Buffer, Port.Count * 2);
            __movsb((PBYTE)(out.Buffer + wcslen(out.Buffer)), (const PBYTE)Path.Buffer, Path.Count * 2);
            if (Query.Buffer)
                __movsb((PBYTE)(out.Buffer + wcslen(out.Buffer)), (const PBYTE)Query.Buffer, Query.Count * 2);

            return out;
        }

        void DeallocPathQuery()
        {
            Path.Dealloc();
            Query.Dealloc();
        }

        void Dealloc()
        {
            Scheme.Dealloc();
            Separator.Dealloc();
            Host.Dealloc();
            Port.Dealloc();
            Path.Dealloc();
            Query.Dealloc();
        }
    };

    namespace Filter
    {
        bool Check(URL& uri);
    }
}
