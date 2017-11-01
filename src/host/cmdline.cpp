/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/
#include "precomp.h"

#include "cmdline.h"

#include "_output.h"
#include "output.h"
#include "stream.h"
#include "_stream.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "../types/inc/convert.hpp"
#include "srvinit.h"
#include "resource.h"

#include "ApiRoutines.h"
#include "KeyEventHelpers.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

#define COPY_TO_CHAR_PROMPT_LENGTH 26
#define COPY_FROM_CHAR_PROMPT_LENGTH 28

#define COMMAND_NUMBER_PROMPT_LENGTH 22
#define COMMAND_NUMBER_LENGTH 5
#define MINIMUM_COMMAND_PROMPT_SIZE COMMAND_NUMBER_LENGTH

#define COMMAND_NUM_TO_INDEX(NUM, CMDHIST) (SHORT)(((NUM+(CMDHIST)->FirstCommand)%((CMDHIST)->MaximumNumberOfCommands)))
#define COMMAND_INDEX_TO_NUM(INDEX, CMDHIST) (SHORT)(((INDEX+((CMDHIST)->MaximumNumberOfCommands)-(CMDHIST)->FirstCommand)%((CMDHIST)->MaximumNumberOfCommands)))

#define POPUP_SIZE_X(POPUP) (SHORT)(((POPUP)->Region.Right - (POPUP)->Region.Left - 1))
#define POPUP_SIZE_Y(POPUP) (SHORT)(((POPUP)->Region.Bottom - (POPUP)->Region.Top - 1))
#define COMMAND_NUMBER_SIZE 8   // size of command number buffer


// COMMAND_IND_NEXT and COMMAND_IND_PREV go to the next and prev command
// COMMAND_IND_INC  and COMMAND_IND_DEC  go to the next and prev slots
//
// Don't get the two confused - it matters when the cmd history is not full!
#define COMMAND_IND_PREV(IND, CMDHIST)               \
{                                                    \
    if (IND <= 0) {                                  \
        IND = (CMDHIST)->NumberOfCommands;           \
    }                                                \
    IND--;                                           \
}

#define COMMAND_IND_NEXT(IND, CMDHIST)               \
{                                                    \
    ++IND;                                           \
    if (IND >= (CMDHIST)->NumberOfCommands) {        \
        IND = 0;                                     \
    }                                                \
}

#define COMMAND_IND_DEC(IND, CMDHIST)                \
{                                                    \
    if (IND <= 0) {                                  \
        IND = (CMDHIST)->MaximumNumberOfCommands;    \
    }                                                \
    IND--;                                           \
}

#define COMMAND_IND_INC(IND, CMDHIST)                \
{                                                    \
    ++IND;                                           \
    if (IND >= (CMDHIST)->MaximumNumberOfCommands) { \
        IND = 0;                                     \
    }                                                \
}

#define FMCFL_EXACT_MATCH   1
#define FMCFL_JUST_LOOKING  2

#define UCLP_WRAP   1

// fwd decls
void EmptyCommandHistory(_In_opt_ PCOMMAND_HISTORY CommandHistory);
PCOMMAND_HISTORY ReallocCommandHistory(_In_opt_ PCOMMAND_HISTORY CurrentCommandHistory, _In_ DWORD const NumCommands);
PCOMMAND_HISTORY FindExeCommandHistory(_In_reads_(AppNameLength) PVOID AppName, _In_ DWORD AppNameLength, _In_ BOOLEAN const UnicodeExe);
void DrawCommandListBorder(_In_ PCLE_POPUP const Popup, _In_ PSCREEN_INFORMATION const ScreenInfo);
PCOMMAND GetLastCommand(_In_ PCOMMAND_HISTORY CommandHistory);
SHORT FindMatchingCommand(_In_ PCOMMAND_HISTORY CommandHistory,
                          _In_reads_bytes_(CurrentCommandLength) PCWCHAR CurrentCommand,
                          _In_ ULONG CurrentCommandLength,
                          _In_ SHORT CurrentIndex,
                          _In_ DWORD Flags);
NTSTATUS CommandNumberPopup(_In_ COOKED_READ_DATA* const CookedReadData);
void DrawCommandListPopup(_In_ PCLE_POPUP const Popup,
                          _In_ SHORT const CurrentCommand,
                          _In_ PCOMMAND_HISTORY const CommandHistory,
                          _In_ PSCREEN_INFORMATION const ScreenInfo);
void UpdateCommandListPopup(_In_ SHORT Delta,
                            _Inout_ PSHORT CurrentCommand,
                            _In_ PCOMMAND_HISTORY const CommandHistory,
                            _In_ PCLE_POPUP Popup,
                            _In_ PSCREEN_INFORMATION const ScreenInfo,
                            _In_ DWORD const Flags);
NTSTATUS RetrieveCommand(_In_ PCOMMAND_HISTORY CommandHistory,
                         _In_ WORD VirtualKeyCode,
                         _In_reads_bytes_(BufferSize) PWCHAR Buffer,
                         _In_ ULONG BufferSize,
                         _Out_ PULONG CommandSize);
UINT LoadStringEx(_In_ HINSTANCE hModule, _In_ UINT wID, _Out_writes_(cchBufferMax) LPWSTR lpBuffer, _In_ UINT cchBufferMax, _In_ WORD wLangId);

// Extended Edit Key
ExtKeyDefTable gaKeyDef;
CONST ExtKeyDefTable gaDefaultKeyDef = {
    {   // A
     0, VK_HOME, 0, // Ctrl
     LEFT_CTRL_PRESSED, VK_HOME, 0, // Alt
     0, 0, 0,   // Ctrl+Alt
     }
    ,
    {   // B
     0, VK_LEFT, 0, // Ctrl
     LEFT_CTRL_PRESSED, VK_LEFT, 0, // Alt
     }
    ,
    {   // C
     0,
     }
    ,
    {   // D
     0, VK_DELETE, 0,   // Ctrl
     LEFT_CTRL_PRESSED, VK_DELETE, 0,   // Alt
     0, 0, 0,   // Ctrl+Alt
     }
    ,
    {   // E
     0, VK_END, 0,  // Ctrl
     LEFT_CTRL_PRESSED, VK_END, 0,  // Alt
     0, 0, 0,   // Ctrl+Alt
     }
    ,
    {   // F
     0, VK_RIGHT, 0,    // Ctrl
     LEFT_CTRL_PRESSED, VK_RIGHT, 0,    // Alt
     0, 0, 0,   // Ctrl+Alt
     }
    ,
    {   // G
     0,
     }
    ,
    {   // H
     0,
     }
    ,
    {   // I
     0,
     }
    ,
    {   // J
     0,
     }
    ,
    {   // K
     LEFT_CTRL_PRESSED, VK_END, 0,  // Ctrl
     }
    ,
    {   // L
     0,
     }
    ,
    {   // M
     0,
     }
    ,
    {   // N
     0, VK_DOWN, 0, // Ctrl
     }
    ,
    {   // O
     0,
     }
    ,
    {   // P
     0, VK_UP, 0,   // Ctrl
     }
    ,
    {   // Q
     0,
     }
    ,
    {   // R
     0, VK_F8, 0,   // Ctrl
     }
    ,
    {   // S
     0, VK_PAUSE, 0,    // Ctrl
     }
    ,
    {   // T
     LEFT_CTRL_PRESSED, VK_DELETE, 0,   // Ctrl
     }
    ,
    {   // U
     0, VK_ESCAPE, 0,   // Ctrl
     }
    ,
    {   // V
     0,
     }
    ,
    {   // W
     LEFT_CTRL_PRESSED, VK_BACK, EXTKEY_ERASE_PREV_WORD,    // Ctrl
     }
    ,
    {   // X
     0,
     }
    ,
    {   // Y
     0,
     }
    ,
    {   // Z
     0,
     }
    ,
};

// Routine Description:
// - This routine validates a string buffer and returns the pointers of where the strings start within the buffer.
// Arguments:
// - Unicode - Supplies a boolean that is TRUE if the buffer contains Unicode strings, FALSE otherwise.
// - Buffer - Supplies the buffer to be validated.
// - Size - Supplies the size, in bytes, of the buffer to be validated.
// - Count - Supplies the expected number of strings in the buffer.
// ... - Supplies a pair of arguments per expected string. The first one is the expected size, in bytes, of the string
//       and the second one receives a pointer to where the string starts.
// Return Value:
// - TRUE if the buffer is valid, FALSE otherwise.
BOOLEAN IsValidStringBuffer(_In_ BOOLEAN Unicode, _In_reads_bytes_(Size) PVOID Buffer, _In_ ULONG Size, _In_ ULONG Count, ...)
{
    va_list Marker;
    va_start(Marker, Count);

    while (Count > 0)
    {
        ULONG const StringSize = va_arg(Marker, ULONG);
        PVOID* StringStart = va_arg(Marker, PVOID *);

        // Make sure the string fits in the supplied buffer and that it is properly aligned.
        if (StringSize > Size)
        {
            break;
        }

        if ((Unicode != FALSE) && ((StringSize % sizeof(WCHAR)) != 0))
        {
            break;
        }

        *StringStart = Buffer;

        // Go to the next string.
        Buffer = RtlOffsetToPointer(Buffer, StringSize);
        Size -= StringSize;
        Count -= 1;
    }

    va_end(Marker);

    return Count == 0;
}

// Routine Description:
// - Initialize the extended edit key table.
// - If pKeyDefbuf is nullptr, the internal default table is used.
// - Otherwise, lpbyte should point to a valid ExtKeyDefBuf.
void InitExtendedEditKeys(_In_opt_ ExtKeyDefBuf const * const pKeyDefBuf)
{
    // Sanity check:
    // If pKeyDefBuf is nullptr, give it the default value.
    // If the version is not supported, just use the default and bail.
    if (pKeyDefBuf == nullptr || pKeyDefBuf->dwVersion != 0)
    {
        if (pKeyDefBuf != nullptr)
        {
            RIPMSG1(RIP_WARNING, "InitExtendedEditKeys: Unsupported version number(%d)", pKeyDefBuf->dwVersion);
        }

    retry_clean:
        memmove(gaKeyDef, gaDefaultKeyDef, sizeof gaKeyDef);
        return;
    }

    // Calculate check sum
    DWORD dwCheckSum = 0;
    BYTE* const lpbyte = (BYTE*)pKeyDefBuf;
    for (int i = FIELD_OFFSET(ExtKeyDefBuf, table); i < sizeof *pKeyDefBuf; ++i)
    {
        dwCheckSum += lpbyte[i];
    }

    if (dwCheckSum != pKeyDefBuf->dwCheckSum)
    {
        goto retry_clean;
    }

    // Copy the entity
    memmove(gaKeyDef, pKeyDefBuf->table, sizeof gaKeyDef);
}

const ExtKeyDef* const GetKeyDef(WORD virtualKeyCode)
{
    size_t index = virtualKeyCode - 'A';
    if (index >= ARRAYSIZE(gaKeyDef))
    {
        return nullptr;
    }
    return &gaKeyDef[index];
}

// Routine Description:
// - Detects Word delimiters
bool IsWordDelim(_In_ WCHAR const wch)
{
    // Before it reaches here, L' ' case should have beeen already detected, and ServiceLocator::LocateGlobals()->aWordDelimChars is specified.
    ASSERT(wch != L' ' && ServiceLocator::LocateGlobals()->aWordDelimChars[0]);

    for (int i = 0; i < WORD_DELIM_MAX && ServiceLocator::LocateGlobals()->aWordDelimChars[i]; ++i)
    {
        if (wch == ServiceLocator::LocateGlobals()->aWordDelimChars[i])
        {
            return true;
        }
    }

    return false;
}

PEXE_ALIAS_LIST AddExeAliasList(_In_ LPVOID ExeName,
                                _In_ USHORT ExeLength, // in bytes
                                _In_ BOOLEAN UnicodeExe)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PEXE_ALIAS_LIST AliasList = new EXE_ALIAS_LIST();
    if (AliasList == nullptr)
    {
        return nullptr;
    }

    if (UnicodeExe)
    {
        // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
        AliasList->ExeName = new WCHAR[(ExeLength + 1) / sizeof(WCHAR)];
        if (AliasList->ExeName == nullptr)
        {
            delete AliasList;
            return nullptr;
        }
        memmove(AliasList->ExeName, ExeName, ExeLength);
        AliasList->ExeLength = ExeLength;
    }
    else
    {
        AliasList->ExeName = new WCHAR[ExeLength];
        if (AliasList->ExeName == nullptr)
        {
            delete AliasList;
            return nullptr;
        }
        AliasList->ExeLength = (USHORT)ConvertInputToUnicode(gci->CP, (LPSTR)ExeName, ExeLength, AliasList->ExeName, ExeLength);
        AliasList->ExeLength *= 2;
    }
    InitializeListHead(&AliasList->AliasList);
    InsertHeadList(&gci->ExeAliasList, &AliasList->ListLink);
    return AliasList;
}

// Routine Description:
// - This routine searches for the specified exe alias list.  It returns a pointer to the exe list if found, nullptr if not found.
PEXE_ALIAS_LIST FindExe(_In_ LPVOID ExeName,
                        _In_ USHORT ExeLength, // in bytes
                        _In_ BOOLEAN UnicodeExe)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    LPWSTR UnicodeExeName;
    if (UnicodeExe)
    {
        UnicodeExeName = (PWSTR)ExeName;
    }
    else
    {
        UnicodeExeName = new WCHAR[ExeLength];
        if (UnicodeExeName == nullptr)
            return nullptr;
        ExeLength = (USHORT)ConvertInputToUnicode(gci->CP, (LPSTR)ExeName, ExeLength, UnicodeExeName, ExeLength);
        ExeLength *= 2;
    }
    PLIST_ENTRY const ListHead = &gci->ExeAliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PEXE_ALIAS_LIST const AliasList = CONTAINING_RECORD(ListNext, EXE_ALIAS_LIST, ListLink);
        // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
        if (AliasList->ExeLength == ExeLength && !_wcsnicmp(AliasList->ExeName, UnicodeExeName, (ExeLength + 1) / sizeof(WCHAR)))
        {
            if (!UnicodeExe)
            {
                delete[] UnicodeExeName;
            }
            return AliasList;
        }
        ListNext = ListNext->Flink;
    }
    if (!UnicodeExe)
    {
        delete[] UnicodeExeName;
    }
    return nullptr;
}


// Routine Description:
// - This routine searches for the specified alias.  If it finds one,
// - it moves it to the head of the list and returns a pointer to the
// - alias. Otherwise it returns nullptr.
PALIAS FindAlias(_In_ PEXE_ALIAS_LIST AliasList, _In_reads_bytes_(AliasLength) const WCHAR *AliasName, _In_ USHORT AliasLength)
{
    PLIST_ENTRY const ListHead = &AliasList->AliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PALIAS const Alias = CONTAINING_RECORD(ListNext, ALIAS, ListLink);
        // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
        if (Alias->SourceLength == AliasLength && !_wcsnicmp(Alias->Source, AliasName, (AliasLength + 1) / sizeof(WCHAR)))
        {
            if (ListNext != ListHead->Flink)
            {
                RemoveEntryList(ListNext);
                InsertHeadList(ListHead, ListNext);
            }
            return Alias;
        }
        ListNext = ListNext->Flink;
    }

    return nullptr;
}

// Routine Description:
// - This routine creates an alias and inserts it into the exe alias list.
NTSTATUS AddAlias(_In_ PEXE_ALIAS_LIST ExeAliasList,
                  _In_reads_bytes_(SourceLength) const WCHAR *Source,
                  _In_ USHORT SourceLength,
                  _In_reads_bytes_(TargetLength) const WCHAR *Target,
                  _In_ USHORT TargetLength)
{
    PALIAS const Alias = new ALIAS();
    if (Alias == nullptr)
    {
        return STATUS_NO_MEMORY;
    }

    // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
    Alias->Source = new WCHAR[(SourceLength + 1) / sizeof(WCHAR)];
    if (Alias->Source == nullptr)
    {
        delete Alias;
        return STATUS_NO_MEMORY;
    }

    // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
    Alias->Target = new WCHAR[(TargetLength + 1) / sizeof(WCHAR)];
    if (Alias->Target == nullptr)
    {
        delete[] Alias->Source;
        delete Alias;
        return STATUS_NO_MEMORY;
    }

    Alias->SourceLength = SourceLength;
    Alias->TargetLength = TargetLength;
    memmove(Alias->Source, Source, SourceLength);
    memmove(Alias->Target, Target, TargetLength);
    InsertHeadList(&ExeAliasList->AliasList, &Alias->ListLink);
    return STATUS_SUCCESS;
}


// Routine Description:
// - This routine replaces an existing target with a new target.
NTSTATUS ReplaceAlias(_In_ PALIAS Alias, _In_reads_bytes_(TargetLength) const WCHAR *Target, _In_ USHORT TargetLength)
{
    // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
    WCHAR* const NewTarget = new WCHAR[(TargetLength + 1) / sizeof(WCHAR)];
    if (NewTarget == nullptr)
    {
        return STATUS_NO_MEMORY;
    }

    delete[] Alias->Target;
    Alias->Target = NewTarget;
    Alias->TargetLength = TargetLength;
    memmove(Alias->Target, Target, TargetLength);

    return STATUS_SUCCESS;
}


// Routine Description:
// - This routine removes an alias.
NTSTATUS RemoveAlias(_In_ PALIAS Alias)
{
    RemoveEntryList(&Alias->ListLink);
    delete[] Alias->Source;
    delete[] Alias->Target;
    delete Alias;
    return STATUS_SUCCESS;
}

void FreeAliasList(_In_ PEXE_ALIAS_LIST ExeAliasList)
{
    PLIST_ENTRY const ListHead = &ExeAliasList->AliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PALIAS const Alias = CONTAINING_RECORD(ListNext, ALIAS, ListLink);
        ListNext = ListNext->Flink;
        RemoveAlias(Alias);
    }
    RemoveEntryList(&ExeAliasList->ListLink);
    delete[] ExeAliasList->ExeName;
    delete ExeAliasList;
}

void FreeAliasBuffers()
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PLIST_ENTRY const ListHead = &gci->ExeAliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PEXE_ALIAS_LIST const AliasList = CONTAINING_RECORD(ListNext, EXE_ALIAS_LIST, ListLink);
        ListNext = ListNext->Flink;
        FreeAliasList(AliasList);
    }
}

// Routine Description:
// - Adds a command line alias to the global set.
// - Converts and calls the W version of this function.
// Arguments:
// - psSourceBuffer - The shorthand/alias or source buffer to set
// - cchSourceBufferLength - Length in characters of source buffer
// - psTargetBuffer - The destination/expansion or target buffer to set
// - cchTargetBufferLength - Length in characters of target buffer
// - psExeNameBuffer - The client EXE application attached to the host to whom this substitution will apply
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::AddConsoleAliasAImpl(_In_reads_or_z_(cchSourceBufferLength) const char* const psSourceBuffer,
                                          _In_ size_t const cchSourceBufferLength,
                                          _In_reads_or_z_(cchTargetBufferLength) const char* const psTargetBuffer,
                                          _In_ size_t const cchTargetBufferLength,
                                          _In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                          _In_ size_t const cchExeNameBufferLength)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    wistd::unique_ptr<wchar_t[]> pwsSource;
    size_t cchSource;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psSourceBuffer, cchSourceBufferLength, pwsSource, cchSource));

    wistd::unique_ptr<wchar_t[]> pwsTarget;
    size_t cchTarget;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psTargetBuffer, cchTargetBufferLength, pwsTarget, cchTarget));

    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    return AddConsoleAliasWImpl(pwsSource.get(), cchSource, pwsTarget.get(), cchTarget, pwsExeName.get(), cchExeName);
}

