/**
 @file stackinfo.cpp

 @brief Implements the stackinfo class.
 */

#include "stackinfo.h"
#include "memory.h"
#include "disasm_helper.h"
#include "disasm_fast.h"
#include "_exports.h"
#include "module.h"

bool stackcommentget(uint addr, STACK_COMMENT* comment)
{
    uint data = 0;
    memset(comment, 0, sizeof(STACK_COMMENT));
    MemRead((void*)addr, &data, sizeof(uint), 0);
    if(!MemIsValidReadPtr(data)) //the stack value is no pointer
        return false;

    uint size = 0;
    uint base = MemFindBaseAddr(data, &size);
    uint readStart = data - 16 * 4;
    if(readStart < base)
        readStart = base;
    unsigned char disasmData[256];
    MemRead((void*)readStart, disasmData, sizeof(disasmData), 0);
    uint prev = disasmback(disasmData, 0, sizeof(disasmData), data - readStart, 1);
    uint previousInstr = readStart + prev;

    BASIC_INSTRUCTION_INFO basicinfo;
    bool valid = disasmfast(disasmData + prev, previousInstr, &basicinfo);
    if(valid && basicinfo.call) //call
    {
        char label[MAX_LABEL_SIZE] = "";
        ADDRINFO addrinfo;
        addrinfo.flags = flaglabel;
        if(_dbg_addrinfoget(data, SEG_DEFAULT, &addrinfo))
            strcpy_s(label, addrinfo.label);
        char module[MAX_MODULE_SIZE] = "";
        ModNameFromAddr(data, module, false);
        char returnToAddr[MAX_COMMENT_SIZE] = "";
        if(*module)
            sprintf(returnToAddr, "%s.", module);
        if(!*label)
            sprintf(label, fhex, data);
        strcat(returnToAddr, label);

        data = basicinfo.addr;
        if(data)
        {
            *label = 0;
            addrinfo.flags = flaglabel;
            if(_dbg_addrinfoget(data, SEG_DEFAULT, &addrinfo))
                strcpy_s(label, addrinfo.label);
            *module = 0;
            ModNameFromAddr(data, module, false);
            char returnFromAddr[MAX_COMMENT_SIZE] = "";
            if(*module)
                sprintf(returnFromAddr, "%s.", module);
            if(!*label)
                sprintf(label, fhex, data);
            strcat_s(returnFromAddr, label);
            sprintf_s(comment->comment, "return to %s from %s", returnToAddr, returnFromAddr);
        }
        else
            sprintf_s(comment->comment, "return to %s from ???", returnToAddr);
        strcpy_s(comment->color, "#ff0000");
        return true;
    }

    //string
    STRING_TYPE strtype;
    char string[512] = "";
    if(disasmgetstringat(data, &strtype, string, string, 500))
    {
        if(strtype == str_ascii)
            sprintf(comment->comment, "\"%s\"", string);
        else //unicode
            sprintf(comment->comment, "L\"%s\"", string);
        return true;
    }

    //label
    char label[MAX_LABEL_SIZE] = "";
    ADDRINFO addrinfo;
    addrinfo.flags = flaglabel;
    if(_dbg_addrinfoget(data, SEG_DEFAULT, &addrinfo))
        strcpy_s(label, addrinfo.label);
    char module[MAX_MODULE_SIZE] = "";
    ModNameFromAddr(data, module, false);

    if(*module) //module
    {
        if(*label) //+label
            sprintf(comment->comment, "%s.%s", module, label);
        else //module only
            sprintf(comment->comment, "%s."fhex, module, data);
        return true;
    }
    else if(*label) //label only
    {
        sprintf(comment->comment, "<%s>", label);
        return true;
    }

    return false;
}
#include "console.h"
void stackgetcallstack(uint csp, CALLSTACK* callstack)
{
    callstack->total = 0;
    if(!DbgIsDebugging() || csp % sizeof(uint)) //alignment problem
        return;
    if(!MemIsValidReadPtr(csp))
        return;
    std::vector<CALLSTACKENTRY> callstackVector;
    DWORD ticks = GetTickCount();
    uint stacksize = 0;
    uint stackbase = MemFindBaseAddr(csp, &stacksize, false);
    if(!stackbase) //super-fail (invalid stack address)
        return;
    //walk up the stack
    uint i = csp;
    while(i != stackbase + stacksize)
    {
        uint data = 0;
        MemRead((void*)i, &data, sizeof(uint), 0);
        if(MemIsValidReadPtr(data)) //the stack value is a pointer
        {
            uint size = 0;
            uint base = MemFindBaseAddr(data, &size);
            uint readStart = data - 16 * 4;
            if(readStart < base)
                readStart = base;
            unsigned char disasmData[256];
            MemRead((void*)readStart, disasmData, sizeof(disasmData), 0);
            uint prev = disasmback(disasmData, 0, sizeof(disasmData), data - readStart, 1);
            uint previousInstr = readStart + prev;
            BASIC_INSTRUCTION_INFO basicinfo;
            bool valid = disasmfast(disasmData + prev, previousInstr, &basicinfo);
            if(valid && basicinfo.call) //call
            {
                char label[MAX_LABEL_SIZE] = "";
                ADDRINFO addrinfo;
                addrinfo.flags = flaglabel;
                if(_dbg_addrinfoget(data, SEG_DEFAULT, &addrinfo))
                    strcpy_s(label, addrinfo.label);
                char module[MAX_MODULE_SIZE] = "";
                ModNameFromAddr(data, module, false);
                char returnToAddr[MAX_COMMENT_SIZE] = "";
                if(*module)
                    sprintf(returnToAddr, "%s.", module);
                if(!*label)
                    sprintf(label, fhex, data);
                strcat(returnToAddr, label);

                CALLSTACKENTRY curEntry;
                memset(&curEntry, 0, sizeof(CALLSTACKENTRY));
                curEntry.addr = i;
                curEntry.to = data;
                curEntry.from = basicinfo.addr;

                data = basicinfo.addr;

                if(data)
                {
                    *label = 0;
                    addrinfo.flags = flaglabel;
                    if(_dbg_addrinfoget(data, SEG_DEFAULT, &addrinfo))
                        strcpy_s(label, addrinfo.label);
                    *module = 0;
                    ModNameFromAddr(data, module, false);
                    char returnFromAddr[MAX_COMMENT_SIZE] = "";
                    if(*module)
                        sprintf(returnFromAddr, "%s.", module);
                    if(!*label)
                        sprintf(label, fhex, data);
                    strcat(returnFromAddr, label);

                    sprintf_s(curEntry.comment, "return to %s from %s", returnToAddr, returnFromAddr);
                }
                else
                    sprintf_s(curEntry.comment, "return to %s from ???", returnToAddr);
                callstackVector.push_back(curEntry);
            }
        }
        i += sizeof(uint);
    }
    callstack->total = (int)callstackVector.size();
    if(callstack->total)
    {
        callstack->entries = (CALLSTACKENTRY*)BridgeAlloc(callstack->total * sizeof(CALLSTACKENTRY));
        for(int i = 0; i < callstack->total; i++)
        {
            //CALLSTACKENTRY curEntry;
            //memcpy(&curEntry, &callstackVector.at(i), sizeof(CALLSTACKENTRY));
            //dprintf(fhex":"fhex":"fhex":%s\n", curEntry.addr, curEntry.to, curEntry.from, curEntry.comment);
            memcpy(&callstack->entries[i], &callstackVector.at(i), sizeof(CALLSTACKENTRY));
        }
    }
}