// Routine Description:
// - Adds a command line alias to the global set.
// Arguments:
// - pwsSourceBuffer - The shorthand/alias or source buffer to set
// - cchSourceBufferLength - Length in characters of source buffer
// - pwsTargetBuffer - The destination/expansion or target buffer to set
// - cchTargetBufferLength - Length in characters of target buffer
// - pwsExeNameBuffer - The client EXE application attached to the host to whom this substitution will apply
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::AddConsoleAliasWImpl(_In_reads_or_z_(cchSourceBufferLength) const wchar_t* const pwsSourceBuffer,
                                          _In_ size_t const cchSourceBufferLength,
                                          _In_reads_or_z_(cchTargetBufferLength) const wchar_t* const pwsTargetBuffer,
                                          _In_ size_t const cchTargetBufferLength,
                                          _In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                          _In_ size_t const cchExeNameBufferLength)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    RETURN_HR_IF(E_INVALIDARG, cchSourceBufferLength == 0);

    // Convert size_ts into SHORTs for existing alias functions to use.
    USHORT cbExeNameBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));
    USHORT cbSourceBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchSourceBufferLength, &cbSourceBufferLength));
    USHORT cbTargetBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchTargetBufferLength, &cbTargetBufferLength));

    // find specified exe.  if it's not there, add it if we're not removing an alias.
    PEXE_ALIAS_LIST ExeAliasList = FindExe((LPVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
    if (ExeAliasList != nullptr)
    {
        PALIAS Alias = FindAlias(ExeAliasList, pwsSourceBuffer, cbSourceBufferLength);
        if (cbTargetBufferLength > 0)
        {
            if (Alias != nullptr)
            {
                RETURN_NTSTATUS(ReplaceAlias(Alias, pwsTargetBuffer, cbTargetBufferLength));
            }
            else
            {
                RETURN_NTSTATUS(AddAlias(ExeAliasList, pwsSourceBuffer, cbSourceBufferLength, pwsTargetBuffer, cbTargetBufferLength));
            }
        }
        else
        {
            if (Alias != nullptr)
            {
                RETURN_NTSTATUS(RemoveAlias(Alias));
            }
        }
    }
    else
    {
        if (cbTargetBufferLength > 0)
        {
            ExeAliasList = AddExeAliasList((LPVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
            if (ExeAliasList != nullptr)
            {
                RETURN_NTSTATUS(AddAlias(ExeAliasList, pwsSourceBuffer, cbSourceBufferLength, pwsTargetBuffer, cbTargetBufferLength));
            }
            else
            {
                RETURN_HR(E_OUTOFMEMORY);
            }
        }
    }

    return S_OK;
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - pwsSourceBuffer - The shorthand/alias or source buffer to use in lookup
// - cchSourceBufferLength - Length in characters of source buffer
// - pwsTargetBuffer - The destination/expansion or target buffer we are attempting to retrieve. Optionally nullptr to retrieve needed space.
// - cchTargetBufferLength - Length in characters of target buffer. Set to 0 when pwsTargetBuffer is nullptr.
// - pcchTargetBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written (if pwsTargetBuffer is valid)
//                                     or how many characters would have been consumed (if pwsTargetBuffer was valid.)
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleAliasWImplHelper(_In_reads_or_z_(cchSourceBufferLength) const wchar_t* const pwsSourceBuffer,
                                   _In_ size_t const cchSourceBufferLength,
                                   _Out_writes_to_opt_(cchTargetBufferLength, *pcchTargetBufferWrittenOrNeeded) _Always_(_Post_z_) wchar_t* const pwsTargetBuffer,
                                   _In_ size_t const cchTargetBufferLength,
                                   _Out_ size_t* const pcchTargetBufferWrittenOrNeeded,
                                   _In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                   _In_ size_t const cchExeNameBufferLength)
{
    // Ensure output variables are initialized
    *pcchTargetBufferWrittenOrNeeded = 0;
    if (nullptr != pwsTargetBuffer && cchTargetBufferLength > 0)
    {
        *pwsTargetBuffer = L'\0';
    }

    // Convert size_ts into SHORTs for existing alias functions to use.
    USHORT cbExeNameBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));
    USHORT cbSourceBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchSourceBufferLength, &cbSourceBufferLength));

    PEXE_ALIAS_LIST const pExeAliasList = FindExe((LPVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), nullptr == pExeAliasList);

    PALIAS const pAlias = FindAlias(pExeAliasList, pwsSourceBuffer, cbSourceBufferLength);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), nullptr == pAlias);

    // TargetLength is a byte count, convert to characters.
    size_t cchTarget = pAlias->TargetLength / sizeof(wchar_t);
    size_t const cchNull = 1;

    // The total space we need is the length of the string + the null terminator.
    size_t cchNeeded;
    RETURN_IF_FAILED(SizeTAdd(cchTarget, cchNull, &cchNeeded));

    *pcchTargetBufferWrittenOrNeeded = cchNeeded;

    if (nullptr != pwsTargetBuffer)
    {
        // if the user didn't give us enough space, return with insufficient buffer code early.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), cchTargetBufferLength < cchNeeded);

        RETURN_IF_FAILED(StringCchCopyNW(pwsTargetBuffer, cchTargetBufferLength, pAlias->Target, cchTarget));
    }

    return S_OK;
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// - This function will convert input parameters from A to W, call the W version of the routine,
//   and attempt to convert the resulting data back to A for return.
// Arguments:
// - pwsSourceBuffer - The shorthand/alias or source buffer to use in lookup
// - cchSourceBufferLength - Length in characters of source buffer
// - pwsTargetBuffer - The destination/expansion or target buffer we are attempting to retrieve.
// - cchTargetBufferLength - Length in characters of target buffer.
// - pcchTargetBufferWritten - Pointer to space that will specify how many characters were written
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasAImpl(_In_reads_or_z_(cchSourceBufferLength) const char* const psSourceBuffer,
                                          _In_ size_t const cchSourceBufferLength,
                                          _Out_writes_to_(cchTargetBufferLength, *pcchTargetBufferWritten) _Always_(_Post_z_) char* const psTargetBuffer,
                                          _In_ size_t const cchTargetBufferLength,
                                          _Out_ size_t* const pcchTargetBufferWritten,
                                          _In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                          _In_ size_t const cchExeNameBufferLength)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchTargetBufferWritten = 0;
    if (nullptr != psTargetBuffer && cchTargetBufferLength > 0)
    {
        *psTargetBuffer = L'\0';
    }

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    // Convert our input parameters to Unicode.
    wistd::unique_ptr<wchar_t[]> pwsSource;
    size_t cchSource;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psSourceBuffer, cchSourceBufferLength, pwsSource, cchSource));

    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    // Figure out how big our temporary Unicode buffer must be to retrieve output
    size_t cchTargetBufferNeeded;
    RETURN_IF_FAILED(GetConsoleAliasWImplHelper(pwsSource.get(), cchSource, nullptr, 0, &cchTargetBufferNeeded, pwsExeName.get(), cchExeName));

    // If there's nothing to get, then simply return.
    RETURN_HR_IF(S_OK, 0 == cchTargetBufferNeeded);

    // If the user hasn't given us a buffer at all and we need one, return an error.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), 0 == cchTargetBufferLength);

    // Allocate a unicode buffer of the right size.
    wistd::unique_ptr<wchar_t[]> pwsTarget = wil::make_unique_nothrow<wchar_t[]>(cchTargetBufferNeeded);
    RETURN_IF_NULL_ALLOC(pwsTarget);

    // Call the Unicode version of this method
    size_t cchTargetBufferWritten;
    RETURN_IF_FAILED(GetConsoleAliasWImplHelper(pwsSource.get(), cchSource, pwsTarget.get(), cchTargetBufferNeeded, &cchTargetBufferWritten, pwsExeName.get(), cchExeName));

    // Set the return size copied to the size given before we attempt to copy.
    // Then multiply by sizeof(wchar_t) due to a long standing bug that we must preserve for compatibility.
    // On failure, the API has historically given back this value.
    *pcchTargetBufferWritten = cchTargetBufferLength * sizeof(wchar_t);

    // Convert result to A
    wistd::unique_ptr<char[]> psConverted;
    size_t cchConverted;
    RETURN_IF_FAILED(ConvertToA(uiCodePage, pwsTarget.get(), cchTargetBufferWritten, psConverted, cchConverted));

    // Copy safely to output buffer
    RETURN_IF_FAILED(StringCchCopyNA(psTargetBuffer, cchTargetBufferLength, psConverted.get(), cchConverted));

    // And return the size copied.
    *pcchTargetBufferWritten = cchConverted;

    return S_OK;
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// Arguments:
// - pwsSourceBuffer - The shorthand/alias or source buffer to use in lookup
// - cchSourceBufferLength - Length in characters of source buffer
// - pwsTargetBuffer - The destination/expansion or target buffer we are attempting to retrieve.
// - cchTargetBufferLength - Length in characters of target buffer.
// - pcchTargetBufferWritten - Pointer to space that will specify how many characters were written
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasWImpl(_In_reads_or_z_(cchSourceBufferLength) const wchar_t* const pwsSourceBuffer,
                                          _In_ size_t const cchSourceBufferLength,
                                          _Out_writes_to_(cchTargetBufferLength, *pcchTargetBufferWritten) _Always_(_Post_z_) wchar_t* const pwsTargetBuffer,
                                          _In_ size_t const cchTargetBufferLength,
                                          _Out_ size_t* const pcchTargetBufferWritten,
                                          _In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                          _In_ size_t const cchExeNameBufferLength)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    HRESULT hr = GetConsoleAliasWImplHelper(pwsSourceBuffer, cchSourceBufferLength, pwsTargetBuffer, cchTargetBufferLength, pcchTargetBufferWritten, pwsExeNameBuffer, cchExeNameBufferLength);

    if (FAILED(hr))
    {
        *pcchTargetBufferWritten = cchTargetBufferLength;
    }

    return hr;
}

// These variables define the seperator character and the length of the string.
// They will be used to as the joiner between source and target strings when returning alias data in list form.
static PCWSTR const pwszAliasesSeperator = L"=";
static size_t const cchAliasesSeperator = wcslen(pwszAliasesSeperator);

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - fCountInUnicode - True for W version (UCS-2 Unicode) calls. False for A version calls (all multibyte formats.)
// - uiCodePage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - pcchAliasesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleAliasesLengthWImplHelper(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                           _In_ size_t const cchExeNameBufferLength,
                                           _In_ bool const fCountInUnicode,
                                           _In_ UINT const uiCodePage,
                                           _Out_ size_t* const pcchAliasesBufferRequired)
{
    // Ensure output variables are initialized
    *pcchAliasesBufferRequired = 0;

    // Convert size_ts into SHORTs for existing alias functions to use.
    USHORT cbExeNameBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    PEXE_ALIAS_LIST const pExeAliasList = FindExe((PVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
    if (nullptr != pExeAliasList)
    {
        size_t cchNeeded = 0;

        // Each of the aliases will be made up of the source, a seperator, the target, then a null character.
        // They are of the form "Source=Target" when returned.

        size_t const cchNull = 1;
        size_t cchSeperator = cchAliasesSeperator;

        // If we're counting how much multibyte space will be needed, trial convert the seperator before we add.
        if (!fCountInUnicode)
        {
            RETURN_IF_FAILED(GetALengthFromW(uiCodePage, pwszAliasesSeperator, cchSeperator, &cchSeperator));
        }

        PLIST_ENTRY const ListHead = &pExeAliasList->AliasList;
        PLIST_ENTRY ListNext = ListHead->Flink;
        while (ListNext != ListHead)
        {
            PALIAS Alias = CONTAINING_RECORD(ListNext, ALIAS, ListLink);

            // Alias stores lengths in bytes.
            size_t cchSource = Alias->SourceLength / sizeof(wchar_t);
            size_t cchTarget = Alias->TargetLength / sizeof(wchar_t);

            // If we're counting how much multibyte space will be needed, trial convert the source and target strings before we add.
            if (!fCountInUnicode)
            {
                RETURN_IF_FAILED(GetALengthFromW(uiCodePage, Alias->Source, cchSource, &cchSource));
                RETURN_IF_FAILED(GetALengthFromW(uiCodePage, Alias->Target, cchTarget, &cchTarget));
            }

            // Accumulate all sizes to the final string count.
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSource, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSeperator, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchTarget, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));

            ListNext = ListNext->Flink;
        }

        *pcchAliasesBufferRequired = cchNeeded;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pcchAliasesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasesLengthAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                                  _In_ size_t const cchExeNameBufferLength,
                                                  _Out_ size_t* const pcchAliasesBufferRequired)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchAliasesBufferRequired = 0;

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    // Convert our input parameters to Unicode
    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    return GetConsoleAliasesLengthWImplHelper(pwsExeName.get(), cchExeName, false, uiCodePage, pcchAliasesBufferRequired);
}

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pcchAliasesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasesLengthWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                  _In_ size_t const cchExeNameBufferLength,
                                                  _Out_ size_t* const pcchAliasesBufferRequired)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleAliasesLengthWImplHelper(pwsExeNameBuffer, cchExeNameBufferLength, true, 0, pcchAliasesBufferRequired);
}

VOID ClearAliases()
{
    PEXE_ALIAS_LIST const ExeAliasList = FindExe(L"cmd.exe", 14, TRUE);
    if (ExeAliasList == nullptr)
    {
        return;
    }

    PLIST_ENTRY const ListHead = &ExeAliasList->AliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PALIAS const Alias = CONTAINING_RECORD(ListNext, ALIAS, ListLink);
        ListNext = ListNext->Flink;
        RemoveAlias(Alias);
    }
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pwsAliasBuffer - The target buffer to hold all alias pairs we are trying to retrieve.
//                    Optionally nullptr to retrieve needed space.
// - cchAliasBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written (if buffer is valid)
//                                     or how many characters would have been consumed.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleAliasesWImplHelper(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                     _In_ size_t const cchExeNameBufferLength,
                                     _Out_writes_to_opt_(cchAliasBufferLength, *pcchAliasBufferWrittenOrNeeded) _Always_(_Post_z_) wchar_t* const pwsAliasBuffer,
                                     _In_ size_t const cchAliasBufferLength,
                                     _Out_ size_t* const pcchAliasBufferWrittenOrNeeded)
{
    // Ensure output variables are initialized.
    *pcchAliasBufferWrittenOrNeeded = 0;
    if (nullptr != pwsAliasBuffer)
    {
        *pwsAliasBuffer = L'\0';
    }

    // Convert size_ts into SHORTs for existing alias functions to use.
    USHORT cbExeNameBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    PEXE_ALIAS_LIST const pExeAliasList = FindExe((LPVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
    if (nullptr != pExeAliasList)
    {
        LPWSTR AliasesBufferPtrW = pwsAliasBuffer;
        size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

        // Each of the alises will be made up of the source, a seperator, the target, then a null character.
        // They are of the form "Source=Target" when returned.

        size_t const cchNull = 1;

        PLIST_ENTRY const ListHead = &pExeAliasList->AliasList;
        PLIST_ENTRY ListNext = ListHead->Flink;
        while (ListNext != ListHead)
        {
            PALIAS const Alias = CONTAINING_RECORD(ListNext, ALIAS, ListLink);

            // Alias stores lengths in bytes.
            size_t const cchSource = Alias->SourceLength / sizeof(wchar_t);
            size_t const cchTarget = Alias->TargetLength / sizeof(wchar_t);

            // Add up how many characters we will need for the full alias data.
            size_t cchNeeded = 0;
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSource, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchAliasesSeperator, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchTarget, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));

            // If we can return the data, attempt to do so until we're done or it overflows.
            // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
            if (nullptr != pwsAliasBuffer)
            {
                // Calculate the new final total after we add what we need to see if it will exceed the limit
                size_t cchNewTotal;
                RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));

                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > cchAliasBufferLength);

                size_t cchAliasBufferRemaining;
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferLength, cchTotalLength, &cchAliasBufferRemaining));

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, Alias->Source, cchSource));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, cchSource, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchSource;

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, pwszAliasesSeperator, cchAliasesSeperator));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, cchAliasesSeperator, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchAliasesSeperator;

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, Alias->Target, cchTarget));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, cchTarget, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchTarget;

                // StringCchCopyNW ensures that the destination string is null terminated, so simply advance the pointer.
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, 1, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchNull;
            }

            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));

            ListNext = ListNext->Flink;
        }

        *pcchAliasBufferWrittenOrNeeded = cchTotalLength;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// - Will convert all input from A to W, call the W version of the function, then convert resulting W to A text and return.
// Arguments:
// - psExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - psAliasBuffer - The target buffer to hold all alias pairs we are trying to retrieve.
// - cchAliasBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasBufferWritten - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasesAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                            _In_ size_t const cchExeNameBufferLength,
                                            _Out_writes_to_(cchAliasBufferLength, *pcchAliasBufferWritten) _Always_(_Post_z_) char* const psAliasBuffer,
                                            _In_ size_t const cchAliasBufferLength,
                                            _Out_ size_t* const pcchAliasBufferWritten)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchAliasBufferWritten = 0;
    *psAliasBuffer = '\0';

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    // Convert our input parameters to Unicode.
    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    // Figure out how big our temporary Unicode buffer must be to retrieve output
    size_t cchAliasBufferNeeded;
    RETURN_IF_FAILED(GetConsoleAliasesWImplHelper(pwsExeName.get(), cchExeName, nullptr, 0, &cchAliasBufferNeeded));

    // If there's nothing to get, then simply return.
    RETURN_HR_IF(S_OK, 0 == cchAliasBufferNeeded);

    // Allocate a unicode buffer of the right size.
    wistd::unique_ptr<wchar_t[]> pwsAlias = wil::make_unique_nothrow<wchar_t[]>(cchAliasBufferNeeded);
    RETURN_IF_NULL_ALLOC(pwsAlias);

    // Call the Unicode version of this method
    size_t cchAliasBufferWritten;
    RETURN_IF_FAILED(GetConsoleAliasesWImplHelper(pwsExeName.get(), cchExeName, pwsAlias.get(), cchAliasBufferNeeded, &cchAliasBufferWritten));

    // Convert result to A
    wistd::unique_ptr<char[]> psConverted;
    size_t cchConverted;
    RETURN_IF_FAILED(ConvertToA(uiCodePage, pwsAlias.get(), cchAliasBufferWritten, psConverted, cchConverted));

    // Copy safely to the output buffer
    // - Aliases are a series of null terminated strings. We cannot use a SafeString function to copy.
    //   So instead, validate and use raw memory copy.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchConverted > cchAliasBufferLength);
    memcpy_s(psAliasBuffer, cchAliasBufferLength, psConverted.get(), cchConverted);

    // And return the size copied.
    *pcchAliasBufferWritten = cchConverted;

    return S_OK;
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pwsAliasBuffer - The target buffer to hold all alias pairs we are trying to retrieve.
// - cchAliasBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasBufferWritten - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasesWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                            _In_ size_t const cchExeNameBufferLength,
                                            _Out_writes_to_(cchAliasBufferLength, *pcchAliasBufferWritten) _Always_(_Post_z_) wchar_t* const pwsAliasBuffer,
                                            _In_ size_t const cchAliasBufferLength,
                                            _Out_ size_t* const pcchAliasBufferWritten)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleAliasesWImplHelper(pwsExeNameBuffer, cchExeNameBufferLength, pwsAliasBuffer, cchAliasBufferLength, pcchAliasBufferWritten);
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - fCountInUnicode - True for W version (UCS-2 Unicode) calls. False for A version calls (all multibyte formats.)
// - uiCodePage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - pcchAliasExesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleAliasExesLengthImplHelper(_In_ bool const fCountInUnicode, _In_ UINT const uiCodePage, _Out_ size_t* const pcchAliasExesBufferRequired)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Ensure output variables are initialized
    *pcchAliasExesBufferRequired = 0;

    size_t cchNeeded = 0;

    // Each alias exe will be made up of the string payload and a null terminator.
    size_t const cchNull = 1;

    PLIST_ENTRY const ListHead = &gci->ExeAliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PEXE_ALIAS_LIST const AliasList = CONTAINING_RECORD(ListNext, EXE_ALIAS_LIST, ListLink);

        // AliasList stores lengths in bytes.
        size_t cchExe = AliasList->ExeLength / sizeof(wchar_t);

        // If we're counting how much multibyte space will be needed, trial convert the exe string before we add.
        if (!fCountInUnicode)
        {
            RETURN_IF_FAILED(GetALengthFromW(uiCodePage, AliasList->ExeName, cchExe, &cchExe));
        }

        // Accumulate to total
        RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchExe, &cchNeeded));
        RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));

        ListNext = ListNext->Flink;
    }

    *pcchAliasExesBufferRequired = cchNeeded;

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// Arguments:
// - pcchAliasExesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasExesLengthAImpl(_Out_ size_t* const pcchAliasExesBufferRequired)
{
    LockConsole();
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleAliasExesLengthImplHelper(false, gci->CP, pcchAliasExesBufferRequired);
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// Arguments:
// - pcchAliasExesBufferRequired - Pointer to receive the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasExesLengthWImpl(_Out_ size_t* const pcchAliasExesBufferRequired)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleAliasExesLengthImplHelper(true, 0, pcchAliasExesBufferRequired);
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - pwsAliasExesBuffer - The target buffer to hold all known EXE names we are trying to retrieve.
//                        Optionally nullptr to retrieve needed space.
// - cchAliasExesBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasExesBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written (if buffer is valid)
//                                        or how many characters would have been consumed.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleAliasExesWImplHelper(_Out_writes_to_opt_(cchAliasExesBufferLength, *pcchAliasExesBufferWrittenOrNeeded) _Always_(_Post_z_) wchar_t* const pwsAliasExesBuffer,
                                       _In_ size_t const cchAliasExesBufferLength,
                                       _Out_ size_t* const pcchAliasExesBufferWrittenOrNeeded)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Ensure output variables are initialized.
    *pcchAliasExesBufferWrittenOrNeeded = 0;
    if (nullptr != pwsAliasExesBuffer)
    {
        *pwsAliasExesBuffer = L'\0';
    }

    LPWSTR AliasExesBufferPtrW = pwsAliasExesBuffer;
    size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

    size_t const cchNull = 1;

    PLIST_ENTRY const ListHead = &gci->ExeAliasList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PEXE_ALIAS_LIST const AliasList = CONTAINING_RECORD(ListNext, EXE_ALIAS_LIST, ListLink);

        // AliasList stores length in bytes. Add 1 for null terminator.
        size_t const cchExe = (AliasList->ExeLength) / sizeof(wchar_t);

        size_t cchNeeded;
        RETURN_IF_FAILED(SizeTAdd(cchExe, cchNull, &cchNeeded));

        // If we can return the data, attempt to do so until we're done or it overflows.
        // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
        if (nullptr != pwsAliasExesBuffer)
        {
            // Calculate the new total length after we add to the buffer
            // Error out early if there is a problem.
            size_t cchNewTotal;
            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > cchAliasExesBufferLength);

            size_t cchRemaining;
            RETURN_IF_FAILED(SizeTSub(cchAliasExesBufferLength, cchTotalLength, &cchRemaining));

            RETURN_IF_FAILED(StringCchCopyNW(AliasExesBufferPtrW, cchRemaining, AliasList->ExeName, cchExe));
            AliasExesBufferPtrW += cchNeeded;
        }

        // Accumulate the total written amount.
        RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));

        ListNext = ListNext->Flink;
    }

    *pcchAliasExesBufferWrittenOrNeeded = cchTotalLength;

    return S_OK;
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// - Will call the W version of the function and convert all text back to A on returning.
// Arguments:
// - psAliasExesBuffer - The target buffer to hold all known EXE names we are trying to retrieve.
// - cchAliasExesBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasExesBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasExesAImpl(_Out_writes_to_(cchAliasExesBufferLength, *pcchAliasExesBufferWritten) _Always_(_Post_z_) char* const psAliasExesBuffer,
                                              _In_ size_t const cchAliasExesBufferLength,
                                              _Out_ size_t* const pcchAliasExesBufferWritten)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchAliasExesBufferWritten = 0;
    *psAliasExesBuffer = '\0';

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    // Figure our how big our temporary Unicode buffer must be to retrieve output
    size_t cchAliasExesBufferNeeded;
    RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(nullptr, 0, &cchAliasExesBufferNeeded));

    // If there's nothing to get, then simply return.
    RETURN_HR_IF(S_OK, 0 == cchAliasExesBufferNeeded);

    // Allocate a unicode buffer of the right size.
    wistd::unique_ptr<wchar_t[]> pwsTarget = wil::make_unique_nothrow<wchar_t[]>(cchAliasExesBufferNeeded);
    RETURN_IF_NULL_ALLOC(pwsTarget);

    // Call the Unicode version of this method
    size_t cchAliasExesBufferWritten;
    RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(pwsTarget.get(), cchAliasExesBufferNeeded, &cchAliasExesBufferWritten));

    // Convert result to A
    wistd::unique_ptr<char[]> psConverted;
    size_t cchConverted;
    RETURN_IF_FAILED(ConvertToA(uiCodePage, pwsTarget.get(), cchAliasExesBufferWritten, psConverted, cchConverted));

    // Copy safely to the output buffer
    // - AliasExes are a series of null terminated strings. We cannot use a SafeString function to copy.
    //   So instead, validate and use raw memory copy.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchConverted > cchAliasExesBufferLength);
    memcpy_s(psAliasExesBuffer, cchAliasExesBufferLength, psConverted.get(), cchConverted);

    // And return the size copied.
    *pcchAliasExesBufferWritten = cchConverted;

    return S_OK;
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// Arguments:
// - pwsAliasExesBuffer - The target buffer to hold all known EXE names we are trying to retrieve.
// - cchAliasExesBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasExesBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleAliasExesWImpl(_Out_writes_to_(cchAliasExesBufferLength, *pcchAliasExesBufferWritten) _Always_(_Post_z_)  wchar_t* const pwsAliasExesBuffer,
                                              _In_ size_t const cchAliasExesBufferLength,
                                              _Out_ size_t* const pcchAliasExesBufferWritten)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleAliasExesWImplHelper(pwsAliasExesBuffer, cchAliasExesBufferLength, pcchAliasExesBufferWritten);
}

#define MAX_ARGS 9

// Routine Description:
// - This routine matches the input string with an alias and copies the alias to the input buffer.
// Arguments:
// - pwchSource - string to match
// - cbSource - length of pwchSource in bytes
// - pwchTarget - where to store matched string
// - pcbTarget - on input, contains size of pwchTarget.  On output, contains length of alias stored in pwchTarget.
// - SourceIsCommandLine - if true, source buffer is a command line, where
//                         the first blank separate token is to be check for an alias, and if
//                         it matches, replaced with the value of the alias.  if false, then
//                         the source string is a null terminated alias name.
// - LineCount - aliases can contain multiple commands.  $T is the command separator
// Return Value:
// - SUCCESS - match was found and alias was copied to buffer.
NTSTATUS MatchAndCopyAlias(_In_reads_bytes_(cbSource) PWCHAR pwchSource,
                           _In_ USHORT cbSource,
                           _Out_writes_bytes_(*pcbTarget) PWCHAR pwchTarget,
                           _Inout_ PUSHORT pcbTarget,
                           _In_reads_bytes_(cbExe) PWCHAR pwchExe,
                           _In_ USHORT cbExe,
                           _Out_ PDWORD pcLines)
{
    NTSTATUS Status = STATUS_SUCCESS;

    // Alloc of exename may have failed.
    if (pwchExe == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Find exe.
    PEXE_ALIAS_LIST const ExeAliasList = FindExe(pwchExe, cbExe, TRUE);
    if (ExeAliasList == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Find first blank.
    PWCHAR Tmp = pwchSource;
    USHORT SourceUpToFirstBlank = 0; // in chars
#pragma prefast(suppress:26019, "Legacy. This is bounded appropriately by cbSource.")
    for (; *Tmp != (WCHAR)' ' && SourceUpToFirstBlank < (USHORT)(cbSource / sizeof(WCHAR)); Tmp++, SourceUpToFirstBlank++)
    {
        /* Do nothing */
    }

    // find char past first blank
    USHORT j = SourceUpToFirstBlank;
    while (j < (USHORT)(cbSource / sizeof(WCHAR)) && *Tmp == (WCHAR)' ')
    {
        Tmp++;
        j++;
    }

    LPWSTR SourcePtr = Tmp;
    USHORT const SourceRemainderLength = (USHORT)((cbSource / sizeof(WCHAR)) - j); // in chars

    // find alias
    PALIAS const Alias = FindAlias(ExeAliasList, pwchSource, (USHORT)(SourceUpToFirstBlank * sizeof(WCHAR)));
    if (Alias == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
    PWCHAR const TmpBuffer = new WCHAR[(*pcbTarget + 1) / sizeof(WCHAR)];
    if (!TmpBuffer)
    {
        return STATUS_NO_MEMORY;
    }

    // count args in target
    USHORT ArgCount = 0;
    *pcLines = 1;
    Tmp = Alias->Target;
    for (USHORT i = 0; (USHORT)(i + 1) < (USHORT)(Alias->TargetLength / sizeof(WCHAR)); i++)
    {
        if (*Tmp == (WCHAR)'$' && *(Tmp + 1) >= (WCHAR)'1' && *(Tmp + 1) <= (WCHAR)'9')
        {
            USHORT ArgNum = *(Tmp + 1) - (WCHAR)'0';

            if (ArgNum > ArgCount)
            {
                ArgCount = ArgNum;
            }

            Tmp++;
            i++;
        }
        else if (*Tmp == (WCHAR)'$' && *(Tmp + 1) == (WCHAR)'*')
        {
            if (ArgCount == 0)
            {
                ArgCount = 1;
            }

            Tmp++;
            i++;
        }

        Tmp++;
    }

    // Package up space separated strings in source into array of args.
    USHORT NumSourceArgs = 0;
    Tmp = SourcePtr;
    LPWSTR Args[MAX_ARGS];
    USHORT ArgsLength[MAX_ARGS];    // in bytes
    for (USHORT i = 0, k = 0; i < ArgCount; i++)
    {
        if (k < SourceRemainderLength)
        {
            Args[NumSourceArgs] = Tmp;
            ArgsLength[NumSourceArgs] = 0;
            while (k++ < SourceRemainderLength && *Tmp++ != (WCHAR)' ')
            {
                ArgsLength[NumSourceArgs] += sizeof(WCHAR);
            }

            while (k < SourceRemainderLength && *Tmp == (WCHAR)' ')
            {
                k++;
                Tmp++;
            }

            NumSourceArgs++;
        }
        else
        {
            break;
        }
    }

    // Put together the target string.
    PWCHAR Buffer = TmpBuffer;
    USHORT NewTargetLength = 2 * sizeof(WCHAR);    // for CRLF
    PWCHAR TargetAlias = Alias->Target;
    for (USHORT i = 0; i < (USHORT)(Alias->TargetLength / sizeof(WCHAR)); i++)
    {
        if (NewTargetLength >= *pcbTarget)
        {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (*TargetAlias == (WCHAR)'$' && (USHORT)(i + 1) < (USHORT)(Alias->TargetLength / sizeof(WCHAR)))
        {
            TargetAlias++;
            i++;
            if (*TargetAlias >= (WCHAR)'1' && *TargetAlias <= (WCHAR)'9')
            {
                // do numbered parameter substitution
                USHORT ArgNumber;

                ArgNumber = (USHORT)(*TargetAlias - (WCHAR)'1');
                if (ArgNumber < NumSourceArgs)
                {
                    if ((NewTargetLength + ArgsLength[ArgNumber]) <= *pcbTarget)
                    {
                        memmove(Buffer, Args[ArgNumber], ArgsLength[ArgNumber]);
                        Buffer += ArgsLength[ArgNumber] / sizeof(WCHAR);
                        NewTargetLength += ArgsLength[ArgNumber];
                    }
                    else
                    {
                        Status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }
                }
            }
            else if (*TargetAlias == (WCHAR)'*')
            {
                // Do * parameter substitution.
                if (NumSourceArgs)
                {
                    if ((USHORT)(NewTargetLength + (SourceRemainderLength * sizeof(WCHAR))) <= *pcbTarget)
                    {
                        memmove(Buffer, Args[0], SourceRemainderLength * sizeof(WCHAR));
                        Buffer += SourceRemainderLength;
                        NewTargetLength += SourceRemainderLength * sizeof(WCHAR);
                    }
                    else
                    {
                        Status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }
                }
            }
            else if (*TargetAlias == (WCHAR)'l' || *TargetAlias == (WCHAR)'L')
            {
                // Do < substitution.
                *Buffer++ = (WCHAR)'<';
                NewTargetLength += sizeof(WCHAR);
            }
            else if (*TargetAlias == (WCHAR)'g' || *TargetAlias == (WCHAR)'G')
            {
                // Do > substitution.
                *Buffer++ = (WCHAR)'>';
                NewTargetLength += sizeof(WCHAR);
            }
            else if (*TargetAlias == (WCHAR)'b' || *TargetAlias == (WCHAR)'B')
            {
                // Do | substitution.
                *Buffer++ = (WCHAR)'|';
                NewTargetLength += sizeof(WCHAR);
            }
            else if (*TargetAlias == (WCHAR)'t' || *TargetAlias == (WCHAR)'T')
            {
                // do newline substitution
                if ((USHORT)(NewTargetLength + (sizeof(WCHAR) * 2)) > *pcbTarget)
                {
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                *pcLines += 1;
                *Buffer++ = UNICODE_CARRIAGERETURN;
                *Buffer++ = UNICODE_LINEFEED;
                NewTargetLength += sizeof(WCHAR) * 2;
            }
            else
            {
                // copy $X
                *Buffer++ = (WCHAR)'$';
                NewTargetLength += sizeof(WCHAR);
                *Buffer++ = *TargetAlias;
                NewTargetLength += sizeof(WCHAR);
            }
            TargetAlias++;
        }
        else
        {
            // copy char
            *Buffer++ = *TargetAlias++;
            NewTargetLength += sizeof(WCHAR);
        }
    }

    __analysis_assume(!NT_SUCCESS(Status) || NewTargetLength <= *pcbTarget);
    if (NT_SUCCESS(Status))
    {
        // We pre-reserve space for these two characters so we know there's enough room here.
        ASSERT(NewTargetLength <= *pcbTarget);
        *Buffer++ = UNICODE_CARRIAGERETURN;
        *Buffer++ = UNICODE_LINEFEED;

        memmove(pwchTarget, TmpBuffer, NewTargetLength);
    }

    delete[] TmpBuffer;
    *pcbTarget = NewTargetLength;

    return Status;
}

// Routine Description:
// - Clears all command history for the given EXE name
// - Will convert input parameters and call the W version of this method
// Arguments:
// - psExeNameBuffer - The client EXE application attached to the host whose history we should clear
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::ExpungeConsoleCommandHistoryAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                                       _In_ size_t const cchExeNameBufferLength)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(gci->CP, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));


    return ExpungeConsoleCommandHistoryWImpl(pwsExeName.get(),
                                             cchExeName);
}

// Routine Description:
// - Clears all command history for the given EXE name
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose history we should clear
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::ExpungeConsoleCommandHistoryWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                       _In_ size_t const cchExeNameBufferLength)
{
    // Convert character count to DWORD byte count to interface with existing functions
    DWORD cbExeNameBufferLength;
    RETURN_IF_FAILED(GetDwordByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    EmptyCommandHistory(FindExeCommandHistory((PVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE));

    return S_OK;
}

// Routine Description:
// - Sets the number of commands that will be stored in history for a given EXE name
// - Will convert input parameters and call the W version of this method
// Arguments:
// - psExeNameBuffer - A client EXE application attached to the host
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - NumberOfCommands - Specifies the maximum length of the associated history buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::SetConsoleNumberOfCommandsAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                                     _In_ size_t const cchExeNameBufferLength,
                                                     _In_ size_t const NumberOfCommands)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(gci->CP, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    return SetConsoleNumberOfCommandsWImpl(pwsExeName.get(),
                                           cchExeName,
                                           NumberOfCommands);
}

// Routine Description:
// - Sets the number of commands that will be stored in history for a given EXE name
// Arguments:
// - pwsExeNameBuffer - A client EXE application attached to the host
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - NumberOfCommands - Specifies the maximum length of the associated history buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::SetConsoleNumberOfCommandsWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                     _In_ size_t const cchExeNameBufferLength,
                                                     _In_ size_t const NumberOfCommands)
{
    // Convert character count to DWORD byte count to interface with existing functions
    DWORD cbExeNameBufferLength;
    RETURN_IF_FAILED(GetDwordByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    // Convert number of commands to DWORD to interface with existing functions
    DWORD dwNumberOfCommands;
    RETURN_IF_FAILED(SizeTToDWord(NumberOfCommands, &dwNumberOfCommands));

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    ReallocCommandHistory(FindExeCommandHistory((PVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE), dwNumberOfCommands);

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - fCountInUnicode - True for W version (UCS-2 Unicode) calls. False for A version calls (all multibyte formats.)
// - uiCodePage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - pcchCommandHistoryLength - Pointer to receive the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleCommandHistoryLengthImplHelper(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                 _In_ size_t const cchExeNameBufferLength,
                                                 _In_ bool const fCountInUnicode,
                                                 _In_ UINT const uiCodePage,
                                                 _Out_ size_t* const pcchCommandHistoryLength)
{
    // Ensure output variables are initialized
    *pcchCommandHistoryLength = 0;

    // Convert character count to DWORD byte count to interface with existing functions
    DWORD cbExeNameBufferLength;
    RETURN_IF_FAILED(GetDwordByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    PCOMMAND_HISTORY const pCommandHistory = FindExeCommandHistory((PVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);
    if (nullptr != pCommandHistory)
    {
        size_t cchNeeded = 0;

        // Every command history item is made of a string length followed by 1 null character.
        size_t const cchNull = 1;

        for (SHORT i = 0; i < pCommandHistory->NumberOfCommands; i++)
        {
            // Commands store lengths in bytes.
            size_t cchCommand = pCommandHistory->Commands[i]->CommandLength / sizeof(wchar_t);

            // This is the proposed length of the whole string.
            size_t cchProposed;
            RETURN_IF_FAILED(SizeTAdd(cchCommand, cchNull, &cchProposed));

            // If we're counting how much multibyte space will be needed, trial convert the command string before we add.
            if (!fCountInUnicode)
            {
                RETURN_IF_FAILED(GetALengthFromW(uiCodePage, pCommandHistory->Commands[i]->Command, cchCommand, &cchCommand));
            }

            // Accumulate the result
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchProposed, &cchNeeded));
        }

        *pcchCommandHistoryLength = cchNeeded;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - psExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pcchCommandHistoryLength - Pointer to receive the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryLengthAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                                         _In_ size_t const cchExeNameBufferLength,
                                                         _Out_ size_t* const pcchCommandHistoryLength)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchCommandHistoryLength = 0;

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    return GetConsoleCommandHistoryLengthImplHelper(pwsExeName.get(), cchExeName, false, uiCodePage, pcchCommandHistoryLength);
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pcchCommandHistoryLength - Pointer to receive the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryLengthWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                         _In_ size_t const cchExeNameBufferLength,
                                                         _Out_ size_t* const pcchCommandHistoryLength)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleCommandHistoryLengthImplHelper(pwsExeNameBuffer, cchExeNameBufferLength, true, 0, pcchCommandHistoryLength);
}

// Routine Description:
// - Retrieves a the full command history for a given EXE name known to the console.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pwsCommandHistoryBuffer - The target buffer for data we are attempting to retrieve. Optionally nullptr to retrieve needed space.
// - cchCommandHistoryBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchCommandHistoryBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written (if buffer is valid)
//                                             or how many characters would have been consumed.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleCommandHistoryWImplHelper(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                            _In_ size_t const cchExeNameBufferLength,
                                            _Out_writes_to_opt_(cchCommandHistoryBufferLength, *pcchCommandHistoryBufferWrittenOrNeeded) _Always_(_Post_z_) wchar_t* const pwsCommandHistoryBuffer,
                                            _In_ size_t const cchCommandHistoryBufferLength,
                                            _Out_ size_t* const pcchCommandHistoryBufferWrittenOrNeeded)
{
    // Ensure output variables are initialized
    *pcchCommandHistoryBufferWrittenOrNeeded = 0;
    if (nullptr != pwsCommandHistoryBuffer)
    {
        *pwsCommandHistoryBuffer = L'\0';
    }

    // Convert size_ts into SHORTs for existing command functions to use.
    USHORT cbExeNameBufferLength;
    RETURN_IF_FAILED(GetUShortByteCount(cchExeNameBufferLength, &cbExeNameBufferLength));

    PCOMMAND_HISTORY const CommandHistory = FindExeCommandHistory((PVOID)pwsExeNameBuffer, cbExeNameBufferLength, TRUE);

    if (nullptr != CommandHistory)
    {
        PWCHAR CommandBufferW = pwsCommandHistoryBuffer;

        size_t cchTotalLength = 0;

        size_t const cchNull = 1;

        for (SHORT i = 0; i < CommandHistory->NumberOfCommands; i++)
        {
            // Command stores length in bytes. Add 1 for null terminator.
            size_t const cchCommand = CommandHistory->Commands[i]->CommandLength / sizeof(wchar_t);

            size_t cchNeeded;
            RETURN_IF_FAILED(SizeTAdd(cchCommand, cchNull, &cchNeeded));

            // If we can return the data, attempt to do so until we're done or it overflows.
            // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
            if (nullptr != pwsCommandHistoryBuffer)
            {
                // Calculate what the new total would be after we add what we need.
                size_t cchNewTotal;
                RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));

                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > cchCommandHistoryBufferLength);

                size_t cchRemaining;
                RETURN_IF_FAILED(SizeTSub(cchCommandHistoryBufferLength,
                                          cchTotalLength,
                                          &cchRemaining));

                RETURN_IF_FAILED(StringCchCopyNW(CommandBufferW,
                                                 cchRemaining,
                                                 CommandHistory->Commands[i]->Command,
                                                 cchCommand));

                CommandBufferW += cchNeeded;
            }

            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));
        }

        *pcchCommandHistoryBufferWrittenOrNeeded = cchTotalLength;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves a the full command history for a given EXE name known to the console.
// - Converts inputs from A to W, calls the W version of this method, and then converts the resulting text W to A.
// Arguments:
// - psExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - psCommandHistoryBuffer - The target buffer for data we are attempting to retrieve.
// - cchCommandHistoryBufferLength - Length in characters of target buffer.
// - pcchCommandHistoryBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryAImpl(_In_reads_or_z_(cchExeNameBufferLength) const char* const psExeNameBuffer,
                                                   _In_ size_t const cchExeNameBufferLength,
                                                   _Out_writes_to_(cchCommandHistoryBufferLength, *pcchCommandHistoryBufferWritten) _Always_(_Post_z_) char* const psCommandHistoryBuffer,
                                                   _In_ size_t const cchCommandHistoryBufferLength,
                                                   _Out_ size_t* const pcchCommandHistoryBufferWritten)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    UINT const uiCodePage = gci->CP;

    // Ensure output variables are initialized
    *pcchCommandHistoryBufferWritten = 0;
    *psCommandHistoryBuffer = '\0';

    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    // Convert our input parameters to Unicode.
    wistd::unique_ptr<wchar_t[]> pwsExeName;
    size_t cchExeName;
    RETURN_IF_FAILED(ConvertToW(uiCodePage, psExeNameBuffer, cchExeNameBufferLength, pwsExeName, cchExeName));

    // Figure out how big our temporary Unicode buffer must be to retrieve output
    size_t cchCommandBufferNeeded;
    RETURN_IF_FAILED(GetConsoleCommandHistoryWImplHelper(pwsExeName.get(), cchExeName, nullptr, 0, &cchCommandBufferNeeded));

    // If there's nothing to get, then simply return.
    RETURN_HR_IF(S_OK, 0 == cchCommandBufferNeeded);

    // Allocate a unicode buffer of the right size.
    wistd::unique_ptr<wchar_t[]> pwsCommand = wil::make_unique_nothrow<wchar_t[]>(cchCommandBufferNeeded);
    RETURN_IF_NULL_ALLOC(pwsCommand);

    // Call the Unicode version of this method
    size_t cchCommandBufferWritten;
    RETURN_IF_FAILED(GetConsoleCommandHistoryWImplHelper(pwsExeName.get(), cchExeName, pwsCommand.get(), cchCommandBufferNeeded, &cchCommandBufferWritten));

    // Convert result to A
    wistd::unique_ptr<char[]> psConverted;
    size_t cchConverted;
    RETURN_IF_FAILED(ConvertToA(uiCodePage, pwsCommand.get(), cchCommandBufferWritten, psConverted, cchConverted));

    // Copy safely to output buffer
    // - CommandHistory are a series of null terminated strings. We cannot use a SafeString function to copy.
    //   So instead, validate and use raw memory copy.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchConverted > cchCommandHistoryBufferLength);
    memcpy_s(psCommandHistoryBuffer, cchCommandHistoryBufferLength, psConverted.get(), cchConverted);

    // And return the size copied.
    *pcchCommandHistoryBufferWritten = cchConverted;

    return S_OK;
}

// Routine Description:
// - Retrieves a the full command history for a given EXE name known to the console.
// - Converts inputs from A to W, calls the W version of this method, and then converts the resulting text W to A.
// Arguments:
// - pwsExeNameBuffer - The client EXE application attached to the host whose set we should check
// - cchExeNameBufferLength - Length in characters of EXE name buffer
// - pwsCommandHistoryBuffer - The target buffer for data we are attempting to retrieve.
// - cchCommandHistoryBufferLength - Length in characters of target buffer.
// - pcchCommandHistoryBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryWImpl(_In_reads_or_z_(cchExeNameBufferLength) const wchar_t* const pwsExeNameBuffer,
                                                   _In_ size_t const cchExeNameBufferLength,
                                                   _Out_writes_to_(cchCommandHistoryBufferLength, *pcchCommandHistoryBufferWritten) _Always_(_Post_z_) wchar_t* const pwsCommandHistoryBuffer,
                                                   _In_ size_t const cchCommandHistoryBufferLength,
                                                   _Out_ size_t* const pcchCommandHistoryBufferWritten)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleCommandHistoryWImplHelper(pwsExeNameBuffer, cchExeNameBufferLength, pwsCommandHistoryBuffer, cchCommandHistoryBufferLength, pcchCommandHistoryBufferWritten);
}

PCOMMAND_HISTORY ReallocCommandHistory(_In_opt_ PCOMMAND_HISTORY CurrentCommandHistory, _In_ DWORD const NumCommands)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // To protect ourselves from overflow and general arithmetic errors, a limit of SHORT_MAX is put on the size of the command history.
    if (CurrentCommandHistory == nullptr || CurrentCommandHistory->MaximumNumberOfCommands == (SHORT)NumCommands || NumCommands > SHORT_MAX)
    {
        return CurrentCommandHistory;
    }

    PCOMMAND_HISTORY const History = (PCOMMAND_HISTORY) new BYTE[sizeof(COMMAND_HISTORY) + NumCommands * sizeof(PCOMMAND)];
    if (History == nullptr)
    {
        return CurrentCommandHistory;
    }

    *History = *CurrentCommandHistory;
    History->Flags |= CLE_RESET;
    History->NumberOfCommands = min(History->NumberOfCommands, (SHORT)NumCommands);
    History->LastAdded = History->NumberOfCommands - 1;
    History->LastDisplayed = History->NumberOfCommands - 1;
    History->FirstCommand = 0;
    History->MaximumNumberOfCommands = (SHORT)NumCommands;
    int i;
    for (i = 0; i < History->NumberOfCommands; i++)
    {
        History->Commands[i] = CurrentCommandHistory->Commands[COMMAND_NUM_TO_INDEX(i, CurrentCommandHistory)];
    }
    for (; i < CurrentCommandHistory->NumberOfCommands; i++)
    {
#pragma prefast(suppress:6001, "Confused by 0 length array being used. This is fine until 0-size array is refactored.")
        delete[](CurrentCommandHistory->Commands[COMMAND_NUM_TO_INDEX(i, CurrentCommandHistory)]);
    }

    RemoveEntryList(&CurrentCommandHistory->ListLink);
    InitializeListHead(&History->PopupList);
    InsertHeadList(&gci->CommandHistoryList, &History->ListLink);

    delete[] CurrentCommandHistory;
    return History;
}

PCOMMAND_HISTORY FindExeCommandHistory(_In_reads_(AppNameLength) PVOID AppName, _In_ DWORD AppNameLength, _In_ BOOLEAN const Unicode)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PWCHAR AppNamePtr = nullptr;
    if (!Unicode)
    {
        AppNamePtr = new WCHAR[AppNameLength];
        if (AppNamePtr == nullptr)
        {
            return nullptr;
        }
        AppNameLength = ConvertInputToUnicode(gci->CP, (PSTR)AppName, AppNameLength, AppNamePtr, AppNameLength);
        AppNameLength *= 2;
    }
    else
    {
        AppNamePtr = (PWCHAR)AppName;
    }

    PLIST_ENTRY const ListHead = &gci->CommandHistoryList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PCOMMAND_HISTORY const History = CONTAINING_RECORD(ListNext, COMMAND_HISTORY, ListLink);
        ListNext = ListNext->Flink;

        if (History->Flags & CLE_ALLOCATED && !_wcsnicmp(History->AppName, AppNamePtr, (USHORT)AppNameLength / sizeof(WCHAR)))
        {
            if (!Unicode)
            {
                delete[] AppNamePtr;
            }
            return History;
        }
    }
    if (!Unicode)
    {
        delete[] AppNamePtr;
    }
    return nullptr;
}

// Routine Description:
// - This routine returns the LRU command history buffer, or the command history buffer that corresponds to the app name.
// Arguments:
// - Console - pointer to console.
// Return Value:
// - Pointer to command history buffer.  if none are available, returns nullptr.
PCOMMAND_HISTORY AllocateCommandHistory(_In_reads_bytes_(cbAppName) PCWSTR pwszAppName, _In_ const DWORD cbAppName, _In_ HANDLE hProcess)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Reuse a history buffer.  The buffer must be !CLE_ALLOCATED.
    // If possible, the buffer should have the same app name.
    PLIST_ENTRY const ListHead = &gci->CommandHistoryList;
    PLIST_ENTRY ListNext = ListHead->Blink;
    PCOMMAND_HISTORY BestCandidate = nullptr;
    PCOMMAND_HISTORY History = nullptr;
    BOOL SameApp = FALSE;
    while (ListNext != ListHead)
    {
        History = CONTAINING_RECORD(ListNext, COMMAND_HISTORY, ListLink);
        ListNext = ListNext->Blink;

        if ((History->Flags & CLE_ALLOCATED) == 0)
        {
            // use LRU history buffer with same app name
            if (History->AppName && !_wcsnicmp(History->AppName, pwszAppName, (USHORT)cbAppName / sizeof(WCHAR)))
            {
                BestCandidate = History;
                SameApp = TRUE;
                break;
            }

            // second best choice is LRU history buffer
            if (BestCandidate == nullptr)
            {
                BestCandidate = History;
            }
        }
    }

    // if there isn't a free buffer for the app name and the maximum number of
    // command history buffers hasn't been allocated, allocate a new one.
    if (!SameApp && gci->NumCommandHistories < gci->GetNumberOfHistoryBuffers())
    {
        size_t Size, TotalSize;

        if (FAILED(SizeTMult(gci->GetHistoryBufferSize(), sizeof(PCOMMAND), &Size)))
        {
            return nullptr;
        }

        if (FAILED(SizeTAdd(sizeof(COMMAND_HISTORY), Size, &TotalSize)))
        {
            return nullptr;
        }

        History = (PCOMMAND_HISTORY) new BYTE[TotalSize];
        if (History == nullptr)
        {
            return nullptr;
        }

        // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
        History->AppName = new WCHAR[(cbAppName + 1) / sizeof(WCHAR)];
        if (History->AppName == nullptr)
        {
            delete[] History;
            return nullptr;
        }

        memmove(History->AppName, pwszAppName, cbAppName);
        History->Flags = CLE_ALLOCATED;
        History->NumberOfCommands = 0;
        History->LastAdded = -1;
        History->LastDisplayed = -1;
        History->FirstCommand = 0;
        History->MaximumNumberOfCommands = (SHORT)gci->GetHistoryBufferSize();
        InsertHeadList(&gci->CommandHistoryList, &History->ListLink);
        gci->NumCommandHistories += 1;
        History->ProcessHandle = hProcess;
        InitializeListHead(&History->PopupList);
        return History;
    }

    // If the app name doesn't match, copy in the new app name and free the old commands.
    if (BestCandidate)
    {
        History = BestCandidate;
        ASSERT(CLE_NO_POPUPS(History));
        if (!SameApp)
        {
            SHORT i;
            if (History->AppName)
            {
                delete[] History->AppName;
            }

            for (i = 0; i < History->NumberOfCommands; i++)
            {
                delete[] History->Commands[i];
            }

            History->NumberOfCommands = 0;
            History->LastAdded = -1;
            History->LastDisplayed = -1;
            History->FirstCommand = 0;
            // Length is in bytes. Add 1 so dividing by WCHAR (2) is always rounding up.
            History->AppName = new WCHAR[(cbAppName + 1) / sizeof(WCHAR)];
            if (History->AppName == nullptr)
            {
                History->Flags &= ~CLE_ALLOCATED;
                return nullptr;
            }

            memmove(History->AppName, pwszAppName, cbAppName);
        }

        History->ProcessHandle = hProcess;
        History->Flags |= CLE_ALLOCATED;

        // move to the front of the list
        RemoveEntryList(&BestCandidate->ListLink);
        InsertHeadList(&gci->CommandHistoryList, &BestCandidate->ListLink);
    }

    return BestCandidate;
}

NTSTATUS BeginPopup(_In_ PSCREEN_INFORMATION ScreenInfo, _In_ PCOMMAND_HISTORY CommandHistory, _In_ COORD PopupSize)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // determine popup dimensions
    COORD Size = PopupSize;
    Size.X += 2;    // add borders
    Size.Y += 2;    // add borders
    if (Size.X >= (SHORT)(ScreenInfo->GetScreenWindowSizeX()))
    {
        Size.X = (SHORT)(ScreenInfo->GetScreenWindowSizeX());
    }
    if (Size.Y >= (SHORT)(ScreenInfo->GetScreenWindowSizeY()))
    {
        Size.Y = (SHORT)(ScreenInfo->GetScreenWindowSizeY());
    }

    // make sure there's enough room for the popup borders

    if (Size.X < 2 || Size.Y < 2)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // determine origin.  center popup on window
    COORD Origin;
    Origin.X = (SHORT)((ScreenInfo->GetScreenWindowSizeX() - Size.X) / 2 + ScreenInfo->GetBufferViewport().Left);
    Origin.Y = (SHORT)((ScreenInfo->GetScreenWindowSizeY() - Size.Y) / 2 + ScreenInfo->GetBufferViewport().Top);

    // allocate a popup structure
    PCLE_POPUP const Popup = new CLE_POPUP();
    if (Popup == nullptr)
    {
        return STATUS_NO_MEMORY;
    }

    // allocate a buffer
    Popup->OldScreenSize = ScreenInfo->GetScreenBufferSize();
    Popup->OldContents = new CHAR_INFO[Popup->OldScreenSize.X * Size.Y];
    if (Popup->OldContents == nullptr)
    {
        delete Popup;
        return STATUS_NO_MEMORY;
    }

    // fill in popup structure
    InsertHeadList(&CommandHistory->PopupList, &Popup->ListLink);
    Popup->Region.Left = Origin.X;
    Popup->Region.Top = Origin.Y;
    Popup->Region.Right = (SHORT)(Origin.X + Size.X - 1);
    Popup->Region.Bottom = (SHORT)(Origin.Y + Size.Y - 1);
    Popup->Attributes = ScreenInfo->GetPopupAttributes()->GetLegacyAttributes();
    Popup->BottomIndex = COMMAND_INDEX_TO_NUM(CommandHistory->LastDisplayed, CommandHistory);

    // copy old contents
    SMALL_RECT TargetRect;
    TargetRect.Left = 0;
    TargetRect.Top = Popup->Region.Top;
    TargetRect.Right = Popup->OldScreenSize.X - 1;
    TargetRect.Bottom = Popup->Region.Bottom;
    ReadScreenBuffer(ScreenInfo, Popup->OldContents, &TargetRect);

    gci->PopupCount++;
    if (1 == gci->PopupCount)
    {
        // If this is the first popup to be shown, stop the cursor from appearing/blinking
        ScreenInfo->TextInfo->GetCursor()->SetIsPopupShown(TRUE);
    }

    DrawCommandListBorder(Popup, ScreenInfo);
    return STATUS_SUCCESS;
}

NTSTATUS EndPopup(_In_ PSCREEN_INFORMATION ScreenInfo, _In_ PCOMMAND_HISTORY CommandHistory)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    ASSERT(!CLE_NO_POPUPS(CommandHistory));
    if (CLE_NO_POPUPS(CommandHistory))
    {
        return STATUS_UNSUCCESSFUL;
    }

    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);

    // restore previous contents to screen
    COORD Size;
    Size.X = Popup->OldScreenSize.X;
    Size.Y = (SHORT)(Popup->Region.Bottom - Popup->Region.Top + 1);

    SMALL_RECT SourceRect;
    SourceRect.Left = 0;
    SourceRect.Top = Popup->Region.Top;
    SourceRect.Right = Popup->OldScreenSize.X - 1;
    SourceRect.Bottom = Popup->Region.Bottom;

    WriteScreenBuffer(ScreenInfo, Popup->OldContents, &SourceRect);
    WriteToScreen(ScreenInfo, SourceRect);

    // Free popup structure.
    RemoveEntryList(&Popup->ListLink);
    delete[] Popup->OldContents;
    delete Popup;
    gci->PopupCount--;

    if (gci->PopupCount == 0)
    {
        // Notify we're done showing popups.
        ScreenInfo->TextInfo->GetCursor()->SetIsPopupShown(FALSE);
    }

    return STATUS_SUCCESS;
}

void CleanUpPopups(_In_ COOKED_READ_DATA* const CookedReadData)
{
    PCOMMAND_HISTORY const CommandHistory = CookedReadData->_CommandHistory;
    if (CommandHistory == nullptr)
    {
        return;
    }

    while (!CLE_NO_POPUPS(CommandHistory))
    {
        EndPopup(CookedReadData->_pScreenInfo, CommandHistory);
    }
}

CommandLine::CommandLine()
{

}

CommandLine::~CommandLine()
{

}

CommandLine& CommandLine::Instance()
{
    static CommandLine c;
    return c;
}

bool CommandLine::IsEditLineEmpty() const
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    const COOKED_READ_DATA* const pTyped = gci->lpCookedReadData;

    if (nullptr == pTyped)
    {
        // If the cooked read data pointer is null, there is no edit line data and therefore it's empty.
        return true;
    }
    else if (0 == pTyped->_NumberOfVisibleChars)
    {
        // If we had a valid pointer, but there are no visible characters for the edit line, then it's empty.
        // Someone started editing and back spaced the whole line out so it exists, but has no data.
        return true;
    }
    else
    {
        return false;
    }
}

void CommandLine::Hide(_In_ bool const fUpdateFields)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    if (!IsEditLineEmpty())
    {
        COOKED_READ_DATA* CookedReadData = gci->lpCookedReadData;
        DeleteCommandLine(CookedReadData, fUpdateFields);
    }
}

void CommandLine::Show()
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    if (!IsEditLineEmpty())
    {
        COOKED_READ_DATA* CookedReadData = gci->lpCookedReadData;
        RedrawCommandLine(CookedReadData);
    }
}

void DeleteCommandLine(_Inout_ COOKED_READ_DATA* const pCookedReadData, _In_ const BOOL fUpdateFields)
{
    DWORD CharsToWrite = pCookedReadData->_NumberOfVisibleChars;
    COORD coordOriginalCursor = pCookedReadData->_OriginalCursorPosition;
    const COORD coordBufferSize = pCookedReadData->_pScreenInfo->GetScreenBufferSize();

    // catch the case where the current command has scrolled off the top of the screen.
    if (coordOriginalCursor.Y < 0)
    {
        CharsToWrite += coordBufferSize.X * coordOriginalCursor.Y;
        CharsToWrite += pCookedReadData->_OriginalCursorPosition.X;   // account for prompt
        pCookedReadData->_OriginalCursorPosition.X = 0;
        pCookedReadData->_OriginalCursorPosition.Y = 0;
        coordOriginalCursor.X = 0;
        coordOriginalCursor.Y = 0;
    }

    if (!CheckBisectStringW(pCookedReadData->_BackupLimit,
                            CharsToWrite,
                            coordBufferSize.X - pCookedReadData->_OriginalCursorPosition.X))
    {
        CharsToWrite++;
    }

    FillOutput(pCookedReadData->_pScreenInfo,
               L' ',
               coordOriginalCursor,
               CONSOLE_FALSE_UNICODE,    // faster than real unicode
               &CharsToWrite);

    if (fUpdateFields)
    {
        pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit;
        pCookedReadData->_BytesRead = 0;
        pCookedReadData->_CurrentPosition = 0;
        pCookedReadData->_NumberOfVisibleChars = 0;
    }

    pCookedReadData->_pScreenInfo->SetCursorPosition(pCookedReadData->_OriginalCursorPosition, TRUE);
}

void RedrawCommandLine(_Inout_ COOKED_READ_DATA* const pCookedReadData)
{
    if (pCookedReadData->_Echo)
    {
        // Draw the command line
        pCookedReadData->_OriginalCursorPosition = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();

        SHORT ScrollY = 0;
#pragma prefast(suppress:28931, "Status is not unused. It's used in debug assertions.")
        NTSTATUS Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                           pCookedReadData->_BackupLimit,
                                           pCookedReadData->_BackupLimit,
                                           pCookedReadData->_BackupLimit,
                                           &pCookedReadData->_BytesRead,
                                           &pCookedReadData->_NumberOfVisibleChars,
                                           pCookedReadData->_OriginalCursorPosition.X,
                                           WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                           &ScrollY);
        ASSERT(NT_SUCCESS(Status));

        pCookedReadData->_OriginalCursorPosition.Y += ScrollY;

        // Move the cursor back to the right position
        COORD CursorPosition = pCookedReadData->_OriginalCursorPosition;
        CursorPosition.X += (SHORT)RetrieveTotalNumberOfSpaces(pCookedReadData->_OriginalCursorPosition.X,
                                                               pCookedReadData->_BackupLimit,
                                                               pCookedReadData->_CurrentPosition);
        if (CheckBisectStringW(pCookedReadData->_BackupLimit,
                               pCookedReadData->_CurrentPosition,
                               pCookedReadData->_pScreenInfo->GetScreenBufferSize().X - pCookedReadData->_OriginalCursorPosition.X))
        {
            CursorPosition.X++;
        }
        Status = AdjustCursorPosition(pCookedReadData->_pScreenInfo, CursorPosition, TRUE, nullptr);
        ASSERT(NT_SUCCESS(Status));
    }
}

NTSTATUS RetrieveNthCommand(_In_ PCOMMAND_HISTORY CommandHistory, _In_ SHORT Index, // index, not command number
                            _In_reads_bytes_(BufferSize)
                            PWCHAR Buffer, _In_ ULONG BufferSize, _Out_ PULONG CommandSize)
{
    ASSERT(Index < CommandHistory->NumberOfCommands);
    CommandHistory->LastDisplayed = Index;
    PCOMMAND const CommandRecord = CommandHistory->Commands[Index];
    if (CommandRecord->CommandLength > (USHORT) BufferSize)
    {
        *CommandSize = (USHORT)BufferSize; // room for CRLF?
    }
    else
    {
        *CommandSize = CommandRecord->CommandLength;
    }

    memmove(Buffer, CommandRecord->Command, *CommandSize);
    return STATUS_SUCCESS;
}

// Routine Description:
// - This routine copies the commandline specified by Index into the cooked read buffer
void SetCurrentCommandLine(_In_ COOKED_READ_DATA* const CookedReadData, _In_ SHORT Index) // index, not command number
{
    DeleteCommandLine(CookedReadData, TRUE);
#pragma prefast(suppress:28931, "Status is not unused. Used by assertions.")
    NTSTATUS Status = RetrieveNthCommand(CookedReadData->_CommandHistory, Index, CookedReadData->_BackupLimit, CookedReadData->_BufferSize, &CookedReadData->_BytesRead);
    ASSERT(NT_SUCCESS(Status));
    ASSERT(CookedReadData->_BackupLimit == CookedReadData->_BufPtr);
    if (CookedReadData->_Echo)
    {
        SHORT ScrollY = 0;
        Status = WriteCharsLegacy(CookedReadData->_pScreenInfo,
                                  CookedReadData->_BackupLimit,
                                  CookedReadData->_BufPtr,
                                  CookedReadData->_BufPtr,
                                  &CookedReadData->_BytesRead,
                                  &CookedReadData->_NumberOfVisibleChars,
                                  CookedReadData->_OriginalCursorPosition.X,
                                  WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                  &ScrollY);
        ASSERT(NT_SUCCESS(Status));
        CookedReadData->_OriginalCursorPosition.Y += ScrollY;
    }

    DWORD const CharsToWrite = CookedReadData->_BytesRead / sizeof(WCHAR);
    CookedReadData->_CurrentPosition = CharsToWrite;
    CookedReadData->_BufPtr = CookedReadData->_BackupLimit + CharsToWrite;
}

// Routine Description:
// - This routine handles the command list popup.  It returns when we're out of input or the user has selected a command line.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - CONSOLE_STATUS_READ_COMPLETE - user hit return
NTSTATUS ProcessCommandListInput(_In_ COOKED_READ_DATA* const pCookedReadData)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PCOMMAND_HISTORY const pCommandHistory = pCookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(pCommandHistory->PopupList.Flink, CLE_POPUP, ListLink);
    NTSTATUS Status = STATUS_SUCCESS;
    INPUT_READ_HANDLE_DATA* const pInputReadHandleData = pCookedReadData->GetInputReadHandleData();
    InputBuffer* const pInputBuffer = pCookedReadData->GetInputBuffer();

    for (;;)
    {
        WCHAR Char;
        bool commandLinePopupKeys = false;

        Status = GetChar(pInputBuffer,
                         &Char,
                         true,
                         nullptr,
                         &commandLinePopupKeys,
                         nullptr);
        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                pCookedReadData->_BytesRead = 0;
            }
            return Status;
        }

        SHORT Index;
        if (commandLinePopupKeys)
        {
            switch (Char)
            {
            case VK_F9:
            {
                // prompt the user to enter the desired command number. copy that command to the command line.
                COORD PopupSize;
                if (pCookedReadData->_CommandHistory &&
                    pCookedReadData->_pScreenInfo->GetScreenBufferSize().X >= MINIMUM_COMMAND_PROMPT_SIZE + 2)
                {
                    // 2 is for border
                    PopupSize.X = COMMAND_NUMBER_PROMPT_LENGTH + COMMAND_NUMBER_LENGTH;
                    PopupSize.Y = 1;
                    Status = BeginPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory, PopupSize);
                    if (NT_SUCCESS(Status))
                    {
                        // CommandNumberPopup does EndPopup call
                        return CommandNumberPopup(pCookedReadData);
                    }
                }
                break;
            }
            case VK_ESCAPE:
                EndPopup(pCookedReadData->_pScreenInfo, pCommandHistory);
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            case VK_UP:
                UpdateCommandListPopup(-1, &Popup->CurrentCommand, pCommandHistory, Popup, pCookedReadData->_pScreenInfo, 0);
                break;
            case VK_DOWN:
                UpdateCommandListPopup(1, &Popup->CurrentCommand, pCommandHistory, Popup, pCookedReadData->_pScreenInfo, 0);
                break;
            case VK_END:
                // Move waaay forward, UpdateCommandListPopup() can handle it.
                UpdateCommandListPopup((SHORT)(pCommandHistory->NumberOfCommands),
                                       &Popup->CurrentCommand,
                                       pCommandHistory,
                                       Popup,
                                       pCookedReadData->_pScreenInfo,
                                       0);
                break;
            case VK_HOME:
                // Move waaay back, UpdateCommandListPopup() can handle it.
                UpdateCommandListPopup((SHORT)-(pCommandHistory->NumberOfCommands),
                                       &Popup->CurrentCommand,
                                       pCommandHistory,
                                       Popup,
                                       pCookedReadData->_pScreenInfo,
                                       0);
                break;
            case VK_PRIOR:
                UpdateCommandListPopup((SHORT)-POPUP_SIZE_Y(Popup),
                                       &Popup->CurrentCommand,
                                       pCommandHistory,
                                       Popup,
                                       pCookedReadData->_pScreenInfo,
                                       0);
                break;
            case VK_NEXT:
                UpdateCommandListPopup(POPUP_SIZE_Y(Popup),
                                       &Popup->CurrentCommand,
                                       pCommandHistory,
                                       Popup,
                                       pCookedReadData->_pScreenInfo, 0);
                break;
            case VK_LEFT:
            case VK_RIGHT:
                Index = Popup->CurrentCommand;
                EndPopup(pCookedReadData->_pScreenInfo, pCommandHistory);
                SetCurrentCommandLine(pCookedReadData, Index);
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            default:
                break;
            }
        }
        else if (Char == UNICODE_CARRIAGERETURN)
        {
            ULONG i, lStringLength;
            DWORD LineCount = 1;
            Index = Popup->CurrentCommand;
            EndPopup(pCookedReadData->_pScreenInfo, pCommandHistory);
            SetCurrentCommandLine(pCookedReadData, Index);
            lStringLength = pCookedReadData->_BytesRead;
            ProcessCookedReadInput(pCookedReadData, UNICODE_CARRIAGERETURN, 0, &Status);
            // complete read
            if (pCookedReadData->_Echo)
            {
                // check for alias
                i = pCookedReadData->_BufferSize;
                if (NT_SUCCESS(MatchAndCopyAlias(pCookedReadData->_BackupLimit,
                                                 (USHORT)lStringLength,
                                                 pCookedReadData->_BackupLimit,
                                                 (PUSHORT)& i,
                                                 pCookedReadData->ExeName,
                                                 pCookedReadData->ExeNameLength,
                                                 &LineCount)))
                {
                    pCookedReadData->_BytesRead = i;
                }
            }

            Status = STATUS_SUCCESS;
            DWORD dwNumBytes;
            if (pCookedReadData->_BytesRead > pCookedReadData->_UserBufferSize || LineCount > 1)
            {
                if (LineCount > 1)
                {
                    PWSTR Tmp;
                    SetFlag(pInputReadHandleData->InputHandleFlags, INPUT_READ_HANDLE_DATA::HandleFlags::MultiLineInput);
                    for (Tmp = pCookedReadData->_BackupLimit; *Tmp != UNICODE_LINEFEED; Tmp++)
                        ASSERT(Tmp < (pCookedReadData->_BackupLimit + pCookedReadData->_BytesRead));
                    dwNumBytes = (ULONG)(Tmp - pCookedReadData->_BackupLimit + 1) * sizeof(*Tmp);
                }
                else
                {
                    dwNumBytes = pCookedReadData->_UserBufferSize;
                }
                SetFlag(pInputReadHandleData->InputHandleFlags, INPUT_READ_HANDLE_DATA::HandleFlags::InputPending);
                pInputReadHandleData->BufPtr = pCookedReadData->_BackupLimit;
                pInputReadHandleData->BytesAvailable = pCookedReadData->_BytesRead - dwNumBytes;
                pInputReadHandleData->CurrentBufPtr = (PWCHAR)((PBYTE)pCookedReadData->_BackupLimit + dwNumBytes);
                memmove(pCookedReadData->_UserBuffer, pCookedReadData->_BackupLimit, dwNumBytes);
            }
            else
            {
                dwNumBytes = pCookedReadData->_BytesRead;
                memmove(pCookedReadData->_UserBuffer, pCookedReadData->_BackupLimit, dwNumBytes);
            }

            if (!pCookedReadData->_fIsUnicode)
            {
                PCHAR TransBuffer;

                // If ansi, translate string.
                TransBuffer = (PCHAR) new BYTE[dwNumBytes];
                if (TransBuffer == nullptr)
                {
                    return STATUS_NO_MEMORY;
                }

                dwNumBytes = (ULONG)ConvertToOem(gci->CP,
                                                 pCookedReadData->_UserBuffer,
                                                 dwNumBytes / sizeof(WCHAR),
                                                 TransBuffer,
                                                 dwNumBytes);
                memmove(pCookedReadData->_UserBuffer, TransBuffer, dwNumBytes);
                delete[] TransBuffer;
            }

            *(pCookedReadData->pdwNumBytes) = dwNumBytes;

            return CONSOLE_STATUS_READ_COMPLETE;
        }
        else
        {
            Index = FindMatchingCommand(pCookedReadData->_CommandHistory, &Char, 1 * sizeof(WCHAR), Popup->CurrentCommand, FMCFL_JUST_LOOKING);
            if (Index != -1)
            {
                UpdateCommandListPopup((SHORT)(Index - Popup->CurrentCommand),
                                       &Popup->CurrentCommand,
                                       pCommandHistory,
                                       Popup,
                                       pCookedReadData->_pScreenInfo,
                                       UCLP_WRAP);
            }
        }
    }
}

// Routine Description:
// - This routine handles the delete from cursor to char char popup.  It returns when we're out of input or the user has entered a char.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - CONSOLE_STATUS_READ_COMPLETE - user hit return
NTSTATUS ProcessCopyFromCharInput(_In_ COOKED_READ_DATA* const pCookedReadData)
{
    NTSTATUS Status = STATUS_SUCCESS;
    InputBuffer* const pInputBuffer = pCookedReadData->GetInputBuffer();
    for (;;)
    {
        WCHAR Char;
        bool commandLinePopupKeys = false;
        Status = GetChar(pInputBuffer,
                         &Char,
                         TRUE,
                         nullptr,
                         &commandLinePopupKeys,
                         nullptr);
        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                pCookedReadData->_BytesRead = 0;
            }

            return Status;
        }

        if (commandLinePopupKeys)
        {
            switch (Char)
            {
            case VK_ESCAPE:
                EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            }
        }

        EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);

        int i;  // char index (not byte)
        // delete from cursor up to specified char
        for (i = pCookedReadData->_CurrentPosition + 1; i < (int)(pCookedReadData->_BytesRead / sizeof(WCHAR)); i++)
        {
            if (pCookedReadData->_BackupLimit[i] == Char)
            {
                break;
            }
        }

        if (i != (int)(pCookedReadData->_BytesRead / sizeof(WCHAR) + 1))
        {
            COORD CursorPosition;

            // save cursor position
            CursorPosition = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();

            // Delete commandline.
            DeleteCommandLine(pCookedReadData, FALSE);

            // Delete chars.
            memmove(&pCookedReadData->_BackupLimit[pCookedReadData->_CurrentPosition],
                    &pCookedReadData->_BackupLimit[i],
                    pCookedReadData->_BytesRead - (i * sizeof(WCHAR)));
            pCookedReadData->_BytesRead -= (i - pCookedReadData->_CurrentPosition) * sizeof(WCHAR);

            // Write commandline.
            if (pCookedReadData->_Echo)
            {
                Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                          pCookedReadData->_BackupLimit,
                                          pCookedReadData->_BackupLimit,
                                          pCookedReadData->_BackupLimit,
                                          &pCookedReadData->_BytesRead,
                                          &pCookedReadData->_NumberOfVisibleChars,
                                          pCookedReadData->_OriginalCursorPosition.X,
                                          WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                          nullptr);
                ASSERT(NT_SUCCESS(Status));
            }

            // restore cursor position
            Status = pCookedReadData->_pScreenInfo->SetCursorPosition(CursorPosition, TRUE);
            ASSERT(NT_SUCCESS(Status));
        }

        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
}

// Routine Description:
// - This routine handles the delete char popup.  It returns when we're out of input or the user has entered a char.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - CONSOLE_STATUS_READ_COMPLETE - user hit return
NTSTATUS ProcessCopyToCharInput(_In_ COOKED_READ_DATA* const pCookedReadData)
{
    NTSTATUS Status = STATUS_SUCCESS;
    InputBuffer* const pInputBuffer = pCookedReadData->GetInputBuffer();
    for (;;)
    {
        WCHAR Char;
        bool commandLinePopupKeys = false;
        Status = GetChar(pInputBuffer,
                         &Char,
                         true,
                         nullptr,
                         &commandLinePopupKeys,
                         nullptr);
        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                pCookedReadData->_BytesRead = 0;
            }
            return Status;
        }

        if (commandLinePopupKeys)
        {
            switch (Char)
            {
            case VK_ESCAPE:
                EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            }
        }

        EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);

        // copy up to specified char
        PCOMMAND const LastCommand = GetLastCommand(pCookedReadData->_CommandHistory);
        if (LastCommand)
        {
            int i;

            // find specified char in last command
            for (i = pCookedReadData->_CurrentPosition + 1; i < (int)(LastCommand->CommandLength / sizeof(WCHAR)); i++)
            {
                if (LastCommand->Command[i] == Char)
                {
                    break;
                }
            }

            // If we found it, copy up to it.
            if (i < (int)(LastCommand->CommandLength / sizeof(WCHAR)) &&
                ((USHORT)(LastCommand->CommandLength / sizeof(WCHAR)) > ((USHORT)pCookedReadData->_CurrentPosition)))
            {
                int j = i - pCookedReadData->_CurrentPosition;
                ASSERT(j > 0);
                memmove(pCookedReadData->_BufPtr,
                        &LastCommand->Command[pCookedReadData->_CurrentPosition],
                        j * sizeof(WCHAR));
                pCookedReadData->_CurrentPosition += j;
                j *= sizeof(WCHAR);
                pCookedReadData->_BytesRead = max(pCookedReadData->_BytesRead,
                                                  pCookedReadData->_CurrentPosition * sizeof(WCHAR));
                if (pCookedReadData->_Echo)
                {
                    DWORD NumSpaces;
                    SHORT ScrollY = 0;

                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BufPtr,
                                              pCookedReadData->_BufPtr,
                                              (PDWORD)&j,
                                              &NumSpaces,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              &ScrollY);
                    ASSERT(NT_SUCCESS(Status));
                    pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                    pCookedReadData->_NumberOfVisibleChars += NumSpaces;
                }

                pCookedReadData->_BufPtr += j / sizeof(WCHAR);
            }
        }

        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
}

// Routine Description:
// - This routine handles the command number selection popup.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - CONSOLE_STATUS_READ_COMPLETE - user hit return
NTSTATUS ProcessCommandNumberInput(_In_ COOKED_READ_DATA* const pCookedReadData)
{
    PCOMMAND_HISTORY const CommandHistory = pCookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);
    NTSTATUS Status = STATUS_SUCCESS;
    InputBuffer* const pInputBuffer = pCookedReadData->GetInputBuffer();
    for (;;)
    {
        WCHAR Char;
        bool commandLinePopupKeys = false;

        Status = GetChar(pInputBuffer,
                         &Char,
                         TRUE,
                         nullptr,
                         &commandLinePopupKeys,
                         nullptr);
        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                pCookedReadData->_BytesRead = 0;
            }
            return Status;
        }

        if (Char >= L'0' && Char <= L'9')
        {
            if (Popup->NumberRead < 5)
            {
                DWORD CharsToWrite = sizeof(WCHAR);
                const TextAttribute realAttributes = pCookedReadData->_pScreenInfo->GetAttributes();
                pCookedReadData->_pScreenInfo->SetAttributes(Popup->Attributes);
                DWORD NumSpaces;
                Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                          Popup->NumberBuffer,
                                          &Popup->NumberBuffer[Popup->NumberRead],
                                          &Char,
                                          &CharsToWrite,
                                          &NumSpaces,
                                          pCookedReadData->_OriginalCursorPosition.X,
                                          WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                          nullptr);
                ASSERT(NT_SUCCESS(Status));
                pCookedReadData->_pScreenInfo->SetAttributes(realAttributes);
                Popup->NumberBuffer[Popup->NumberRead] = Char;
                Popup->NumberRead += 1;
            }
        }
        else if (Char == UNICODE_BACKSPACE)
        {
            if (Popup->NumberRead > 0)
            {
                DWORD CharsToWrite = sizeof(WCHAR);
                const TextAttribute realAttributes = pCookedReadData->_pScreenInfo->GetAttributes();
                pCookedReadData->_pScreenInfo->SetAttributes(Popup->Attributes);
                DWORD NumSpaces;
                Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                          Popup->NumberBuffer,
                                          &Popup->NumberBuffer[Popup->NumberRead],
                                          &Char,
                                          &CharsToWrite,
                                          &NumSpaces,
                                          pCookedReadData->_OriginalCursorPosition.X,
                                          WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                          nullptr);

                ASSERT(NT_SUCCESS(Status));
                pCookedReadData->_pScreenInfo->SetAttributes(realAttributes);
                Popup->NumberBuffer[Popup->NumberRead] = (WCHAR)' ';
                Popup->NumberRead -= 1;
            }
        }
        else if (Char == (WCHAR)VK_ESCAPE)
        {
            EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
            if (!CLE_NO_POPUPS(CommandHistory))
            {
                EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
            }

            // Note that CookedReadData's OriginalCursorPosition is the position before ANY text was entered on the edit line.
            // We want to use the position before the cursor was moved for this popup handler specifically, which may be *anywhere* in the edit line
            // and will be synchronized with the pointers in the CookedReadData structure (BufPtr, etc.)
            pCookedReadData->_pScreenInfo->SetCursorPosition(pCookedReadData->BeforeDialogCursorPosition, TRUE);
        }
        else if (Char == UNICODE_CARRIAGERETURN)
        {
            CHAR NumberBuffer[6];
            int i;

            // This is guaranteed above.
            __analysis_assume(Popup->NumberRead < 6);
            for (i = 0; i < Popup->NumberRead; i++)
            {
                ASSERT(i < ARRAYSIZE(NumberBuffer));
                NumberBuffer[i] = (CHAR)Popup->NumberBuffer[i];
            }
            NumberBuffer[i] = 0;

            SHORT CommandNumber = (SHORT)atoi(NumberBuffer);
            if ((WORD)CommandNumber >= (WORD)pCookedReadData->_CommandHistory->NumberOfCommands)
            {
                CommandNumber = (SHORT)(pCookedReadData->_CommandHistory->NumberOfCommands - 1);
            }

            EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
            if (!CLE_NO_POPUPS(CommandHistory))
            {
                EndPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory);
            }
            SetCurrentCommandLine(pCookedReadData, COMMAND_NUM_TO_INDEX(CommandNumber, pCookedReadData->_CommandHistory));
        }
        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
}

// Routine Description:
// - This routine handles the command list popup.  It puts up the popup, then calls ProcessCommandListInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
NTSTATUS CommandListPopup(_In_ COOKED_READ_DATA* const CookedReadData)
{
    PCOMMAND_HISTORY const CommandHistory = CookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);

    SHORT const CurrentCommand = COMMAND_INDEX_TO_NUM(CommandHistory->LastDisplayed, CommandHistory);

    if (CurrentCommand < (SHORT)(CommandHistory->NumberOfCommands - POPUP_SIZE_Y(Popup)))
    {
        Popup->BottomIndex = (SHORT)(max(CurrentCommand, POPUP_SIZE_Y(Popup) - 1));
    }
    else
    {
        Popup->BottomIndex = (SHORT)(CommandHistory->NumberOfCommands - 1);
    }
    Popup->CurrentCommand = CommandHistory->LastDisplayed;
    DrawCommandListPopup(Popup, CommandHistory->LastDisplayed, CommandHistory, CookedReadData->_pScreenInfo);
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE)ProcessCommandListInput;
    return ProcessCommandListInput(CookedReadData);
}

VOID DrawPromptPopup(_In_ PCLE_POPUP Popup, _In_ PSCREEN_INFORMATION ScreenInfo, _In_reads_(PromptLength) PWCHAR Prompt, _In_ ULONG PromptLength)
{
    // Draw empty popup.
    COORD WriteCoord;
    WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1);
    ULONG lStringLength = POPUP_SIZE_X(Popup);
    for (SHORT i = 0; i < POPUP_SIZE_Y(Popup); i++)
    {
        FillOutput(ScreenInfo, Popup->Attributes.GetLegacyAttributes(), WriteCoord, CONSOLE_ATTRIBUTE, &lStringLength);
        FillOutput(ScreenInfo, (WCHAR)' ', WriteCoord, CONSOLE_FALSE_UNICODE,   // faster that real unicode
                   &lStringLength);

        WriteCoord.Y += 1;
    }

    WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1);

    // write prompt to screen
    lStringLength = PromptLength;
    if (lStringLength > (ULONG)POPUP_SIZE_X(Popup))
    {
        lStringLength = (ULONG)(POPUP_SIZE_X(Popup));
    }

    WriteOutputString(ScreenInfo, Prompt, WriteCoord, CONSOLE_REAL_UNICODE, &lStringLength, nullptr);
}

// Routine Description:
// - This routine handles the "delete up to this char" popup.  It puts up the popup, then calls ProcessCopyFromCharInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
NTSTATUS CopyFromCharPopup(_In_ COOKED_READ_DATA* CookedReadData)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    WCHAR ItemString[70];
    int ItemLength = 0;
    LANGID LangId;
    NTSTATUS Status = GetConsoleLangId(gci->OutputCP, &LangId);
    if (NT_SUCCESS(Status))
    {
        ItemLength = LoadStringEx(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF4, ItemString, ARRAYSIZE(ItemString), LangId);
    }

    if (!NT_SUCCESS(Status) || ItemLength == 0)
    {
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF4, ItemString, ARRAYSIZE(ItemString));
    }

    PCOMMAND_HISTORY const CommandHistory = CookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);

    DrawPromptPopup(Popup, CookedReadData->_pScreenInfo, ItemString, ItemLength);
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE)ProcessCopyFromCharInput;

    return ProcessCopyFromCharInput(CookedReadData);
}

// Routine Description:
// - This routine handles the "copy up to this char" popup.  It puts up the popup, then calls ProcessCopyToCharInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
NTSTATUS CopyToCharPopup(_In_ COOKED_READ_DATA* CookedReadData)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    WCHAR ItemString[70];
    int ItemLength = 0;
    LANGID LangId;

    NTSTATUS Status = GetConsoleLangId(gci->OutputCP, &LangId);
    if (NT_SUCCESS(Status))
    {
        ItemLength = LoadStringEx(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF2, ItemString, ARRAYSIZE(ItemString), LangId);
    }

    if (!NT_SUCCESS(Status) || ItemLength == 0)
    {
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF2, ItemString, ARRAYSIZE(ItemString));
    }

    PCOMMAND_HISTORY const CommandHistory = CookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);
    DrawPromptPopup(Popup, CookedReadData->_pScreenInfo, ItemString, ItemLength);
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE)ProcessCopyToCharInput;
    return ProcessCopyToCharInput(CookedReadData);
}

// Routine Description:
// - This routine handles the "enter command number" popup.  It puts up the popup, then calls ProcessCommandNumberInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
NTSTATUS CommandNumberPopup(_In_ COOKED_READ_DATA* const CookedReadData)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    WCHAR ItemString[70];
    int ItemLength = 0;
    LANGID LangId;

    NTSTATUS Status = GetConsoleLangId(gci->OutputCP, &LangId);
    if (NT_SUCCESS(Status))
    {
        ItemLength = LoadStringEx(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF9, ItemString, ARRAYSIZE(ItemString), LangId);
    }
    if (!NT_SUCCESS(Status) || ItemLength == 0)
    {
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals()->hInstance, ID_CONSOLE_MSGCMDLINEF9, ItemString, ARRAYSIZE(ItemString));
    }

    PCOMMAND_HISTORY const CommandHistory = CookedReadData->_CommandHistory;
    PCLE_POPUP const Popup = CONTAINING_RECORD(CommandHistory->PopupList.Flink, CLE_POPUP, ListLink);

    if (ItemLength > POPUP_SIZE_X(Popup) - COMMAND_NUMBER_LENGTH)
    {
        ItemLength = POPUP_SIZE_X(Popup) - COMMAND_NUMBER_LENGTH;
    }
    DrawPromptPopup(Popup, CookedReadData->_pScreenInfo, ItemString, ItemLength);

    // Save the original cursor position in case the user cancels out of the dialog
    CookedReadData->BeforeDialogCursorPosition = CookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();

    // Move the cursor into the dialog so the user can type multiple characters for the command number
    COORD CursorPosition;
    CursorPosition.X = (SHORT)(Popup->Region.Right - MINIMUM_COMMAND_PROMPT_SIZE);
    CursorPosition.Y = (SHORT)(Popup->Region.Top + 1);
    CookedReadData->_pScreenInfo->SetCursorPosition(CursorPosition, TRUE);

    // Prepare the popup
    Popup->NumberRead = 0;
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE)ProcessCommandNumberInput;

    // Transfer control to the handler routine
    return ProcessCommandNumberInput(CookedReadData);
}


PCOMMAND GetLastCommand(_In_ PCOMMAND_HISTORY CommandHistory)
{
    if (CommandHistory->NumberOfCommands == 0)
    {
        return nullptr;
    }
    else
    {
        return CommandHistory->Commands[CommandHistory->LastDisplayed];
    }
}

void EmptyCommandHistory(_In_opt_ PCOMMAND_HISTORY CommandHistory)
{
    if (CommandHistory == nullptr)
    {
        return;
    }

    for (SHORT i = 0; i < CommandHistory->NumberOfCommands; i++)
    {
        delete[] CommandHistory->Commands[i];
    }

    CommandHistory->NumberOfCommands = 0;
    CommandHistory->LastAdded = -1;
    CommandHistory->LastDisplayed = -1;
    CommandHistory->FirstCommand = 0;
    CommandHistory->Flags = CLE_RESET;
}

BOOL AtFirstCommand(_In_ PCOMMAND_HISTORY CommandHistory)
{
    if (CommandHistory == nullptr)
    {
        return FALSE;
    }

    if (CommandHistory->Flags & CLE_RESET)
    {
        return FALSE;
    }

    SHORT i = (SHORT)(CommandHistory->LastDisplayed - 1);
    if (i == -1)
    {
        i = (SHORT)(CommandHistory->NumberOfCommands - 1);
    }

    return (i == CommandHistory->LastAdded);
}

BOOL AtLastCommand(_In_ PCOMMAND_HISTORY CommandHistory)
{
    if (CommandHistory == nullptr)
    {
        return FALSE;
    }
    else
    {
        return (CommandHistory->LastDisplayed == CommandHistory->LastAdded);
    }
}

// TODO: [MSFT:4586207] Clean up this mess -- needs helpers. http://osgvsowi/4586207
// Routine Description:
// - This routine process command line editing keys.
// Return Value:
// - CONSOLE_STATUS_WAIT - CommandListPopup ran out of input
// - CONSOLE_STATUS_READ_COMPLETE - user hit <enter> in CommandListPopup
// - STATUS_SUCCESS - everything's cool
NTSTATUS ProcessCommandLine(_In_ COOKED_READ_DATA* pCookedReadData,
                            _In_ WCHAR wch,
                            _In_ const DWORD dwKeyState)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    COORD CurrentPosition = { 0 };
    DWORD CharsToWrite;
    NTSTATUS Status;
    SHORT ScrollY = 0;
    const SHORT sScreenBufferSizeX = pCookedReadData->_pScreenInfo->GetScreenBufferSize().X;

    BOOL UpdateCursorPosition = FALSE;
    if (wch == VK_F7 && (dwKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) == 0)
    {
        COORD PopupSize;

        if (pCookedReadData->_CommandHistory && pCookedReadData->_CommandHistory->NumberOfCommands)
        {
            PopupSize.X = 40;
            PopupSize.Y = 10;
            Status = BeginPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory, PopupSize);
            if (NT_SUCCESS(Status))
            {
                // CommandListPopup does EndPopup call
                return CommandListPopup(pCookedReadData);
            }
        }
    }
    else
    {
        switch (wch)
        {
        case VK_ESCAPE:
            DeleteCommandLine(pCookedReadData, TRUE);
            break;
        case VK_UP:
        case VK_DOWN:
        case VK_F5:
            if (wch == VK_F5)
                wch = VK_UP;
            // for doskey compatibility, buffer isn't circular
            if (wch == VK_UP && !AtFirstCommand(pCookedReadData->_CommandHistory) || wch == VK_DOWN && !AtLastCommand(pCookedReadData->_CommandHistory))
            {
                DeleteCommandLine(pCookedReadData, TRUE);
                Status = RetrieveCommand(pCookedReadData->_CommandHistory,
                                         wch,
                                         pCookedReadData->_BackupLimit,
                                         pCookedReadData->_BufferSize,
                                         &pCookedReadData->_BytesRead);
                ASSERT(pCookedReadData->_BackupLimit == pCookedReadData->_BufPtr);
                if (pCookedReadData->_Echo)
                {
                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BufPtr,
                                              pCookedReadData->_BufPtr,
                                              &pCookedReadData->_BytesRead,
                                              &pCookedReadData->_NumberOfVisibleChars,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              &ScrollY);
                    ASSERT(NT_SUCCESS(Status));
                    pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                }
                CharsToWrite = pCookedReadData->_BytesRead / sizeof(WCHAR);
                pCookedReadData->_CurrentPosition = CharsToWrite;
                pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit + CharsToWrite;
            }
            break;
        case VK_PRIOR:
        case VK_NEXT:
            if (pCookedReadData->_CommandHistory && pCookedReadData->_CommandHistory->NumberOfCommands)
            {
                // display oldest or newest command
                SHORT CommandNumber;
                if (wch == VK_PRIOR)
                {
                    CommandNumber = 0;
                }
                else
                {
                    CommandNumber = (SHORT)(pCookedReadData->_CommandHistory->NumberOfCommands - 1);
                }
                DeleteCommandLine(pCookedReadData, TRUE);
                Status = RetrieveNthCommand(pCookedReadData->_CommandHistory,
                                            COMMAND_NUM_TO_INDEX(CommandNumber, pCookedReadData->_CommandHistory),
                                            pCookedReadData->_BackupLimit,
                                            pCookedReadData->_BufferSize,
                                            &pCookedReadData->_BytesRead);
                ASSERT(pCookedReadData->_BackupLimit == pCookedReadData->_BufPtr);
                if (pCookedReadData->_Echo)
                {
                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BufPtr,
                                              pCookedReadData->_BufPtr,
                                              &pCookedReadData->_BytesRead,
                                              &pCookedReadData->_NumberOfVisibleChars,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              &ScrollY);
                    ASSERT(NT_SUCCESS(Status));
                    pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                }
                CharsToWrite = pCookedReadData->_BytesRead / sizeof(WCHAR);
                pCookedReadData->_CurrentPosition = CharsToWrite;
                pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit + CharsToWrite;
            }
            break;
        case VK_END:
            if (dwKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
            {
                DeleteCommandLine(pCookedReadData, FALSE);
                pCookedReadData->_BytesRead = pCookedReadData->_CurrentPosition * sizeof(WCHAR);
                if (pCookedReadData->_Echo)
                {
                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              &pCookedReadData->_BytesRead,
                                              &pCookedReadData->_NumberOfVisibleChars,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              nullptr);
                    ASSERT(NT_SUCCESS(Status));
                }
            }
            else
            {
                pCookedReadData->_CurrentPosition = pCookedReadData->_BytesRead / sizeof(WCHAR);
                pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit + pCookedReadData->_CurrentPosition;
                CurrentPosition.X = (SHORT)(pCookedReadData->_OriginalCursorPosition.X + pCookedReadData->_NumberOfVisibleChars);
                CurrentPosition.Y = pCookedReadData->_OriginalCursorPosition.Y;
                if (CheckBisectProcessW(pCookedReadData->_pScreenInfo,
                                        pCookedReadData->_BackupLimit,
                                        pCookedReadData->_CurrentPosition,
                                        sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X,
                                        pCookedReadData->_OriginalCursorPosition.X,
                                        TRUE))
                {
                    CurrentPosition.X++;
                }
                UpdateCursorPosition = TRUE;
            }
            break;
        case VK_HOME:
            if (dwKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
            {
                DeleteCommandLine(pCookedReadData, FALSE);
                pCookedReadData->_BytesRead -= pCookedReadData->_CurrentPosition * sizeof(WCHAR);
                pCookedReadData->_CurrentPosition = 0;
                memmove(pCookedReadData->_BackupLimit, pCookedReadData->_BufPtr, pCookedReadData->_BytesRead);
                pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit;
                if (pCookedReadData->_Echo)
                {
                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              &pCookedReadData->_BytesRead,
                                              &pCookedReadData->_NumberOfVisibleChars,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              nullptr);
                    ASSERT(NT_SUCCESS(Status));
                }
                CurrentPosition = pCookedReadData->_OriginalCursorPosition;
                UpdateCursorPosition = TRUE;
            }
            else
            {
                pCookedReadData->_CurrentPosition = 0;
                pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit;
                CurrentPosition = pCookedReadData->_OriginalCursorPosition;
                UpdateCursorPosition = TRUE;
            }
            break;
        case VK_LEFT:
            if (dwKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
            {
                PWCHAR LastWord;
                BOOL NonSpaceCharSeen = FALSE;
                if (pCookedReadData->_BufPtr != pCookedReadData->_BackupLimit)
                {
                    if (!gci->GetExtendedEditKey())
                    {
                        LastWord = pCookedReadData->_BufPtr - 1;
                        while (LastWord != pCookedReadData->_BackupLimit)
                        {
                            if (!IS_WORD_DELIM(*LastWord))
                            {
                                NonSpaceCharSeen = TRUE;
                            }
                            else
                            {
                                if (NonSpaceCharSeen)
                                {
                                    break;
                                }
                            }
                            LastWord--;
                        }
                        if (LastWord != pCookedReadData->_BackupLimit)
                        {
                            pCookedReadData->_BufPtr = LastWord + 1;
                        }
                        else
                        {
                            pCookedReadData->_BufPtr = LastWord;
                        }
                    }
                    else
                    {
                        // A bit better word skipping.
                        LastWord = pCookedReadData->_BufPtr - 1;
                        if (LastWord != pCookedReadData->_BackupLimit)
                        {
                            if (*LastWord == L' ')
                            {
                                // Skip spaces, until the non-space character is found.
                                while (--LastWord != pCookedReadData->_BackupLimit)
                                {
                                    ASSERT(LastWord > pCookedReadData->_BackupLimit);
                                    if (*LastWord != L' ')
                                    {
                                        break;
                                    }
                                }
                            }
                            if (LastWord != pCookedReadData->_BackupLimit)
                            {
                                if (IS_WORD_DELIM(*LastWord))
                                {
                                    // Skip WORD_DELIMs until space or non WORD_DELIM is found.
                                    while (--LastWord != pCookedReadData->_BackupLimit)
                                    {
                                        ASSERT(LastWord > pCookedReadData->_BackupLimit);
                                        if (*LastWord == L' ' || !IS_WORD_DELIM(*LastWord))
                                        {
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    // Skip the regular words
                                    while (--LastWord != pCookedReadData->_BackupLimit)
                                    {
                                        ASSERT(LastWord > pCookedReadData->_BackupLimit);
                                        if (IS_WORD_DELIM(*LastWord))
                                        {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        ASSERT(LastWord >= pCookedReadData->_BackupLimit);
                        if (LastWord != pCookedReadData->_BackupLimit)
                        {
                            /*
                             * LastWord is currently pointing to the last character
                             * of the previous word, unless it backed up to the beginning
                             * of the buffer.
                             * Let's increment LastWord so that it points to the expeced
                             * insertion point.
                             */
                            ++LastWord;
                        }
                        pCookedReadData->_BufPtr = LastWord;
                    }
                    pCookedReadData->_CurrentPosition = (ULONG)(pCookedReadData->_BufPtr - pCookedReadData->_BackupLimit);
                    CurrentPosition = pCookedReadData->_OriginalCursorPosition;
                    CurrentPosition.X = (SHORT)(CurrentPosition.X +
                                                RetrieveTotalNumberOfSpaces(pCookedReadData->_OriginalCursorPosition.X,
                                                                            pCookedReadData->_BackupLimit, pCookedReadData->_CurrentPosition));
                    if (CheckBisectStringW(pCookedReadData->_BackupLimit,
                                           pCookedReadData->_CurrentPosition + 1,
                                           sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X))
                    {
                        CurrentPosition.X++;
                    }

                    UpdateCursorPosition = TRUE;
                }
            }
            else
            {
                if (pCookedReadData->_BufPtr != pCookedReadData->_BackupLimit)
                {
                    pCookedReadData->_BufPtr--;
                    pCookedReadData->_CurrentPosition--;
                    CurrentPosition.X = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition().X;
                    CurrentPosition.Y = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition().Y;
                    CurrentPosition.X = (SHORT)(CurrentPosition.X -
                                                RetrieveNumberOfSpaces(pCookedReadData->_OriginalCursorPosition.X,
                                                                       pCookedReadData->_BackupLimit,
                                                                       pCookedReadData->_CurrentPosition));
                    if (CheckBisectProcessW(pCookedReadData->_pScreenInfo,
                                            pCookedReadData->_BackupLimit,
                                            pCookedReadData->_CurrentPosition + 2,
                                            sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X,
                                            pCookedReadData->_OriginalCursorPosition.X,
                                            TRUE))
                    {
                        if ((CurrentPosition.X == -2) || (CurrentPosition.X == -1))
                        {
                            CurrentPosition.X--;
                        }
                    }

                    UpdateCursorPosition = TRUE;
                }
            }
            break;
        case VK_RIGHT:
        case VK_F1:
            // we don't need to check for end of buffer here because we've
            // already done it.
            if (dwKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
            {
                if (wch != VK_F1)
                {
                    if (pCookedReadData->_CurrentPosition < (pCookedReadData->_BytesRead / sizeof(WCHAR)))
                    {
                        PWCHAR NextWord = pCookedReadData->_BufPtr;

                        if (!gci->GetExtendedEditKey())
                        {
                            SHORT i;

                            for (i = (SHORT)(pCookedReadData->_CurrentPosition); i < (SHORT)((pCookedReadData->_BytesRead - 1) / sizeof(WCHAR)); i++)
                            {
                                if (IS_WORD_DELIM(*NextWord))
                                {
                                    i++;
                                    NextWord++;
                                    while ((i < (SHORT)((pCookedReadData->_BytesRead - 1) / sizeof(WCHAR))) && IS_WORD_DELIM(*NextWord))
                                    {
                                        i++;
                                        NextWord++;
                                    }

                                    break;
                                }

                                NextWord++;
                            }
                        }
                        else
                        {
                            // A bit better word skipping.
                            PWCHAR BufLast = pCookedReadData->_BackupLimit + pCookedReadData->_BytesRead / sizeof(WCHAR);

                            ASSERT(NextWord < BufLast);
                            if (*NextWord == L' ')
                            {
                                // If the current character is space, skip to the next non-space character.
                                while (NextWord < BufLast)
                                {
                                    if (*NextWord != L' ')
                                    {
                                        break;
                                    }
                                    ++NextWord;
                                }
                            }
                            else
                            {
                                // Skip the body part.
                                BOOL fStartFromDelim = IS_WORD_DELIM(*NextWord);

                                while (++NextWord < BufLast)
                                {
                                    if (fStartFromDelim != IS_WORD_DELIM(*NextWord))
                                    {
                                        break;
                                    }
                                }

                                // Skip the space block.
                                if (NextWord < BufLast && *NextWord == L' ')
                                {
                                    while (++NextWord < BufLast)
                                    {
                                        if (*NextWord != L' ')
                                        {
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        pCookedReadData->_BufPtr = NextWord;
                        pCookedReadData->_CurrentPosition = (ULONG)(pCookedReadData->_BufPtr - pCookedReadData->_BackupLimit);
                        CurrentPosition = pCookedReadData->_OriginalCursorPosition;
                        CurrentPosition.X = (SHORT)(CurrentPosition.X +
                                                    RetrieveTotalNumberOfSpaces(pCookedReadData->_OriginalCursorPosition.X,
                                                                                pCookedReadData->_BackupLimit,
                                                                                pCookedReadData->_CurrentPosition));
                        if (CheckBisectStringW(pCookedReadData->_BackupLimit,
                                               pCookedReadData->_CurrentPosition + 1,
                                               sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X))
                        {
                            CurrentPosition.X++;
                        }
                        UpdateCursorPosition = TRUE;
                    }
                }
            }
            else
            {
                // If not at the end of the line, move cursor position right.
                if (pCookedReadData->_CurrentPosition < (pCookedReadData->_BytesRead / sizeof(WCHAR)))
                {
                    CurrentPosition = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();
                    CurrentPosition.X = (SHORT)(CurrentPosition.X +
                                                RetrieveNumberOfSpaces(pCookedReadData->_OriginalCursorPosition.X,
                                                                       pCookedReadData->_BackupLimit,
                                                                       pCookedReadData->_CurrentPosition));
                    if (CheckBisectProcessW(pCookedReadData->_pScreenInfo,
                                            pCookedReadData->_BackupLimit,
                                            pCookedReadData->_CurrentPosition + 2,
                                            sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X,
                                            pCookedReadData->_OriginalCursorPosition.X,
                                            TRUE))
                    {
                        if (CurrentPosition.X == (sScreenBufferSizeX - 1))
                            CurrentPosition.X++;
                    }

                    pCookedReadData->_BufPtr++;
                    pCookedReadData->_CurrentPosition++;
                    UpdateCursorPosition = TRUE;

                    // if at the end of the line, copy a character from the same position in the last command
                }
                else if (pCookedReadData->_CommandHistory)
                {
                    PCOMMAND LastCommand;
                    DWORD NumSpaces;
                    LastCommand = GetLastCommand(pCookedReadData->_CommandHistory);
                    if (LastCommand && (USHORT)(LastCommand->CommandLength / sizeof(WCHAR)) > (USHORT)pCookedReadData->_CurrentPosition)
                    {
                        *pCookedReadData->_BufPtr = LastCommand->Command[pCookedReadData->_CurrentPosition];
                        pCookedReadData->_BytesRead += sizeof(WCHAR);
                        pCookedReadData->_CurrentPosition++;
                        if (pCookedReadData->_Echo)
                        {
                            CharsToWrite = sizeof(WCHAR);
                            Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                                      pCookedReadData->_BackupLimit,
                                                      pCookedReadData->_BufPtr,
                                                      pCookedReadData->_BufPtr,
                                                      &CharsToWrite,
                                                      &NumSpaces,
                                                      pCookedReadData->_OriginalCursorPosition.X,
                                                      WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                                      &ScrollY);
                            ASSERT(NT_SUCCESS(Status));
                            pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                            pCookedReadData->_NumberOfVisibleChars += NumSpaces;
                        }
                        pCookedReadData->_BufPtr += 1;
                    }
                }
            }
            break;

        case VK_F2:
            // copy the previous command to the current command, up to but
            // not including the character specified by the user.  the user
            // is prompted via popup to enter a character.
            if (pCookedReadData->_CommandHistory)
            {
                COORD PopupSize;

                PopupSize.X = COPY_TO_CHAR_PROMPT_LENGTH + 2;
                PopupSize.Y = 1;
                Status = BeginPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory, PopupSize);
                if (NT_SUCCESS(Status))
                {
                    // CopyToCharPopup does EndPopup call
                    return CopyToCharPopup(pCookedReadData);
                }
            }
            break;

        case VK_F3:
            // Copy the remainder of the previous command to the current command.
            if (pCookedReadData->_CommandHistory)
            {
                PCOMMAND LastCommand;
                DWORD NumSpaces, cchCount;

                LastCommand = GetLastCommand(pCookedReadData->_CommandHistory);
                if (LastCommand && (USHORT)(LastCommand->CommandLength / sizeof(WCHAR)) > (USHORT)pCookedReadData->_CurrentPosition)
                {
                    cchCount = (LastCommand->CommandLength / sizeof(WCHAR)) - pCookedReadData->_CurrentPosition;

#pragma prefast(suppress:__WARNING_POTENTIAL_BUFFER_OVERFLOW_HIGH_PRIORITY, "This is fine")
                    memmove(pCookedReadData->_BufPtr, &LastCommand->Command[pCookedReadData->_CurrentPosition], cchCount * sizeof(WCHAR));
                    pCookedReadData->_CurrentPosition += cchCount;
                    cchCount *= sizeof(WCHAR);
                    pCookedReadData->_BytesRead = max(LastCommand->CommandLength, pCookedReadData->_BytesRead);
                    if (pCookedReadData->_Echo)
                    {
                        Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                                  pCookedReadData->_BackupLimit,
                                                  pCookedReadData->_BufPtr,
                                                  pCookedReadData->_BufPtr,
                                                  &cchCount,
                                                  (PULONG)&NumSpaces,
                                                  pCookedReadData->_OriginalCursorPosition.X,
                                                  WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                                  &ScrollY);
                        ASSERT(NT_SUCCESS(Status));
                        pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                        pCookedReadData->_NumberOfVisibleChars += NumSpaces;
                    }
                    pCookedReadData->_BufPtr += cchCount / sizeof(WCHAR);
                }

            }
            break;

        case VK_F4:
            // Delete the current command from cursor position to the
            // letter specified by the user. The user is prompted via
            // popup to enter a character.
            if (pCookedReadData->_CommandHistory)
            {
                COORD PopupSize;

                PopupSize.X = COPY_FROM_CHAR_PROMPT_LENGTH + 2;
                PopupSize.Y = 1;
                Status = BeginPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory, PopupSize);
                if (NT_SUCCESS(Status))
                {
                    // CopyFromCharPopup does EndPopup call
                    return CopyFromCharPopup(pCookedReadData);
                }
            }
            break;
        case VK_F6:
        {
            // place a ctrl-z in the current command line
            DWORD NumSpaces = 0;

            *pCookedReadData->_BufPtr = (WCHAR)0x1a;  // ctrl-z
            pCookedReadData->_BytesRead += sizeof(WCHAR);
            pCookedReadData->_CurrentPosition++;
            if (pCookedReadData->_Echo)
            {
                CharsToWrite = sizeof(WCHAR);
                Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                          pCookedReadData->_BackupLimit,
                                          pCookedReadData->_BufPtr,
                                          pCookedReadData->_BufPtr,
                                          &CharsToWrite,
                                          &NumSpaces,
                                          pCookedReadData->_OriginalCursorPosition.X,
                                          WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                          &ScrollY);
                ASSERT(NT_SUCCESS(Status));
                pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                pCookedReadData->_NumberOfVisibleChars += NumSpaces;
            }
            pCookedReadData->_BufPtr += 1;
            break;
        }
        case VK_F7:
            if (dwKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
            {
                if (pCookedReadData->_CommandHistory)
                {
                    EmptyCommandHistory(pCookedReadData->_CommandHistory);
                    pCookedReadData->_CommandHistory->Flags |= CLE_ALLOCATED;
                }
            }
            break;

        case VK_F8:
            if (pCookedReadData->_CommandHistory)
            {
                SHORT i;

                // Cycles through the stored commands that start with the characters in the current command.
                i = FindMatchingCommand(pCookedReadData->_CommandHistory,
                                        pCookedReadData->_BackupLimit,
                                        pCookedReadData->_CurrentPosition * sizeof(WCHAR),
                                        pCookedReadData->_CommandHistory->LastDisplayed,
                                        0);
                if (i != -1)
                {
                    SHORT CurrentPos;
                    COORD CursorPosition;

                    // save cursor position
                    CurrentPos = (SHORT)pCookedReadData->_CurrentPosition;
                    CursorPosition = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();

                    DeleteCommandLine(pCookedReadData, TRUE);
                    Status = RetrieveNthCommand(pCookedReadData->_CommandHistory,
                                                i,
                                                pCookedReadData->_BackupLimit,
                                                pCookedReadData->_BufferSize,
                                                &pCookedReadData->_BytesRead);
                    ASSERT(pCookedReadData->_BackupLimit == pCookedReadData->_BufPtr);
                    if (pCookedReadData->_Echo)
                    {
                        Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                                  pCookedReadData->_BackupLimit,
                                                  pCookedReadData->_BufPtr,
                                                  pCookedReadData->_BufPtr,
                                                  &pCookedReadData->_BytesRead,
                                                  &pCookedReadData->_NumberOfVisibleChars,
                                                  pCookedReadData->_OriginalCursorPosition.X,
                                                  WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                                  &ScrollY);
                        ASSERT(NT_SUCCESS(Status));
                        pCookedReadData->_OriginalCursorPosition.Y += ScrollY;
                    }
                    CursorPosition.Y += ScrollY;

                    // restore cursor position
                    pCookedReadData->_BufPtr = pCookedReadData->_BackupLimit + CurrentPos;
                    pCookedReadData->_CurrentPosition = CurrentPos;
                    Status = pCookedReadData->_pScreenInfo->SetCursorPosition(CursorPosition, TRUE);
                    ASSERT(NT_SUCCESS(Status));
                }
            }
            break;
        case VK_F9:
        {
            // prompt the user to enter the desired command number. copy that command to the command line.
            COORD PopupSize;

            if (pCookedReadData->_CommandHistory &&
                pCookedReadData->_CommandHistory->NumberOfCommands &&
                sScreenBufferSizeX >= MINIMUM_COMMAND_PROMPT_SIZE + 2)
            {   // 2 is for border
                PopupSize.X = COMMAND_NUMBER_PROMPT_LENGTH + COMMAND_NUMBER_LENGTH;
                PopupSize.Y = 1;
                Status = BeginPopup(pCookedReadData->_pScreenInfo, pCookedReadData->_CommandHistory, PopupSize);
                if (NT_SUCCESS(Status))
                {
                    // CommandNumberPopup does EndPopup call
                    return CommandNumberPopup(pCookedReadData);
                }
            }
            break;
        }
        case VK_F10:
            if (dwKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
            {
                ClearAliases();
            }
            break;
        case VK_INSERT:
            pCookedReadData->_InsertMode = !pCookedReadData->_InsertMode;
            pCookedReadData->_pScreenInfo->SetCursorDBMode((BOOLEAN)(pCookedReadData->_InsertMode != gci->GetInsertMode()));
            break;
        case VK_DELETE:
            if (!AT_EOL(pCookedReadData))
            {
                COORD CursorPosition;

                BOOL fStartFromDelim = IS_WORD_DELIM(*pCookedReadData->_BufPtr);

            del_repeat:
                // save cursor position
                CursorPosition = pCookedReadData->_pScreenInfo->TextInfo->GetCursor()->GetPosition();

                // Delete commandline.
#pragma prefast(suppress:__WARNING_BUFFER_OVERFLOW, "Not sure why prefast is getting confused here")
                DeleteCommandLine(pCookedReadData, FALSE);

                // Delete char.
                pCookedReadData->_BytesRead -= sizeof(WCHAR);
                memmove(pCookedReadData->_BufPtr,
                        pCookedReadData->_BufPtr + 1,
                        pCookedReadData->_BytesRead - (pCookedReadData->_CurrentPosition * sizeof(WCHAR)));

                {
                    PWCHAR buf = (PWCHAR)((PBYTE)pCookedReadData->_BackupLimit + pCookedReadData->_BytesRead);
                    *buf = (WCHAR)' ';
                }

                // Write commandline.
                if (pCookedReadData->_Echo)
                {
                    Status = WriteCharsLegacy(pCookedReadData->_pScreenInfo,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              pCookedReadData->_BackupLimit,
                                              &pCookedReadData->_BytesRead,
                                              &pCookedReadData->_NumberOfVisibleChars,
                                              pCookedReadData->_OriginalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              nullptr);
                    ASSERT(NT_SUCCESS(Status));
                }

                // restore cursor position
                if (CheckBisectProcessW(pCookedReadData->_pScreenInfo,
                                        pCookedReadData->_BackupLimit,
                                        pCookedReadData->_CurrentPosition + 1,
                                        sScreenBufferSizeX - pCookedReadData->_OriginalCursorPosition.X,
                                        pCookedReadData->_OriginalCursorPosition.X,
                                        TRUE))
                {
                    CursorPosition.X++;
                }
                CurrentPosition = CursorPosition;
                if (pCookedReadData->_Echo)
                {
                    Status = AdjustCursorPosition(pCookedReadData->_pScreenInfo, CurrentPosition, TRUE, nullptr);
                    ASSERT(NT_SUCCESS(Status));
                }

                // If Ctrl key is pressed, delete a word.
                // If the start point was word delimiter, just remove delimiters portion only.
                if ((dwKeyState & CTRL_PRESSED) && !AT_EOL(pCookedReadData) && fStartFromDelim ^ !IS_WORD_DELIM(*pCookedReadData->_BufPtr))
                {
                    goto del_repeat;
                }
            }
            break;
        default:
            ASSERT(FALSE);
            break;
        }
    }

    if (UpdateCursorPosition && pCookedReadData->_Echo)
    {
        Status = AdjustCursorPosition(pCookedReadData->_pScreenInfo, CurrentPosition, TRUE, nullptr);
        ASSERT(NT_SUCCESS(Status));
    }

    return STATUS_SUCCESS;
}

PCOMMAND RemoveCommand(_In_ PCOMMAND_HISTORY CommandHistory, _In_ SHORT iDel)
{
    SHORT iFirst = CommandHistory->FirstCommand;
    SHORT iLast = CommandHistory->LastAdded;
    SHORT iDisp = CommandHistory->LastDisplayed;

    if (CommandHistory->NumberOfCommands == 0)
    {
        return nullptr;
    }

    SHORT const nDel = COMMAND_INDEX_TO_NUM(iDel, CommandHistory);
    if ((nDel < COMMAND_INDEX_TO_NUM(iFirst, CommandHistory)) || (nDel > COMMAND_INDEX_TO_NUM(iLast, CommandHistory)))
    {
        return nullptr;
    }

    if (iDisp == iDel)
    {
        CommandHistory->LastDisplayed = -1;
    }

    PCOMMAND* const ppcFirst = &(CommandHistory->Commands[iFirst]);
    PCOMMAND* const ppcDel = &(CommandHistory->Commands[iDel]);
    PCOMMAND const pcmdDel = *ppcDel;

    if (iDel < iLast)
    {
        memmove(ppcDel, ppcDel + 1, (iLast - iDel) * sizeof(PCOMMAND));
        if ((iDisp > iDel) && (iDisp <= iLast))
        {
            COMMAND_IND_DEC(iDisp, CommandHistory);
        }
        COMMAND_IND_DEC(iLast, CommandHistory);
    }
    else if (iFirst <= iDel)
    {
        memmove(ppcFirst + 1, ppcFirst, (iDel - iFirst) * sizeof(PCOMMAND));
        if ((iDisp >= iFirst) && (iDisp < iDel))
        {
            COMMAND_IND_INC(iDisp, CommandHistory);
        }
        COMMAND_IND_INC(iFirst, CommandHistory);
    }

    CommandHistory->FirstCommand = iFirst;
    CommandHistory->LastAdded = iLast;
    CommandHistory->LastDisplayed = iDisp;
    CommandHistory->NumberOfCommands--;
    return pcmdDel;
}


// Routine Description:
// - this routine finds the most recent command that starts with the letters already in the current command.  it returns the array index (no mod needed).
SHORT FindMatchingCommand(_In_ PCOMMAND_HISTORY CommandHistory,
                          _In_reads_bytes_(cbIn) PCWCHAR pwchIn,
                          _In_ ULONG cbIn,
                          _In_ SHORT CommandIndex,  // where to start from
                          _In_ DWORD Flags)
{
    if (CommandHistory->NumberOfCommands == 0)
    {
        return -1;
    }

    if (!(Flags & FMCFL_JUST_LOOKING) && (CommandHistory->Flags & CLE_RESET))
    {
        CommandHistory->Flags &= ~CLE_RESET;
    }
    else
    {
        COMMAND_IND_PREV(CommandIndex, CommandHistory);
    }

    if (cbIn == 0)
    {
        return CommandIndex;
    }

    for (SHORT i = 0; i < CommandHistory->NumberOfCommands; i++)
    {
        PCOMMAND pcmdT = CommandHistory->Commands[CommandIndex];

        if ((IsFlagClear(Flags, FMCFL_EXACT_MATCH) && (cbIn <= pcmdT->CommandLength)) || ((USHORT)cbIn == pcmdT->CommandLength))
        {
            if (!wcsncmp(pcmdT->Command, pwchIn, (USHORT)cbIn / sizeof(WCHAR)))
            {
                return CommandIndex;
            }
        }

        COMMAND_IND_PREV(CommandIndex, CommandHistory);
    }

    return -1;
}

void DrawCommandListBorder(_In_ PCLE_POPUP const Popup, _In_ PSCREEN_INFORMATION const ScreenInfo)
{
    // fill attributes of top line
    COORD WriteCoord;
    WriteCoord.X = Popup->Region.Left;
    WriteCoord.Y = Popup->Region.Top;
    ULONG Length = POPUP_SIZE_X(Popup) + 2;
    FillOutput(ScreenInfo, Popup->Attributes.GetLegacyAttributes(), WriteCoord, CONSOLE_ATTRIBUTE, &Length);

    // draw upper left corner
    Length = 1;
    FillOutput(ScreenInfo, ScreenInfo->LineChar[UPPER_LEFT_CORNER], WriteCoord, CONSOLE_REAL_UNICODE, &Length);

    // draw upper bar
    WriteCoord.X += 1;
    Length = POPUP_SIZE_X(Popup);
    FillOutput(ScreenInfo, ScreenInfo->LineChar[HORIZONTAL_LINE], WriteCoord, CONSOLE_REAL_UNICODE, &Length);

    // draw upper right corner
    WriteCoord.X = Popup->Region.Right;
    Length = 1;
    FillOutput(ScreenInfo, ScreenInfo->LineChar[UPPER_RIGHT_CORNER], WriteCoord, CONSOLE_REAL_UNICODE, &Length);

    for (SHORT i = 0; i < POPUP_SIZE_Y(Popup); i++)
    {
        WriteCoord.Y += 1;
        WriteCoord.X = Popup->Region.Left;

        // fill attributes
        Length = POPUP_SIZE_X(Popup) + 2;
        FillOutput(ScreenInfo, Popup->Attributes.GetLegacyAttributes(), WriteCoord, CONSOLE_ATTRIBUTE, &Length);
        Length = 1;
        FillOutput(ScreenInfo, ScreenInfo->LineChar[VERTICAL_LINE], WriteCoord, CONSOLE_REAL_UNICODE, &Length);
        WriteCoord.X = Popup->Region.Right;
        Length = 1;
        FillOutput(ScreenInfo, ScreenInfo->LineChar[VERTICAL_LINE], WriteCoord, CONSOLE_REAL_UNICODE, &Length);
    }

    // Draw bottom line.
    // Fill attributes of top line.
    WriteCoord.X = Popup->Region.Left;
    WriteCoord.Y = Popup->Region.Bottom;
    Length = POPUP_SIZE_X(Popup) + 2;
    FillOutput(ScreenInfo, Popup->Attributes.GetLegacyAttributes(), WriteCoord, CONSOLE_ATTRIBUTE, &Length);

    // Draw bottom left corner.
    Length = 1;
    WriteCoord.X = Popup->Region.Left;
    FillOutput(ScreenInfo, ScreenInfo->LineChar[BOTTOM_LEFT_CORNER], WriteCoord, CONSOLE_REAL_UNICODE, &Length);

    // Draw lower bar.
    WriteCoord.X += 1;
    Length = POPUP_SIZE_X(Popup);
    FillOutput(ScreenInfo, ScreenInfo->LineChar[HORIZONTAL_LINE], WriteCoord, CONSOLE_REAL_UNICODE, &Length);

    // draw lower right corner
    WriteCoord.X = Popup->Region.Right;
    Length = 1;
    FillOutput(ScreenInfo, ScreenInfo->LineChar[BOTTOM_RIGHT_CORNER], WriteCoord, CONSOLE_REAL_UNICODE, &Length);
}

void UpdateHighlight(_In_ PCLE_POPUP Popup,
                     _In_ SHORT OldCurrentCommand, // command number, not index
                     _In_ SHORT NewCurrentCommand,
                     _In_ PSCREEN_INFORMATION ScreenInfo)
{
    SHORT TopIndex;
    if (Popup->BottomIndex < POPUP_SIZE_Y(Popup))
    {
        TopIndex = 0;
    }
    else
    {
        TopIndex = (SHORT)(Popup->BottomIndex - POPUP_SIZE_Y(Popup) + 1);
    }
    const WORD PopupLegacyAttributes = Popup->Attributes.GetLegacyAttributes();
    COORD WriteCoord;
    WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
    ULONG lStringLength = POPUP_SIZE_X(Popup);

    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1 + OldCurrentCommand - TopIndex);
    FillOutput(ScreenInfo, PopupLegacyAttributes, WriteCoord, CONSOLE_ATTRIBUTE, &lStringLength);

    // highlight new command
    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1 + NewCurrentCommand - TopIndex);

    // inverted attributes
    WORD const Attributes = (WORD)(((PopupLegacyAttributes << 4) & 0xf0) | ((PopupLegacyAttributes >> 4) & 0x0f));
    FillOutput(ScreenInfo, Attributes, WriteCoord, CONSOLE_ATTRIBUTE, &lStringLength);
}

void DrawCommandListPopup(_In_ PCLE_POPUP const Popup,
                          _In_ SHORT const CurrentCommand,
                          _In_ PCOMMAND_HISTORY const CommandHistory,
                          _In_ PSCREEN_INFORMATION const ScreenInfo)
{
    // draw empty popup
    COORD WriteCoord;
    WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1);
    ULONG lStringLength = POPUP_SIZE_X(Popup);
    for (SHORT i = 0; i < POPUP_SIZE_Y(Popup); ++i)
    {
        FillOutput(ScreenInfo, Popup->Attributes.GetLegacyAttributes(), WriteCoord, CONSOLE_ATTRIBUTE, &lStringLength);
        FillOutput(ScreenInfo, (WCHAR)' ', WriteCoord, CONSOLE_FALSE_UNICODE,   // faster than real unicode
                   &lStringLength);
        WriteCoord.Y += 1;
    }

    WriteCoord.Y = (SHORT)(Popup->Region.Top + 1);
    SHORT i = max((SHORT)(Popup->BottomIndex - POPUP_SIZE_Y(Popup) + 1), 0);
    for (; i <= Popup->BottomIndex; i++)
    {
        CHAR CommandNumber[COMMAND_NUMBER_SIZE];
        // Write command number to screen.
        if (0 != _itoa_s(i, CommandNumber, ARRAYSIZE(CommandNumber), 10))
        {
            return;
        }

        PCHAR CommandNumberPtr = CommandNumber;

        size_t CommandNumberLength;
        if (FAILED(StringCchLengthA(CommandNumberPtr, ARRAYSIZE(CommandNumber), &CommandNumberLength)))
        {
            return;
        }
        __assume_bound(CommandNumberLength);

        if (CommandNumberLength + 1 >= ARRAYSIZE(CommandNumber))
        {
            return;
        }

        CommandNumber[CommandNumberLength] = ':';
        CommandNumber[CommandNumberLength + 1] = ' ';
        CommandNumberLength += 2;
        if (CommandNumberLength > (ULONG)POPUP_SIZE_X(Popup))
        {
            CommandNumberLength = (ULONG)POPUP_SIZE_X(Popup);
        }

        WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
        WriteOutputString(ScreenInfo, CommandNumberPtr, WriteCoord, CONSOLE_ASCII, (PULONG)& CommandNumberLength, nullptr);

        // write command to screen
        lStringLength = CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i, CommandHistory)]->CommandLength / sizeof(WCHAR);
        {
            DWORD lTmpStringLength = lStringLength;
            LONG lPopupLength = (LONG)(POPUP_SIZE_X(Popup) - CommandNumberLength);
            LPWSTR lpStr = CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i, CommandHistory)]->Command;
            while (lTmpStringLength--)
            {
                if (IsCharFullWidth(*lpStr++))
                {
                    lPopupLength -= 2;
                }
                else
                {
                    lPopupLength--;
                }

                if (lPopupLength <= 0)
                {
                    lStringLength -= lTmpStringLength;
                    if (lPopupLength < 0)
                    {
                        lStringLength--;
                    }

                    break;
                }
            }
        }

        WriteCoord.X = (SHORT)(WriteCoord.X + CommandNumberLength);
        {
            PWCHAR TransBuffer;

            TransBuffer = new WCHAR[lStringLength];
            if (TransBuffer == nullptr)
            {
                return;
            }

            memmove(TransBuffer, CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i, CommandHistory)]->Command, lStringLength * sizeof(WCHAR));
            WriteOutputString(ScreenInfo, TransBuffer, WriteCoord, CONSOLE_REAL_UNICODE, &lStringLength, nullptr);
            delete[] TransBuffer;
        }

        // write attributes to screen
        if (COMMAND_NUM_TO_INDEX(i, CommandHistory) == CurrentCommand)
        {
            WriteCoord.X = (SHORT)(Popup->Region.Left + 1);
            WORD PopupLegacyAttributes = Popup->Attributes.GetLegacyAttributes();
            // inverted attributes
            WORD const Attributes = (WORD)(((PopupLegacyAttributes << 4) & 0xf0) | ((PopupLegacyAttributes >> 4) & 0x0f));
            lStringLength = POPUP_SIZE_X(Popup);
            FillOutput(ScreenInfo, Attributes, WriteCoord, CONSOLE_ATTRIBUTE, &lStringLength);
        }

        WriteCoord.Y += 1;
    }
}

void UpdateCommandListPopup(_In_ SHORT Delta,
                            _Inout_ PSHORT CurrentCommand,   // real index, not command #
                            _In_ PCOMMAND_HISTORY const CommandHistory,
                            _In_ PCLE_POPUP Popup,
                            _In_ PSCREEN_INFORMATION const ScreenInfo,
                            _In_ DWORD const Flags)
{
    if (Delta == 0)
    {
        return;
    }
    SHORT const Size = POPUP_SIZE_Y(Popup);

    SHORT CurCmdNum;
    SHORT NewCmdNum;

    if (Flags & UCLP_WRAP)
    {
        CurCmdNum = *CurrentCommand;
        NewCmdNum = CurCmdNum + Delta;
        NewCmdNum = COMMAND_INDEX_TO_NUM(NewCmdNum, CommandHistory);
        CurCmdNum = COMMAND_INDEX_TO_NUM(CurCmdNum, CommandHistory);
    }
    else
    {
        CurCmdNum = COMMAND_INDEX_TO_NUM(*CurrentCommand, CommandHistory);
        NewCmdNum = CurCmdNum + Delta;
        if (NewCmdNum >= CommandHistory->NumberOfCommands)
        {
            NewCmdNum = (SHORT)(CommandHistory->NumberOfCommands - 1);
        }
        else if (NewCmdNum < 0)
        {
            NewCmdNum = 0;
        }
    }
    Delta = NewCmdNum - CurCmdNum;

    BOOL Scroll = FALSE;
    // determine amount to scroll, if any
    if (NewCmdNum <= Popup->BottomIndex - Size)
    {
        Popup->BottomIndex += Delta;
        if (Popup->BottomIndex < (SHORT)(Size - 1))
        {
            Popup->BottomIndex = (SHORT)(Size - 1);
        }
        Scroll = TRUE;
    }
    else if (NewCmdNum > Popup->BottomIndex)
    {
        Popup->BottomIndex += Delta;
        if (Popup->BottomIndex >= CommandHistory->NumberOfCommands)
        {
            Popup->BottomIndex = (SHORT)(CommandHistory->NumberOfCommands - 1);
        }
        Scroll = TRUE;
    }

    // write commands to popup
    if (Scroll)
    {
        DrawCommandListPopup(Popup, COMMAND_NUM_TO_INDEX(NewCmdNum, CommandHistory), CommandHistory, ScreenInfo);
    }
    else
    {
        UpdateHighlight(Popup, COMMAND_INDEX_TO_NUM((*CurrentCommand), CommandHistory), NewCmdNum, ScreenInfo);
    }

    *CurrentCommand = COMMAND_NUM_TO_INDEX(NewCmdNum, CommandHistory);
}

// Routine Description:
// - This routine marks the command history buffer freed.
// Arguments:
// - Console - pointer to console.
// - ProcessHandle - handle to client process.
// Return Value:
// - <none>
PCOMMAND_HISTORY FindCommandHistory(_In_ const HANDLE hProcess)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PLIST_ENTRY const ListHead = &gci->CommandHistoryList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PCOMMAND_HISTORY const History = CONTAINING_RECORD(ListNext, COMMAND_HISTORY, ListLink);
        ListNext = ListNext->Flink;
        if (History->ProcessHandle == hProcess)
        {
            ASSERT(History->Flags & CLE_ALLOCATED);
            return History;
        }
    }

    return nullptr;
}

// Routine Description:
// - This routine marks the command history buffer freed.
// Arguments:
// - hProcess - handle to client process.
// Return Value:
// - <none>
void FreeCommandHistory(_In_ HANDLE const hProcess)
{
    PCOMMAND_HISTORY const History = FindCommandHistory(hProcess);
    if (History)
    {
        History->Flags &= ~CLE_ALLOCATED;
        History->ProcessHandle = nullptr;
    }
}

void FreeCommandHistoryBuffers()
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    PLIST_ENTRY const ListHead = &gci->CommandHistoryList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PCOMMAND_HISTORY History = CONTAINING_RECORD(ListNext, COMMAND_HISTORY, ListLink);
        ListNext = ListNext->Flink;

        if (History)
        {

            RemoveEntryList(&History->ListLink);
            if (History->AppName)
            {
                delete[] History->AppName;
                History->AppName = nullptr;
            }

            for (SHORT i = 0; i < History->NumberOfCommands; i++)
            {
                delete[] History->Commands[i];
                History->Commands[i] = nullptr;
            }

            delete[] History;
            History = nullptr;
        }
    }
}

void ResizeCommandHistoryBuffers(_In_ UINT const cCommands)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    ASSERT(cCommands <= SHORT_MAX);
    gci->SetHistoryBufferSize(cCommands);

    PLIST_ENTRY const ListHead = &gci->CommandHistoryList;
    PLIST_ENTRY ListNext = ListHead->Flink;
    while (ListNext != ListHead)
    {
        PCOMMAND_HISTORY const History = CONTAINING_RECORD(ListNext, COMMAND_HISTORY, ListLink);
        ListNext = ListNext->Flink;

        PCOMMAND_HISTORY const NewHistory = ReallocCommandHistory(History, cCommands);
        COOKED_READ_DATA* const CookedReadData = gci->lpCookedReadData;
        if (CookedReadData && CookedReadData->_CommandHistory == History)
        {
            CookedReadData->_CommandHistory = NewHistory;
        }
    }
}



// Routine Description:
// - This routine is called when escape is entered or a command is added.
void ResetCommandHistory(_In_opt_ PCOMMAND_HISTORY CommandHistory)
{
    if (CommandHistory == nullptr)
    {
        return;
    }
    CommandHistory->LastDisplayed = CommandHistory->LastAdded;
    CommandHistory->Flags |= CLE_RESET;
}

NTSTATUS AddCommand(_In_ PCOMMAND_HISTORY pCmdHistory,
                    _In_reads_bytes_(cbCommand) PCWCHAR pwchCommand,
                    _In_ const USHORT cbCommand,
                    _In_ const BOOL fHistoryNoDup)
{
    if (pCmdHistory == nullptr || pCmdHistory->MaximumNumberOfCommands == 0)
    {
        return STATUS_NO_MEMORY;
    }

    ASSERT(pCmdHistory->Flags & CLE_ALLOCATED);

    if (cbCommand == 0)
    {
        return STATUS_SUCCESS;
    }

    if (pCmdHistory->NumberOfCommands == 0 ||
        pCmdHistory->Commands[pCmdHistory->LastAdded]->CommandLength != cbCommand ||
        memcmp(pCmdHistory->Commands[pCmdHistory->LastAdded]->Command, pwchCommand, cbCommand))
    {

        PCOMMAND pCmdReuse = nullptr;

        if (fHistoryNoDup)
        {
            SHORT i;
            i = FindMatchingCommand(pCmdHistory, pwchCommand, cbCommand, pCmdHistory->LastDisplayed, FMCFL_EXACT_MATCH);
            if (i != -1)
            {
                pCmdReuse = RemoveCommand(pCmdHistory, i);
            }
        }

        // find free record.  if all records are used, free the lru one.
        if (pCmdHistory->NumberOfCommands < pCmdHistory->MaximumNumberOfCommands)
        {
            pCmdHistory->LastAdded += 1;
            pCmdHistory->NumberOfCommands++;
        }
        else
        {
            COMMAND_IND_INC(pCmdHistory->LastAdded, pCmdHistory);
            COMMAND_IND_INC(pCmdHistory->FirstCommand, pCmdHistory);
            delete[] pCmdHistory->Commands[pCmdHistory->LastAdded];
            if (pCmdHistory->LastDisplayed == pCmdHistory->LastAdded)
            {
                pCmdHistory->LastDisplayed = -1;
            }
        }

        // TODO: Fix Commands history accesses. See: http://osgvsowi/614402
        if (pCmdHistory->LastDisplayed == -1 ||
            pCmdHistory->Commands[pCmdHistory->LastDisplayed]->CommandLength != cbCommand ||
            memcmp(pCmdHistory->Commands[pCmdHistory->LastDisplayed]->Command, pwchCommand, cbCommand))
        {
            ResetCommandHistory(pCmdHistory);
        }

        // add command to array
        PCOMMAND* const ppCmd = &pCmdHistory->Commands[pCmdHistory->LastAdded];
        if (pCmdReuse)
        {
            *ppCmd = pCmdReuse;
        }
        else
        {
            *ppCmd = (PCOMMAND) new BYTE[cbCommand + sizeof(COMMAND)];
            if (*ppCmd == nullptr)
            {
                COMMAND_IND_PREV(pCmdHistory->LastAdded, pCmdHistory);
                pCmdHistory->NumberOfCommands -= 1;
                return STATUS_NO_MEMORY;
            }
            (*ppCmd)->CommandLength = cbCommand;
            memmove((*ppCmd)->Command, pwchCommand, cbCommand);
        }
    }
    pCmdHistory->Flags |= CLE_RESET; // remember that we've returned a cmd
    return STATUS_SUCCESS;
}

NTSTATUS RetrieveCommand(_In_ PCOMMAND_HISTORY CommandHistory,
                         _In_ WORD VirtualKeyCode,
                         _In_reads_bytes_(BufferSize) PWCHAR Buffer,
                         _In_ ULONG BufferSize,
                         _Out_ PULONG CommandSize)
{
    if (CommandHistory == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }

    ASSERT(CommandHistory->Flags & CLE_ALLOCATED);

    if (CommandHistory->NumberOfCommands == 0)
    {
        return STATUS_UNSUCCESSFUL;
    }

    if (CommandHistory->NumberOfCommands == 1)
    {
        CommandHistory->LastDisplayed = 0;
    }
    else if (VirtualKeyCode == VK_UP)
    {
        // if this is the first time for this read that a command has
        // been retrieved, return the current command.  otherwise, return
        // the previous command.
        if (CommandHistory->Flags & CLE_RESET)
        {
            CommandHistory->Flags &= ~CLE_RESET;
        }
        else
        {
            COMMAND_IND_PREV(CommandHistory->LastDisplayed, CommandHistory);
        }
    }
    else
    {
        COMMAND_IND_NEXT(CommandHistory->LastDisplayed, CommandHistory);
    }

    return RetrieveNthCommand(CommandHistory, CommandHistory->LastDisplayed, Buffer, BufferSize, CommandSize);
}

HRESULT GetConsoleTitleWImplHelper(_Out_writes_to_opt_(cchTitleBufferSize, *pcchTitleBufferWrittenOrNeeded) _Always_(_Post_z_) wchar_t* const pwsTitleBuffer,
                                   _In_ size_t const cchTitleBufferSize,
                                   _Out_ size_t* const pcchTitleBufferWritten,
                                   _Out_ size_t* const pcchTitleBufferNeeded,
                                   _In_ bool const fIsOriginal)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Ensure output variables are initialized.
    *pcchTitleBufferWritten = 0;
    *pcchTitleBufferNeeded = 0;

    if (nullptr != pwsTitleBuffer)
    {
        *pwsTitleBuffer = L'\0';
    }

    // Get the appropriate title and length depending on the mode.
    LPWSTR pwszTitle;
    size_t cchTitleLength;

    if (fIsOriginal)
    {
        pwszTitle = gci->OriginalTitle;
        cchTitleLength = wcslen(gci->OriginalTitle);
    }
    else
    {
        pwszTitle = gci->Title;
        cchTitleLength = wcslen(gci->Title);
    }

    // Always report how much space we would need.
    *pcchTitleBufferNeeded = cchTitleLength;

    // If we have a pointer to receive the data, then copy it out.
    if (nullptr != pwsTitleBuffer)
    {
        HRESULT const hr = StringCchCopyNW(pwsTitleBuffer, cchTitleBufferSize, pwszTitle, cchTitleLength);

        // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
        // Just say how much we managed to return.
        if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
        {
            *pcchTitleBufferWritten = min(cchTitleBufferSize, cchTitleLength);
        }
    }

    return S_OK;
}

HRESULT GetConsoleTitleAImplHelper(_Out_writes_to_(cchTitleBufferSize, *pcchTitleBufferWritten) _Always_(_Post_z_) char* const psTitleBuffer,
                                   _In_ size_t const cchTitleBufferSize,
                                   _Out_ size_t* const pcchTitleBufferWritten,
                                   _Out_ size_t* const pcchTitleBufferNeeded,
                                   _In_ bool const fIsOriginal)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Ensure output variables are initialized.
    *pcchTitleBufferWritten = 0;
    *pcchTitleBufferNeeded = 0;

    if (nullptr != psTitleBuffer)
    {
        *psTitleBuffer = '\0';
    }

    // Figure out how big our temporary Unicode buffer must be to get the title.
    size_t cchUnicodeTitleBufferNeeded;
    size_t cchUnicodeTitleBufferWritten;
    RETURN_IF_FAILED(GetConsoleTitleWImplHelper(nullptr, 0, &cchUnicodeTitleBufferWritten, &cchUnicodeTitleBufferNeeded, fIsOriginal));

    // If there's nothing to get, then simply return.
    RETURN_HR_IF(S_OK, 0 == cchUnicodeTitleBufferNeeded);

    // Allocate a unicode buffer of the right size.
    size_t const cchUnicodeTitleBufferSize = cchUnicodeTitleBufferNeeded + 1; // add one for null terminator space
    wistd::unique_ptr<wchar_t[]> pwsUnicodeTitleBuffer = wil::make_unique_nothrow<wchar_t[]>(cchUnicodeTitleBufferSize);
    RETURN_IF_NULL_ALLOC(pwsUnicodeTitleBuffer);

    // Retrieve the title in Unicode.
    RETURN_IF_FAILED(GetConsoleTitleWImplHelper(pwsUnicodeTitleBuffer.get(), cchUnicodeTitleBufferSize, &cchUnicodeTitleBufferWritten, &cchUnicodeTitleBufferNeeded, fIsOriginal));

    // Convert result to A
    wistd::unique_ptr<char[]> psConverted;
    size_t cchConverted;
    RETURN_IF_FAILED(ConvertToA(gci->CP,
                                pwsUnicodeTitleBuffer.get(),
                                cchUnicodeTitleBufferWritten,
                                psConverted,
                                cchConverted));

    // The legacy A behavior is a bit strange. If the buffer given doesn't have enough space to hold
    // the string without null termination (e.g. the title is 9 long, 10 with null. The buffer given isn't >= 9).
    // then do not copy anything back and do not report how much space we need.
    if (cchTitleBufferSize >= cchConverted)
    {
        // Say how many characters of buffer we would need to hold the entire result.
        *pcchTitleBufferNeeded = cchConverted;

        // Copy safely to output buffer
        HRESULT const hr = StringCchCopyNA(psTitleBuffer,
                                           cchTitleBufferSize,
                                           psConverted.get(),
                                           cchConverted);


        // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
        // Just say how much we managed to return.
        if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
        {
            // And return the size copied (either the size of the buffer or the null terminated length of the string we filled it with.)
            *pcchTitleBufferWritten = min(cchTitleBufferSize, cchConverted + 1);

            // Another compatibility fix... If we had exactly the number of bytes needed for an unterminated string,
            // then replace the terminator left behind by StringCchCopyNA with the final character of the title string.
            if (cchTitleBufferSize == cchConverted)
            {
                psTitleBuffer[cchTitleBufferSize - 1] = psConverted.get()[cchConverted - 1];
            }
        }
    }
    else
    {
        // If we didn't copy anything back and there is space, null terminate the given buffer and return.
        if (cchTitleBufferSize > 0)
        {
            psTitleBuffer[0] = '\0';
            *pcchTitleBufferWritten = 1;
        }
    }

    return S_OK;
}

HRESULT ApiRoutines::GetConsoleTitleAImpl(_Out_writes_to_(cchTitleBufferSize, *pcchTitleBufferWritten) _Always_(_Post_z_) char* const psTitleBuffer,
                                          _In_ size_t const cchTitleBufferSize,
                                          _Out_ size_t* const pcchTitleBufferWritten,
                                          _Out_ size_t* const pcchTitleBufferNeeded)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleTitleAImplHelper(psTitleBuffer,
                                      cchTitleBufferSize,
                                      pcchTitleBufferWritten,
                                      pcchTitleBufferNeeded,
                                      false);
}

HRESULT ApiRoutines::GetConsoleTitleWImpl(_Out_writes_to_(cchTitleBufferSize, *pcchTitleBufferWritten) _Always_(_Post_z_) wchar_t* const pwsTitleBuffer,
                                          _In_ size_t const cchTitleBufferSize,
                                          _Out_ size_t* const pcchTitleBufferWritten,
                                          _Out_ size_t* const pcchTitleBufferNeeded)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleTitleWImplHelper(pwsTitleBuffer,
                                      cchTitleBufferSize,
                                      pcchTitleBufferWritten,
                                      pcchTitleBufferNeeded,
                                      false);
}

HRESULT ApiRoutines::GetConsoleOriginalTitleAImpl(_Out_writes_to_(cchTitleBufferSize, *pcchTitleBufferWritten) _Always_(_Post_z_) char* const psTitleBuffer,
                                                  _In_ size_t const cchTitleBufferSize,
                                                  _Out_ size_t* const pcchTitleBufferWritten,
                                                  _Out_ size_t* const pcchTitleBufferNeeded)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleTitleAImplHelper(psTitleBuffer,
                                      cchTitleBufferSize,
                                      pcchTitleBufferWritten,
                                      pcchTitleBufferNeeded,
                                      true);
}

HRESULT ApiRoutines::GetConsoleOriginalTitleWImpl(_Out_writes_to_(cchTitleBufferSize, *pcchTitleBufferWritten) _Always_(_Post_z_) wchar_t* const pwsTitleBuffer,
                                                  _In_ size_t const cchTitleBufferSize,
                                                  _Out_ size_t* const pcchTitleBufferWritten,
                                                  _Out_ size_t* const pcchTitleBufferNeeded)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return GetConsoleTitleWImplHelper(pwsTitleBuffer,
                                      cchTitleBufferSize,
                                      pcchTitleBufferWritten,
                                      pcchTitleBufferNeeded,
                                      true);
}

HRESULT ApiRoutines::SetConsoleTitleAImpl(_In_reads_or_z_(cchTitleBufferSize) const char* const psTitleBuffer,
                                          _In_ size_t const cchTitleBufferSize)
{
    const CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    wistd::unique_ptr<wchar_t[]> pwsUnicodeTitleBuffer;
    size_t cchUnicodeTitleBuffer;
    RETURN_IF_FAILED(ConvertToW(gci->CP,
                                psTitleBuffer,
                                cchTitleBufferSize,
                                pwsUnicodeTitleBuffer,
                                cchUnicodeTitleBuffer));

    return SetConsoleTitleWImpl(pwsUnicodeTitleBuffer.get(), cchUnicodeTitleBuffer);
}

HRESULT ApiRoutines::SetConsoleTitleWImpl(_In_reads_or_z_(cchTitleBufferSize) const wchar_t* const pwsTitleBuffer,
                                          _In_ size_t const cchTitleBufferSize)
{
    LockConsole();
    auto Unlock = wil::ScopeExit([&] { UnlockConsole(); });

    return DoSrvSetConsoleTitleW(pwsTitleBuffer,
                                 cchTitleBufferSize);
}

HRESULT DoSrvSetConsoleTitleW(_In_reads_or_z_(cchBuffer) const wchar_t* const pwsBuffer,
                              _In_ size_t const cchBuffer)
{
    CONSOLE_INFORMATION* const gci = ServiceLocator::LocateGlobals()->getConsoleInformation();
    // Ensure that we add 1 to the length to leave room for a null if it's not already null terminated.
    size_t cchDest;
    RETURN_IF_FAILED(SizeTAdd(cchBuffer, 1, &cchDest));

    wistd::unique_ptr<wchar_t[]> pwszNewTitle = wil::make_unique_nothrow<wchar_t[]>(cchDest);
    RETURN_IF_NULL_ALLOC(pwszNewTitle);
    if (cchBuffer == 0)
    {
        pwszNewTitle[0] = L'\0';
    }
    else
    {
        // Safe string copy will ensure null termination.
        RETURN_IF_FAILED(StringCchCopyNW(pwszNewTitle.get(), cchDest, pwsBuffer, cchBuffer));
    }
    delete[] gci->Title;
    gci->Title = pwszNewTitle.release();

    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        RETURN_HR_IF_FALSE(E_FAIL, pWindow->PostUpdateTitleWithCopy(gci->Title));
    }

    return S_OK;
}

UINT LoadStringEx(_In_ HINSTANCE hModule, _In_ UINT wID, _Out_writes_(cchBufferMax) LPWSTR lpBuffer, _In_ UINT cchBufferMax, _In_ WORD wLangId)
{
    // Make sure the parms are valid.
    if (lpBuffer == nullptr)
    {
        return 0;
    }

    UINT cch = 0;

    // String Tables are broken up into 16 string segments.  Find the segment containing the string we are interested in.
    HANDLE const hResInfo = FindResourceEx(hModule, RT_STRING, (LPTSTR)((LONG_PTR)(((USHORT)wID >> 4) + 1)), wLangId);
    if (hResInfo != nullptr)
    {
        // Load that segment.
        HANDLE const hStringSeg = (HRSRC)LoadResource(hModule, (HRSRC)hResInfo);

        // Lock the resource.
        LPTSTR lpsz;
        if (hStringSeg != nullptr && (lpsz = (LPTSTR)LockResource(hStringSeg)) != nullptr)
        {
            // Move past the other strings in this segment. (16 strings in a segment -> & 0x0F)
            wID &= 0x0F;
            for (;;)
            {
                cch = *((WCHAR *)lpsz++);   // PASCAL like string count
                // first WCHAR is count of WCHARs
                if (wID-- == 0)
                {
                    break;
                }

                lpsz += cch;    // Step to start if next string
            }

            // chhBufferMax == 0 means return a pointer to the read-only resource buffer.
            if (cchBufferMax == 0)
            {
                *(LPTSTR *)lpBuffer = lpsz;
            }
            else
            {
                // Account for the nullptr
                cchBufferMax--;

                // Don't copy more than the max allowed.
                if (cch > cchBufferMax)
                    cch = cchBufferMax;

                // Copy the string into the buffer.
                memmove(lpBuffer, lpsz, cch * sizeof(WCHAR));
            }
        }
    }

    // Append a nullptr.
    if (cchBufferMax != 0)
    {
        lpBuffer[cch] = 0;
    }

    return cch;
}
