//====================================================
//
//  Solid File System (SolFS)
//
//   Copyright (c) 2003-2013, EldoS Corporation
//
//====================================================

#if !defined(STORE_DECL_H)
#define STORE_DECL_H
#pragma pack(8)

#if _MSC_VER > 1000
#  pragma once
#endif // _MSC_VER > 1000

//#define SOLFS_MEM_LOG

//#define SOLFS_SYNC

//#define SOLFS_MAX_STORAGE_SIZE 2048

//$ifdef 0

#if !defined(SOLFS_NO_CUSTOM) && !defined(SOLFS_CUSTOM_ALFEGA) && !defined(SOLFS_CUSTOM_LX) && !defined(SOLFS_CUSTOM_CS) && !defined(SOLFS_CUSTOM_WE) && !defined(SOLFS_CUSTOM_HB) && !defined(SOLFS_CUSTOM_ADVANCIS)
#	define SOLFS_NO_CUSTOM
#endif

#ifndef SOLFS_NO_CUSTOM
#	define SOLFS_NO_TRIAL
#endif // SOLFS_NT_DRV_API

//$endif

//$ifndef SOLFS_NO_CUSTOM
//#define SOLFS_NO_TRIAL
//$endif // SOLFS_NT_DRV_API

#ifndef __BORLANDC__
#	if defined(__GNUC__) && !(defined(__MINGW32__) || defined(__MINGW64__) || defined(__APPLE__) || defined(__FreeBSD__))
#		ifdef __INTERIX
#			include <sys/endian.h>
#		else
#			include <endian.h>
#		endif
#	endif
#	define SOLFS_LITTLE_ENDIAN   1234 /* byte 0 is least significant (i386) */
#	define SOLFS_BIG_ENDIAN      4321 /* byte 0 is most significant (mc68k) */
#	if !defined(SOLFS_BYTE_ORDER)
#	if defined(LITTLE_ENDIAN) || defined(BIG_ENDIAN)
#	  if defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#	    if defined(BYTE_ORDER)
#	      if   (BYTE_ORDER == LITTLE_ENDIAN)
#	        define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	      elif (BYTE_ORDER == BIG_ENDIAN)
#	        define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	      endif
#	    endif
#	  elif defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
#	    define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	  elif !defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#	    define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	  endif
#	elif defined(_LITTLE_ENDIAN) || defined(_BIG_ENDIAN)
#	  if defined(_LITTLE_ENDIAN) && defined(_BIG_ENDIAN)
#	    if defined(_BYTE_ORDER)
#	      if   (_BYTE_ORDER == _LITTLE_ENDIAN)
#	        define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	      elif (_BYTE_ORDER == _BIG_ENDIAN)
#	        define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	      endif
#	    endif
#	  elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#	    define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	  elif !defined(_LITTLE_ENDIAN) && defined(_BIG_ENDIAN)
#	    define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	  endif
#	elif 0     /* **** EDIT HERE IF NECESSARY **** */
#	define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	elif 0     /* **** EDIT HERE IF NECESSARY **** */
#	define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	elif (('1234' >> 24) == '1')
#	  define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#	elif (('4321' >> 24) == '1')
#	  define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#	endif
#	endif
#else
#  define SOLFS_BYTE_ORDER SOLFS_LITTLE_ENDIAN
#endif //__BORLANDC__

#if (defined(WIN32) || defined (_WIN32) || defined (x64)) && !defined(_WINDOWS) && !defined(SOLFS_NT_DRV)
#	define _WINDOWS
#endif // _WINDOWS

#if (defined(_UNICODE)) && (!defined(UNICODE))
#define UNICODE
#endif

#ifdef SOLFS_JAVA
#include <jni.h>
#endif

#ifdef SOLFS_ANDROID
#include <android/log.h>
#endif

#ifdef SOLFS_NT_DRV
#	define SOLFS_WINDOWS
#	pragma warning(disable: 4390)
#	ifdef _WIN64
#		define SOLFS_64BIT
#	endif
#	define SOLFS_WIN32
#	define SOLFS_NO_FPU
#	define SOLFS_ASSERT
#	define SOLFS_ASSERT_REWRITE
#endif // SOLFS_NT_DRV

#ifdef SOLFS_NT_DRV_API
#	define SOLFS_NO_TRIAL
#endif // SOLFS_NT_DRV_API

#ifdef _WINDOWS
#	define SOLFS_WINDOWS
#	include <windows.h>
#	include <winnt.h>
#	include <stdlib.h>
#if !(defined(__MINGW32__) || defined(__MINGW64__))
#	pragma warning(disable: 4267)
#	pragma warning(disable: 4390)
#	pragma warning(disable: 4996)
#endif
#	ifdef _WIN64
#		define SOLFS_64BIT
#	endif
#	if defined(_WIN32_WCE)
#		define SOLFS_WINCE
#	elif defined(_MANAGED)
#		define SOLFS_DOTNET
#		pragma warning(disable:4996)
//#		define _CRT_SECURE_NO_DEPRECATE
#	else
#		define SOLFS_WIN32
#	endif // _WIN32_WCE
#	ifdef B6_UP
#		include <algorith.h>
#	endif // B6_UP
#	ifdef __BORLANDC__
#		pragma warn -aus
#		pragma warn -pro
#	endif // __BCPLUSPLUS__
#endif // _WINDOWS

#if defined(__GNUC__) && !(defined(__MINGW32__) || defined(__MINGW64__))
#	define SOLFS_UNIX
#	ifdef _LP64
#		define SOLFS_64BIT
#	endif // _LP64
#	ifdef __APPLE__
#   define SOLFS_MACOS
#	endif // __APPLE__
#	ifdef __FreeBSD__
#   define SOLFS_FREEBSD
#	endif // __APPLE__
#	ifndef __USE_UNIX98
#		define __USE_UNIX98
#	endif // __USE_UNIX98
#	ifndef _LARGEFILE_SOURCE
#		define _LARGEFILE_SOURCE
#	endif // _LARGEFILE_SOURCE
#	ifndef _LARGEFILE64_SOURCE
#		define _LARGEFILE64_SOURCE
#	endif // _LARGEFILE64_SOURCE
#	ifndef _FILE_OFFSET_BITS
#		define _FILE_OFFSET_BITS 64
#	endif
//#include <assert.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#	include <features.h>
#endif
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <sys/time.h>
#	include <fcntl.h>
#	include <limits.h>
#	include <stdlib.h>
#	include <errno.h>
#	include <wchar.h>
#	include <locale.h>
//#if !defined(__FreeBSD__) && !defined(ANDROID)
#if !defined(ANDROID)
#	include <iconv.h>
#endif
#	include <pthread.h>
#	include <string.h>
#	include <stdint.h>
#	define _stdcall
//#	define max(a, b) (((a)>(b))?(a):(b))
//#	define min(a, b) (((a)<(b))?(a):(b))
#endif // __GNUC__

#if defined(__MWERKS__) && defined(__MC68K__)
#	define SOLFS_PALM
#	undef SOLFS_BYTE_ORDER
#	define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#elif defined(__palmos__)
#	define SOLFS_PALM
#	undef SOLFS_BYTE_ORDER
#	define SOLFS_BYTE_ORDER SOLFS_BIG_ENDIAN
#else
#	undef SOLFS_PALM
#endif

#ifdef SOLFS_PALM
#	include <PalmOS.h>
#	include <PalmTypes.h>
#	include <PalmCompatibility.h>
#	include <FileStream.h>
#	pragma warn_unusedarg off
#	pragma warn_unusedvar off
#	define _stdcall
#endif // SOLFS_PALM

#ifndef SOLFS_BYTE_ORDER
#  error byte order could not be determined
#endif

#if (defined(_DEBUG) || defined(DEBUG))
#	define SOLFS_ASSERT
#	define SOLFS_DEBUG
#endif // _DEBUG

#ifdef SOLFS_ASSERT
#	if defined(SOLFS_NT_DRV)
#       define assert(t) ASSERT(t)
#       if DBG 
#           ifdef RELEASE_BUILD
#               undef assert
#               define assert( exp ) \
                    ((!(exp)) ? \
                    KeBugCheckEx(FILE_SYSTEM, 0 | __LINE__, 0, 0, 0 ) : \
										TRUE)
#           endif //RELEASE_BUILD
#       endif //DBG
#	elif defined(SOLFS_ASSERT_REWRITE)
#		define assert(t) if (!(t)) StoreAssert(#t, __FILE__	, __LINE__)
#	elif defined(SOLFS_WINCE)
#		define assert(t)
#	else
#		include <assert.h>
#	endif
#endif // SOLFS_ASSERT

#ifndef SOLFS_DLL_EXPORT
#	define DLLEXPORTSPEC
#else
#	define DLLEXPORTSPEC __declspec(dllexport)
#endif

/*
#ifdef _WIN32_WCE
#	ifndef SOLFS_DLL_EXPORT
#		ifdef _X86_
#			define CALLCONVSPEC
#		else
#			define CALLCONVSPEC _stdcall
#		endif
#	else
#		define CALLCONVSPEC _stdcall
#	endif
#else
#	define CALLCONVSPEC _stdcall
#endif
*/

#ifdef _WIN32_WCE
#	ifndef SOLFS_DLL_EXPORT
#		define CALLCONVSPEC
#	else
#		define CALLCONVSPEC _stdcall
#	endif
#	ifndef _UNICODE
#		define _UNICODE
#	endif
#else
#	define CALLCONVSPEC _stdcall
#endif

#if defined(SOLFS_MACOS_FUSE)
#define SOLFS_FUSE
#elif defined(SOLFS_LINUX_FUSE)
#define SOLFS_FUSE
#elif defined(SOLFS_FREEBSD_FUSE)
#define SOLFS_FUSE
#endif

#ifdef SOLFS_WINDOWS
typedef unsigned __int8 SolFSChar;
typedef unsigned __int8 SolFSByte;
typedef unsigned __int16 SolFSWord;
typedef unsigned __int32 SolFSLongWord;
typedef signed __int32 SolFSLongInt;

typedef unsigned __int64 SolFSLongLongWord;
typedef signed __int64 SolFSLongLongInt;

typedef void* SolFSHandle;
typedef signed __int32 SolFSError;

#ifndef SOLFS_DOTNET
	#ifndef _WCHAR_T_DEFINED
		typedef unsigned short SolFSWideChar;
	#else
		typedef wchar_t SolFSWideChar;
	#endif
#else
	typedef wchar_t SolFSWideChar;
#endif

typedef unsigned __int32 SolFSUnicodeChar;

typedef SolFSWord TSeekOrigin;
#define soFromBegin 0
#define soFromCurrent 1
#define soFromEnd 2

#ifdef SOLFS_DOTNET
typedef double SolFSDateTime;
#define __declspec(a)
typedef unsigned long SolFSCallbackDataType;
//typedef void* SolFSCallbackDataType;
#else
typedef void* SolFSCallbackDataType;
#endif // SOLFS_DOTNET

#endif // SOLFS_WINDOWS

#ifdef SOLFS_UNIX
typedef u_int8_t SolFSChar;
typedef u_int8_t SolFSByte;
typedef u_int16_t SolFSWord;
typedef u_int32_t SolFSLongWord;
typedef int32_t SolFSLongInt;

typedef u_int64_t SolFSLongLongWord;
typedef int64_t SolFSLongLongInt;
typedef void* SolFSHandle;
typedef int32_t SolFSError;
typedef u_int16_t SolFSWideChar;
typedef u_int32_t SolFSUnicodeChar;

typedef int TSeekOrigin;
#define soFromBegin SEEK_SET
#define soFromCurrent SEEK_CUR
#define soFromEnd SEEK_END

typedef void *SolFSCallbackDataType;

#endif // SOLFS_UNIX

#ifdef SOLFS_DOTNET
#ifdef __BOOL_DEFINED
typedef bool SolFSBool;
#else
typedef unsigned char SolFSBool;
#endif
#else
typedef signed char SolFSBool;
#endif //SOLFS_DOTNET

typedef SolFSHandle* PSolFSHandle;

typedef SolFSBool * PSolFSBool;
typedef SolFSByte * PSolFSByte;
typedef void * SolFSPointer;
typedef SolFSChar * PSolFSChar;
typedef SolFSWord * PSolFSWord;
typedef SolFSLongWord * PSolFSLongWord;
typedef SolFSLongLongWord * PSolFSLongLongWord; 

typedef SolFSWideChar *PSolFSWideChar;
typedef SolFSUnicodeChar *PSolFSUnicodeChar;

#ifndef SOLFS_NO_FPU
typedef double SolFSDouble;
typedef double SolFSDateTime;
#else
typedef SolFSLongLongWord SolFSDouble;
typedef SolFSLongLongWord SolFSDateTime;
#endif //

#if !defined(SOLFS_MACOS)
typedef SolFSBool Bool;
typedef SolFSChar Char;
typedef SolFSByte Byte;
typedef SolFSWord Word;
typedef SolFSLongWord LongWord;
typedef SolFSLongInt LongInt;
typedef SolFSLongLongWord LongLongWord;
typedef SolFSLongLongInt LongLongInt;
typedef SolFSHandle Handle;
typedef SolFSError Error;
typedef SolFSWideChar WideChar;
typedef SolFSUnicodeChar UnicodeChar;
typedef SolFSDateTime DateTime;
typedef SolFSCallbackDataType CallbackDataType;
typedef SolFSDouble Double;

typedef PSolFSHandle PHandle;
typedef PSolFSByte PByte;
typedef SolFSPointer Pointer;
typedef PSolFSChar PChar;
typedef PSolFSWord PWord;
typedef PSolFSLongWord PLongWord;
typedef PSolFSWideChar PWideChar;
typedef PSolFSUnicodeChar PUnicodeChar;

#endif

#if defined(SOLFS_UNIX) || defined(__MINGW32__) || defined(__MINGW64__)
#define UnknownFileSize 0xFFFFFFFFFFFFFFFFULL
#else
#define UnknownFileSize 0xFFFFFFFFFFFFFFFFi64
#endif


#if defined(SOLFS_NT_DRV) || defined(SOLFS_NT_DRV_API)
#define SolFSVersion 0x03000000
#else
#define SolFSVersionCreated 0x04001000
#define SolFSVersionAllowed 0x04000000
#endif

#ifdef SOLFS_DOTNET
#ifdef __BOOL_DEFINED
#define True true
#define False false
#else
#define True -1
#define False 0
#endif
#else
#ifdef SOLFS_JAVA
#define True 1
#else
#define True -1
#endif
#define False 0
#endif //SOLFS_DOTNET

#define nil NULL

#define errInvalidStorageFile -1
#define errInvalidPageSize -2
#define errStorageFileCorrupted -3
#define errTooManyTransactionsCommiting -4
#define errFileOrDirectoryAlreadyExists -5
#define errExistsActiveTransactions -6
#define errTagAlreadyExistInFile -7
#define errFileNotFound -8
#define errPathNotFound -9
#define errSharingViolation -10
#define errSeekBeyondEOF -11
#define errNoMoreFiles -12
#define errInvalidFileName -13
#define errStorageActive -14
#define errStorageNotActive -15
#define errInvalidPassword -16
#define errStorageReadOnly -17
#define errNoEncryptionHandlers -18
#define errOutOfMemory -19
#define errLinkDestinationNotFound -20
#define errFileIsNotSymLink -21
#define errBufferTooSmall -22
#define errBadCompressedData -23
#define errInvalidParameter -24
#define errStorageFull -25
#define errInterruptedByUser -26
#define errTagNotFound -27
#define errDirectoryNotEmpty -28
#define errHandleClosed -29
#define errInvalidStreamHandle -30
#define errFileAccessDenied -31
#define errNoCompressionHandlers -32
#define errNotImplemented -33
#define errLicenseKeyNotSet -34
#define errDriverNotInstalled -35
#define errLicenseKeyExpired -36
#define errNewStorageVersion -37

#define errInternalError -200

//#ifdef SOLFS_PALM
//#define ERROR_OUTOFMEMORY	14L
//#endif

extern const SolFSWideChar strStorageLogo[];

#define ssFixedSize        0x00000001
#define ssReadOnly         0x00000002
#define ssCorrupted        0x00000004
#define ssTransactionsUsed 0x00000008
#define ssAccessTimeUsed   0x00000010
#define ssEncrypted        0x00000020
#define ssValidPasswordSet 0x00000040
#define ssPhysicalVolume   0x00000080
#define ssParted           0x00000100

#define ecUnknown 0xFFFFFFFF
#define ecNoEncryption 0x0
#define ecAES256_SHA256 0x1
#define ecCustom 0x2
#define ecAES256_HMAC256 0x3

#define crUnknown 0xFFFFFFFF
#define crNoCompression 0x0
#define crDefault 0x1
#define crCustom 0x2
#define crZLib 0x3
#define crRLE 0x4

#define attrFile 0x00000001
#define attrDirectory 0x00000002
#define attrDataTree 0x00000004
#define attrCompressed 0x00000008
#define attrEncrypted 0x00000010
#define attrSymLink 0x00000020

#define attrReadOnly 0x00000040
#define attrArchive 0x00000080
#define attrHidden 0x00000100
#define attrSystem 0x00000200
#define attrTemporary 0x00000400
#define attrDeleteOnClose 0x00000800
#define attrSecret 0x00001000
#define attrExtAttr 0x00002000

#define attrReserved1 0x00004000
#define attrReserved2 0x00008000

#define attrReservedLo 0x00004000
#define attrReservedHi 0x00008000

#define attrAnyFile 0xFFFFFFFF

#define attrNoUserChange \
	(attrFile | attrDirectory | attrDataTree | \
	attrCompressed | attrEncrypted | attrSymLink | \
	attrReserved1 | attrReserved2)
#define attrInheritable (attrCompressed)
#define attrUserDefined 0xFFFF0000

#define ftCreation 0x0001
#define ftModification 0x0002
#define ftLastAccess 0x0004
#define ftAllTimes 0x0007

#define fpUserID 0x0001
#define fpGroupID 0x0002
#define fpMode 0x0004
#define fpAll 0x0007

#define ffNeedFullName 0x01
#define ffNeedFileTimes 0x02
#define ffNeedFileSize 0x04
#define ffNeedProperties 0x08

#define ffEmulateFAT 0x8000

#define etCreated 0x0001
#define etDeleted 0x0002
#define etRenamed 0x0004
#define etAttributesChanged 0x0008
#define etDataSizeChanged 0x0010
#define etOtherChanged 0x8000

#define etAllEvents 0xFFFF

#define FileBlockSize 32

#define AES256_SHA256BlockSize 32
#define AES256_SHA256HashSize 32

#define AES256_HMAC256BlockSize 32
#define AES256_HMAC256HashSize 32

#define CustomBlockSize 32
#define CustomHashSize 32

#define ffFastFormat 0x0001
#define ffNoAllocFAT 0x0000
#define ffAllocFAT64K 0x0002
#define ffAllocFAT256K 0x0004
#define ffAllocFAT1M 0x0006
#define ffAllocFAT2M 0x0008
#define ffAllocFAT4M 0x000A
#define ffAllocFAT8M 0x000C
#define ffAllocFAT16M 0x000E
#define ffFormatFAT 0x0010
#define ffCreatePartition 0x0020

#define poFormatting 0
#define poChecking1 1
#define poChecking2 2
#define poChecking3 3
#define poChecking4 4
#define poChecking5 5
#define poReserved1 6
#define poReserved2 7
#define poPageCorrupted 8
#define poPageOrphaned 9
#define poCompressing 10
#define poDecompressing 11
#define poEncrypting 12
#define poDecrypting 13
#define poCompacting 14

#define tgFirstUserTag 0x8000
#define tgLastUserTag 0xCFFF

#define chrCheckOnly 0x0001
#define chrCheckAllPages 0x0002

#define tvtBoolean 1
#define tvtString 2
#define tvtDateTime 3
#define tvtNumber 4
#define tvtAnsiString 100

#define fqfRecurseSubdirectories 0x01

#define fmFileType   0170000
#define fmFile       0100000
#define fmDirectory  0040000
#define fmSymLink    0120000
#define fmOwnerRead  0000400
#define fmOwnerWrite 0000200
#define fmOwnerExec  0000100
#define fmGroupRead  0000040
#define fmGroupWrite 0000020
#define fmGroupExec  0000010
#define fmOtherRead  0000004
#define fmOtherWrite 0000002
#define fmOtherExec  0000001

typedef struct StorageSearchStruct {
	SolFSHandle SearchHandle;
	PSolFSWideChar FullName;
	PSolFSWideChar FileName;
	SolFSLongWord Attributes;
	SolFSDateTime Creation;
	SolFSDateTime Modification;
	SolFSDateTime LastAccess;
	SolFSLongLongWord FileSize;
	SolFSLongWord FileID;
	SolFSLongWord FileMode;
	SolFSLongWord UserID;
  SolFSLongWord GroupID;
} TStorageSearch, *PStorageSearch;

typedef struct StorageChangeStruct {
	SolFSLongWord EventType;
	SolFSDateTime ChangeDataTime;
	PSolFSWideChar Name;
	PSolFSWideChar NewName;
	SolFSLongWord Attributes;
	SolFSLongLongWord FileSize;
	SolFSHandle Internal;
} TStorageChange, *PStorageChange;

#define SOLFS_PROCESS_GRANTED		1
#define SOLFS_PROCESS_DENIED		2

#define SOLFS_PROCESS_ACCESS_READ	1
#define SOLFS_PROCESS_ACCESS_WRITE	2

#define SOLFS_PROCESS_BY_ID			1
#define SOLFS_PROCESS_BY_NAME		2

#define SOLFS_PROCESS_ID_ALL		(~0)

typedef union StorageProcessUnion {
	SolFSLongWord ProcessID;
	PSolFSWideChar ProcessFileName;
} TStorageProcess, *PStorageProcess;

typedef struct StorageProcessRestrictionStruct {
	SolFSLongWord RestrictionType;
	SolFSLongWord DesiredAccess;
	SolFSBool ChildProcesses;
	SolFSLongWord ProcessType;
	TStorageProcess Process;
} TStorageProcessRestriction, *PStorageProcessRestriction;

typedef SolFSLongInt (_stdcall *PTreeCompareProc)(SolFSLongWord RecordSize1,
	SolFSPointer RecordData1, SolFSLongWord RecordSize2, SolFSPointer RecordData2);

typedef SolFSError (_stdcall *SolFSIsCompactAllovedFunc)(SolFSCallbackDataType UserData,
	SolFSBool *IsCompactAlloved);

typedef SolFSError (_stdcall *SolFSCreateFileFunc)(SolFSCallbackDataType UserData,
	PSolFSWideChar FileName, PSolFSHandle File, SolFSBool Overwrite, SolFSBool IsJournalFile);
typedef SolFSError (_stdcall *SolFSOpenFileFunc)(SolFSCallbackDataType UserData,
	PSolFSWideChar FileName, PSolFSHandle File, SolFSBool* ReadOnly, SolFSBool IsJournalFile);
typedef SolFSError (_stdcall *SolFSDeleteFileFunc)(SolFSCallbackDataType UserData,
	PSolFSWideChar FileName);
typedef SolFSError (_stdcall *SolFSCloseFileFunc)(SolFSCallbackDataType UserData,
	PSolFSHandle File);
typedef SolFSError (_stdcall *SolFSFlushFileFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File);
typedef SolFSError (_stdcall *SolFSGetFileSizeFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSLongLongWord * Size);
typedef SolFSError (_stdcall *SolFSSetFileSizeFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSLongLongWord NewSize);
typedef SolFSError (_stdcall *SolFSSeekFileFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSLongLongInt Offset, TSeekOrigin Origin);
typedef SolFSError (_stdcall *SolFSReadFileFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSPointer Buffer, SolFSLongWord Count);
typedef SolFSError (_stdcall *SolFSWriteFileFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSPointer Buffer, SolFSLongWord Count);

typedef SolFSError (_stdcall *SolFSGetParentSizeFunc)(SolFSCallbackDataType UserData,
	SolFSHandle File, SolFSLongLongWord *TotalSize, SolFSLongLongWord *CallerAvailableSize,
	SolFSLongLongWord *ActualAvailableSize);

typedef SolFSError (_stdcall *SolFSCalculateHashFunc)(SolFSCallbackDataType UserData,
	SolFSPointer Buffer, SolFSLongWord Count, SolFSPointer HashBuffer);
typedef SolFSError (_stdcall *SolFSValidateHashFunc)(SolFSCallbackDataType UserData,
	SolFSPointer Buffer, SolFSLongWord Count, SolFSPointer HashBuffer, SolFSBool *Valid);
typedef SolFSError (_stdcall *SolFSCryptDataFunc)(SolFSCallbackDataType UserData,
	PSolFSChar Key, SolFSLongWord KeyLength, PSolFSChar Data, SolFSLongWord DataSize,
	SolFSLongWord ObjectID, SolFSLongWord PageIndex);

typedef SolFSError (_stdcall *SolFSCompressDataFunc)(SolFSCallbackDataType UserData,
	PSolFSChar InData, SolFSLongWord InDataSize, PSolFSChar OutData, SolFSLongWord *OutDataSize,
	SolFSLongWord CompressionLevel);
typedef SolFSError (_stdcall *SolFSDecompressDataFunc)(SolFSCallbackDataType UserData,
	PSolFSChar InData, SolFSLongWord InDataSize, PSolFSChar OutData, SolFSLongWord *OutDataSize);

typedef void (_stdcall *SolFSProgressFunc)(SolFSCallbackDataType UserData,
	SolFSLongWord Operation, SolFSLongWord Progress, SolFSLongWord Total,
	SolFSBool CanStop, SolFSBool *Stop);

typedef void (_stdcall *SolFSChangeNotificationFunc)(SolFSCallbackDataType UserData,
	PStorageChange Change, SolFSHandle ChangeNotification);

#if !defined(SOLFS_NATIVE_LINK) || defined(SOLFS_NT_DRV_API)

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef SOLFS_JAVA
DLLEXPORTSPEC void CALLCONVSPEC StorageSetJVM(JavaVM *JVM);
#endif

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageInitialize(PSolFSChar ProgramName);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageInit();
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDone();

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetRegistrationKey(PSolFSChar Key);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetVersion(SolFSLongWord *Version);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCheckAndRepair(PSolFSWideChar FileName,
	SolFSLongWord PageSize, SolFSLongWord Flags, PSolFSWideChar Password, SolFSLongWord PasswordLen,
	SolFSCallbackDataType ProgressUserData, SolFSProgressFunc ProgressFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCheckAndRepairCB(PSolFSWideChar FileName,
	SolFSLongWord PageSize, SolFSLongWord Flags, PSolFSWideChar Password, SolFSLongWord PasswordLen,
	SolFSCallbackDataType ProgressUserData, SolFSProgressFunc ProgressFunc,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCheckAndRepairCB2(PSolFSWideChar FileName,
	SolFSLongWord PageSize, SolFSLongWord Flags, PSolFSWideChar Password, SolFSLongWord PasswordLen,
	SolFSCallbackDataType ProgressUserData, SolFSProgressFunc ProgressFunc,
	// callbacks
	SolFSBool UseCallbacks, SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc,

	SolFSCallbackDataType CryptUserData,
	SolFSCalculateHashFunc CalculateHashFunc,
	SolFSValidateHashFunc ValidateHashFunc,
	SolFSCryptDataFunc EncryptDataFunc,
	SolFSCryptDataFunc DecryptDataFunc,

	SolFSCallbackDataType CompressUserData,
	SolFSCompressDataFunc CompressDataFunc,
	SolFSDecompressDataFunc DecompressDataFunc);

#ifdef SOLFS_RESQUE
SolFSError CALLCONVSPEC StorageResque(PSolFSWideChar FileName,
  PSolFSWideChar NewPassword, SolFSLongWord NewPasswordLen,
  SolFSLongWord OldPasswordPage,
  PSolFSWideChar OldPassword, SolFSLongWord OldPasswordLen,
  SolFSLongWord FirstPage, SolFSLongWord LastPage,
	SolFSCallbackDataType ProgressUserData, SolFSProgressFunc ProgressFunc);
#endif // SOLFS_RESQUE

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFormatFixedSize(
	PSolFSWideChar FileName, SolFSLongLongWord FileSize, SolFSLongWord PageSize,
	PSolFSWideChar Logo, SolFSLongWord Flags, SolFSCallbackDataType ProgressUserData,
	SolFSProgressFunc ProgressFunc);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFormatFixedSizeParted(
  PSolFSWideChar FileName, SolFSLongLongWord FileSize, SolFSLongWord PartSize,
  SolFSLongWord PageSize, PSolFSWideChar Logo, SolFSLongWord Flags,
  SolFSCallbackDataType ProgressUserData, SolFSProgressFunc ProgressFunc);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFormatFixedSizeCB(
	PSolFSWideChar FileName, SolFSLongLongWord FileSize, SolFSLongWord PageSize,
	PSolFSWideChar Logo, SolFSLongWord Flags, SolFSCallbackDataType ProgressUserData,
	SolFSProgressFunc ProgressFunc,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsValidStorage(PSolFSWideChar FileName);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsValidStorageEx(PSolFSWideChar FileName,
	SolFSBool *Corrupted, SolFSLongWord *StorageID, SolFSLongWord *PageSize,
	PSolFSWideChar LogoBuffer, SolFSLongWord LogoBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsValidStorageCB(PSolFSWideChar FileName,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsValidStorageExCB(PSolFSWideChar FileName,
	SolFSBool *Corrupted, SolFSLongWord *StorageID, SolFSLongWord *PageSize,
	PSolFSWideChar LogoBuffer, SolFSLongWord LogoBufferSize,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreate(PSolFSWideChar FileName,
	SolFSBool Overwrite, SolFSLongWord PageSize, PSolFSWideChar Logo, PSolFSHandle Storage,
	SolFSWideChar PathSeparator, SolFSBool UseTransactions, SolFSBool UseAccessTime);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateParted(PSolFSWideChar FileName,
	SolFSBool Overwrite, SolFSLongWord PartSize, SolFSLongWord PageSize,
  PSolFSWideChar Logo, PSolFSHandle Storage, SolFSWideChar PathSeparator,
  SolFSBool UseTransactions, SolFSBool UseAccessTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateCB(
	PSolFSWideChar FileName, SolFSBool Overwrite, SolFSLongWord PageSize, PSolFSWideChar Logo,
	PSolFSHandle Storage, SolFSWideChar PathSeparator, SolFSBool UseTransactions,
	SolFSBool UseAccessTime, SolFSBool ReadOnly,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpen(
	PSolFSWideChar FileName, PSolFSHandle Storage, SolFSWideChar PathSeparator,
	SolFSBool UseTransactions, SolFSBool UseAccessTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpenReadOnly(
	PSolFSWideChar FileName, PSolFSHandle Storage, SolFSWideChar PathSeparator,
	SolFSBool UseTransactions, SolFSBool UseAccessTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpenCB(
	PSolFSWideChar FileName, PSolFSHandle Storage, SolFSWideChar PathSeparator,
	SolFSBool UseTransactions, SolFSBool UseAccessTime, SolFSBool ReadOnly,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageReopen(SolFSHandle Storage,
	PSolFSWideChar FileName);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageReopenCB(SolFSHandle Storage,
	PSolFSWideChar FileName,
// callbacks
	SolFSCallbackDataType UserData,
	SolFSCreateFileFunc CreateFileFunc, SolFSOpenFileFunc OpenFileFunc,
	SolFSCloseFileFunc CloseFileFunc, SolFSFlushFileFunc FlushFileFunc,
	SolFSDeleteFileFunc DeleteFileFunc, SolFSGetFileSizeFunc GetFileSizeFunc,
	SolFSSetFileSizeFunc SetFileSizeFunc, SolFSSeekFileFunc SeekFileFunc,
	SolFSReadFileFunc ReadFileFunc, SolFSWriteFileFunc WriteFileFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetRegistrationKeyEx(
	SolFSHandle Storage, PSolFSChar Key);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetState(SolFSHandle Storage,
  SolFSLongWord *State);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFlushBuffers(SolFSHandle Storage);

// Functions specific to Driver Edition

#define SOLFS_MODULE_PNP_BUS_DRIVER         0x00000001
#define SOLFS_MODULE_DISK_DRIVER            0x00000002
#define SOLFS_MODULE_FS_DRIVER              0x00000004
#define SOLFS_MODULE_NET_REDIRECTOR_DLL     0x00010000
#define SOLFS_MODULE_MOUNT_NOTIFIER_DLL     0x00020000

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageInstall(
  PSolFSChar ProgramName, PSolFSWideChar CabFileName,
  PSolFSWideChar PathToInstall, SolFSBool SupportPnP,
  SolFSLongWord ModulesToInstall, SolFSLongWord *RebootNeeded);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageUninstall(
  PSolFSChar ProgramName, PSolFSWideChar CabFileName,
  PSolFSWideChar PathToInstall, SolFSLongWord *RebootNeeded);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetModuleStatus(
  PSolFSChar ProgramName, SolFSLongWord Module, SolFSBool *Installed,
  SolFSLongWord *FileVersionHigh,	SolFSLongWord *FileVersionLow,
  SolFSPointer ServiceStatus);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageForceUnmount(PSolFSWideChar FileName);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetForceUnmount(SolFSHandle Storage,
	SolFSBool ForceUnmount);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetUseSystemCache(
  PSolFSBool UseSystemCache);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetUseSystemCache(
  SolFSBool UseSystemCache);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetUseSystemCacheEx(
	SolFSHandle StorageHandle, PSolFSBool UseSystemCache);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetUseSystemCacheEx(
	SolFSHandle StorageHandle, SolFSBool UseSystemCache);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetDestroyStorageOnProcessTerminated(
	SolFSHandle Storage, SolFSBool *Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetDestroyStorageOnProcessTerminated(
	SolFSHandle Storage, SolFSBool Value);

#define SOLFS_SYMLINK_SIMPLE                        0x00010000
#define SOLFS_SYMLINK_MOUNT_MANAGER                 0x00020000
#define SOLFS_SYMLINK_NETWORK                       0x00040000

#define SOLFS_SYMLINK_LOCAL                         0x10000000

#define SOLFS_SYMLINK_NETWORK_ALLOW_MAP_AS_DRIVE    0x00000001
#define SOLFS_SYMLINK_NETWORK_HIDDEN_SHARE          0x00000002
#define SOLFS_SYMLINK_NETWORK_READ_NETWORK_ACCESS   0x00000004
#define SOLFS_SYMLINK_NETWORK_WRITE_NETWORK_ACCESS  0x00000008

#define SOLFS_SYMLINK_DEBUG                         0x40000000
#define SOLFS_SYMLINK_SYSTEM_DEBUG                  0x80000000

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMountingPointCount(
	SolFSHandle Storage, SolFSLongWord *MountingPointCount);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMountingPoint(
	SolFSHandle Storage, SolFSLongWord Index, PSolFSWideChar MountingPointBuffer,
	SolFSLongWord *MountingPointBufferSize, SolFSLongWord *Flags,
	SolFSPointer AuthenticationId);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAddMountingPoint(
	SolFSHandle Storage, PSolFSWideChar MountingPoint,
	SolFSLongWord Flags, SolFSPointer AuthenticationId);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteMountingPoint(
	SolFSHandle Storage, PSolFSWideChar MountingPoint,
	SolFSLongWord Flags, SolFSPointer AuthenticationId);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageConvertFileNameToSystem(
	SolFSHandle Storage, PSolFSWideChar StorageFileName, PSolFSWideChar SystemFileNameBuffer,
	SolFSLongWord *SystemFileNameBufferSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageConvertFileNameToStorage(
	SolFSHandle Storage, PSolFSWideChar SystemFileName, PSolFSWideChar StorageFileNameBuffer,
	SolFSLongWord *StorageFileNameBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetProcessRestrictionsEnabled(
	SolFSHandle Storage, SolFSBool RestrictionsEnabled);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetProcessRestrictionsEnabled(
	SolFSHandle Storage, SolFSBool *RestrictionsEnabled);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetProcessRestrictionCounts(
	SolFSHandle Storage, SolFSLongWord *ProcessRestrictionCount,
	SolFSLongWord *GrantedProcessCount, SolFSLongWord *DeniedProcessCount);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetProcessRestriction(
	SolFSHandle Storage, SolFSLongWord Index, PStorageProcessRestriction ProcessRestriction);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageClearProcessRestrictionStruct(
	PStorageProcessRestriction ProcessRestriction);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAddProcessRestriction(
	SolFSHandle Storage, PStorageProcessRestriction ProcessRestriction);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteProcessRestriction(
	SolFSHandle Storage, PStorageProcessRestriction ProcessRestriction);

SolFSError CALLCONVSPEC StorageSetParentSizeCallback(
	SolFSHandle Storage, SolFSGetParentSizeFunc GetParentSizeFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetCallbackTimeout(
	PSolFSLongWord CallbackTimeout);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetCallbackTimeout(
	SolFSLongWord CallbackTimeout);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetDiskProperties(
	SolFSLongWord *Type, SolFSLongWord *Characteristics);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetDiskProperties(
	SolFSLongWord Type, SolFSLongWord Characteristics);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageInstallIcon(
  PSolFSWideChar IconPath, PSolFSWideChar IconId, SolFSBool *RebootNeeded);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageUninstallIcon(
  PSolFSWideChar IconId, SolFSBool *RebootNeeded);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsIconInstalled(
  PSolFSWideChar IconId, SolFSBool *Installed);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetIcon(
  SolFSHandle Storage, PSolFSWideChar IconId);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageResetIcon(SolFSHandle Storage);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetVolumeProperties(
  PSolFSWideChar VolumePath,
  SolFSBool *SolFSFormatted, SolFSLongWord *Encryption);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFormatVolume(
  PSolFSWideChar VolumePath,
  SolFSLongWord PageSize, PSolFSWideChar Logo, SolFSLongWord Flags,
  SolFSCallbackDataType ProgressUserData,	SolFSProgressFunc ProgressFunc);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpenVolume(
  PSolFSWideChar VolumePath, PSolFSHandle Storage,
  SolFSBool UseTransactions, SolFSBool UseAccessTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsPhysicalVolume(
  SolFSHandle Storage, SolFSBool *PhysicalVolume);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageEjectVolume(
  SolFSHandle Storage);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileSystemName(
  SolFSHandle Storage, PSolFSWideChar FileSyetemName);

// End of functions specific to Driver Edition

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetCustomEncryptionHandlers(
	SolFSHandle Storage,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCalculateHashFunc CalculateHashFunc,
	SolFSValidateHashFunc ValidateHashFunc,
	SolFSCryptDataFunc EncryptDataFunc,
	SolFSCryptDataFunc DecryptDataFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetCustomCompressionHandlers(
	SolFSHandle Storage,
	// callbacks
	SolFSCallbackDataType UserData,
	SolFSCompressDataFunc CompressDataFunc,
	SolFSDecompressDataFunc DecompressDataFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageClose(SolFSHandle Storage);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpenRootData(SolFSHandle Storage, PSolFSHandle File);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetEncryption(SolFSHandle Storage,
	SolFSLongWord *Encryption);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCheckPassword(SolFSHandle Storage,
	PSolFSWideChar Password, SolFSLongWord PasswordLen, SolFSBool *Valid,
	SolFSLongWord *ActualEncryption);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetPassword(SolFSHandle Storage,
	PSolFSWideChar Password, SolFSLongWord PasswordLen);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetEncryption(SolFSHandle Storage,
	SolFSLongWord Encryption, PSolFSWideChar OldPassword, SolFSLongWord OldPasswordLen,
	PSolFSWideChar NewPassword, SolFSLongWord NewPasswordLen);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetEncryptionEx(SolFSHandle Storage,
	SolFSLongWord Encryption, PSolFSWideChar OldPassword, SolFSLongWord OldPasswordLen,
	PSolFSWideChar NewPassword, SolFSLongWord NewPasswordLen, SolFSCallbackDataType UserData,
	SolFSProgressFunc ProgressFunc);

//$ifndef SOLFS_NO_CUSTOM
//$ifdef SOLFS_CUSTOM_ALFEGA
//$ifdef 0
#ifdef SOLFS_CUSTOM_ALFEGA
//$endif
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAddPasswordRecovery(SolFSHandle Storage,
	PSolFSWideChar Password, SolFSLongWord PasswordLen,
	PSolFSWideChar Question, SolFSLongWord QuestionLen,
	PSolFSWideChar Answer, SolFSLongWord AnswerLen);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetPasswordRecovery(SolFSHandle Storage,
	PSolFSWideChar Question, SolFSLongWord *QuestionLen);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeletePasswordRecovery(SolFSHandle Storage);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageRecoverPassword(SolFSHandle Storage,
	PSolFSWideChar Answer, SolFSLongWord AnswerLen,
	PSolFSWideChar Password, SolFSLongWord *PasswordLen);
//$ifdef 0
#endif
//$endif
//$endif
//$endif

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsCorrupted(SolFSHandle Storage,
	SolFSBool *Corrupted);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetStorageID(SolFSHandle Storage,
	SolFSLongWord *StorageID);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsMediaChanged(SolFSHandle Storage,
	SolFSBool *Changed);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsReadOnly(SolFSHandle Storage,
	SolFSBool *ReadOnly);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsFixedSize(SolFSHandle Storage,
	SolFSBool *FixedSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetInfo(SolFSHandle Storage,
	SolFSLongWord *PageSize, PSolFSWideChar *Logo);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetSizes(SolFSHandle Storage,
	SolFSBool *FixedSize, SolFSLongLongWord *TotalSize, SolFSLongLongWord *FreeSpace);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetCurrentSizes(SolFSHandle Storage,
	SolFSBool *FixedSize, SolFSLongLongWord *TotalSize, SolFSLongLongWord *FreeSpace);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetPartSize(SolFSHandle Storage,
	SolFSLongWord *PartSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMaxPageCount(SolFSHandle Storage,
	SolFSLongWord *PageCount);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetMaxPageCount(SolFSHandle Storage,
	SolFSLongWord PageCount);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetStorageSize(SolFSHandle Storage,
	SolFSLongLongWord NewSize, SolFSCallbackDataType ProgressUserData,
	SolFSProgressFunc ProgressFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetLogo(SolFSHandle Storage,
	PSolFSWideChar Logo);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetCaseSensitive(SolFSHandle Storage,
	SolFSBool *Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetCaseSensitive(SolFSHandle Storage,
	SolFSBool Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetSeparator(SolFSHandle Storage,
	SolFSWideChar *Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetSeparator(SolFSHandle Storage,
	SolFSWideChar Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetUseTransactions(SolFSHandle Storage,
	SolFSBool *Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetUseTransactions(SolFSHandle Storage,
	SolFSBool Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetUseAccessTime(SolFSHandle Storage,
	SolFSBool *Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetUseAccessTime(SolFSHandle Storage,
	SolFSBool Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetLastAccessTime(SolFSHandle Storage,
	SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetLastWriteTime(SolFSHandle Storage,
	SolFSDateTime *Time);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetAutoCompact(SolFSHandle Storage,
	SolFSLongWord *Percent);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetAutoCompact(SolFSHandle Storage,
	SolFSLongWord Percent);
#ifndef SOLFS_NO_CB
SolFSError CALLCONVSPEC StorageSetAutoCompactEx(SolFSHandle Storage, SolFSLongWord Percent,
	SolFSCallbackDataType UserData, SolFSIsCompactAllovedFunc IsCompactAllovedFunc);
#endif // SOLFS_NO_CB

/*
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetDefaultCompression(
	SolFSHandle Storage, SolFSLongWord *Compression, SolFSLongWord *CompressionLevel,
	SolFSLongWord *PagesPerCluster, SolFSBool *UseForNewFiles);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetDefaultCompression(
	SolFSHandle Storage, SolFSLongWord Compression, SolFSLongWord CompressionLevel,
	SolFSLongWord PagesPerCluster, SolFSBool UseForNewFiles);
*/

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetBuffering(SolFSHandle Storage,
	SolFSLongWord *PagesPerCluster);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetBuffering(SolFSHandle Storage,
	SolFSLongWord PagesPerCluster);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMaxTransactionSize(SolFSHandle Storage,
	SolFSLongWord *Pages);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetMaxTransactionSize(SolFSHandle Storage,
	SolFSLongWord Pages);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetPageCacheSize(SolFSHandle Storage,
	SolFSLongWord *CacheSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetPageCacheSize(SolFSHandle Storage,
	SolFSLongWord CacheSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCompact(SolFSHandle Storage,
	SolFSBool *Compacted);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCompactEx(SolFSHandle Storage,
	SolFSBool *Compacted, SolFSLongWord Flags, SolFSCallbackDataType ProgressUserData,
  SolFSProgressFunc ProgressFunc);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOnIdle(SolFSHandle Storage);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFileExists(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSBool *Exists);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateDirectory(SolFSHandle Storage,
	PSolFSWideChar Directory);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageForceCreateDirectories(SolFSHandle Storage,
	PSolFSWideChar Path);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteDirectory(SolFSHandle Storage,
	PSolFSWideChar Directory);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageIsDirectoryEmpty(SolFSHandle Storage,
	PSolFSWideChar Directory, SolFSBool *Empty);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileEncryption(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord *Encryption);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileEncryption(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord Encryption,
	PSolFSWideChar OldPassword, SolFSLongWord OldPasswordLen,
	PSolFSWideChar NewPassword, SolFSLongWord NewPasswordLen);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCheckFilePassword(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord Encryption, SolFSBool EncryptionUnknown,
	PSolFSWideChar Password, SolFSLongWord PasswordLen, SolFSBool *Valid);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileCompression(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord *Compression, SolFSLongWord *CompressionLevel,
	SolFSLongWord *PagesPerCluster);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileCompression(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord Compression, SolFSLongWord CompressionLevel,
	SolFSLongWord PagesPerCluster, PSolFSWideChar Password, SolFSLongWord PasswordLen);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileAttributes(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord *Attributes);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileAttributes(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord Attributes);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileProperties(SolFSHandle Storage,
	PSolFSWideChar FileName, PStorageSearch Search, SolFSLongWord Flags);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileUserGroup(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord UserID, SolFSLongWord GroupID);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileMode(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSLongWord FileMode);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageLink(SolFSHandle Storage,
	PSolFSWideChar LinkName, PSolFSWideChar DestinationName);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetLinkDestination(SolFSHandle Storage,
	PSolFSWideChar LinkName, PSolFSWideChar *DestinationName);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetLinkDestinationEx(SolFSHandle Storage,
	PSolFSWideChar LinkName, PSolFSWideChar DestinationBuffer,
	SolFSLongWord *DestinationBufferSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageResolveLink(SolFSHandle Storage,
	PSolFSWideChar LinkName, PSolFSWideChar DestinationBuffer,
	SolFSLongWord *DestinationBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileCreationTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileCreationTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime NewTime);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileModificationTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileModificationTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime NewTime);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileLastAccessTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileLastAccessTime(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime NewTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTimes(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime *Creation, SolFSDateTime *Modification,
	SolFSDateTime *LastAccess);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTimes(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSDateTime Creation, SolFSDateTime Modification,
	SolFSDateTime LastAccess);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagInfo(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSWord TagID, SolFSBool *TagExists, SolFSLongWord *TagDataSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTag(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSWord TagID, SolFSPointer TagData, SolFSLongWord *TagDataSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTag(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSWord TagID, SolFSPointer TagData, SolFSLongWord TagDataSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteFileTag(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSWord TagID);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindFirst(SolFSHandle Storage,
	PStorageSearch Search, PSolFSWideChar Mask, SolFSLongWord Attributes);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindFirstEx(SolFSHandle Storage,
	PStorageSearch Search, PSolFSWideChar Mask, SolFSLongWord Attributes, SolFSLongWord Flags);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindNext(SolFSHandle Storage,
	PStorageSearch Search);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindClose(SolFSHandle Storage,
	PStorageSearch Search);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAddChangeNotification(SolFSHandle Storage,
	PSolFSWideChar Path, SolFSBool Recursive, SolFSLongWord EventMask,
	SolFSCallbackDataType CallbackUserData,
	SolFSChangeNotificationFunc NotificationFunc, PSolFSHandle ChangeNotification);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageRemoveChangeNotification(SolFSHandle Storage,
	SolFSHandle ChangeNotification);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFilePassword(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar Password, SolFSLongWord PasswordLen);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateFile(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSBool ReadEnabled, SolFSBool WriteEnabled,
	SolFSBool ShareDenyRead, SolFSBool ShareDenyWrite,
	SolFSLongWord Encryption, PSolFSWideChar Password, SolFSLongWord PasswordLen, PSolFSHandle File,
	SolFSBool OpenExisting, SolFSBool TruncateExisting);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateFileWithAttributes(
    SolFSHandle Storage, PSolFSWideChar FileName, SolFSBool ReadEnabled,
    SolFSBool WriteEnabled, SolFSBool ShareDenyRead, SolFSBool ShareDenyWrite,
	SolFSLongWord Encryption, PSolFSWideChar Password, SolFSLongWord PasswordLen,
    PSolFSHandle File, SolFSBool OpenExisting, SolFSBool TruncateExisting,
    SolFSLongWord Attributes);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCreateFileCompressed(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSBool ReadEnabled, SolFSBool WriteEnabled,
	SolFSBool ShareDenyRead, SolFSBool ShareDenyWrite,
	SolFSLongWord Encryption, PSolFSWideChar Password, SolFSLongWord PasswordLen,
	SolFSLongWord Compression, SolFSLongWord CompressionLevel, SolFSLongWord PagesPerCluster,
	PSolFSHandle File, SolFSBool OpenExisting, SolFSBool TruncateExisting);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageOpenFile(SolFSHandle Storage,
	PSolFSWideChar FileName, SolFSBool ReadEnabled, SolFSBool WriteEnabled,
	SolFSBool ShareDenyRead, SolFSBool ShareDenyWrite,
	PSolFSWideChar Password, SolFSLongWord PasswordLen, PSolFSHandle File);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageCloseFile(SolFSHandle File);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFlushFile(SolFSHandle File);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFlushFileEx(SolFSHandle File,
	SolFSLongWord Flags);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteFile(SolFSHandle Storage,
	PSolFSWideChar FileName);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageMoveFile(SolFSHandle Storage,
	PSolFSWideChar OldFileName, PSolFSWideChar NewFileName);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteAndRenameFile(SolFSHandle Storage,
	PSolFSWideChar OldFileName, PSolFSWideChar NewFileName);

// Opened files properties get and set functions

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetOpenedFileAttributes(
  SolFSHandle File, SolFSLongWord *Attributes);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetOpenedFileAttributes(
  SolFSHandle File, SolFSLongWord Attributes);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetOpenedFileCreationTime(
  SolFSHandle File, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetOpenedFileCreationTime(
  SolFSHandle File, SolFSDateTime NewTime);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetOpenedFileModificationTime(
  SolFSHandle File, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetOpenedFileModificationTime(
  SolFSHandle File, SolFSDateTime NewTime);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetOpenedFileLastAccessTime(
  SolFSHandle File, SolFSDateTime *Time);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetOpenedFileLastAccessTime(
  SolFSHandle File, SolFSDateTime NewTime);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetOpenedFileTimes(
  SolFSHandle File, SolFSDateTime *Creation,
  SolFSDateTime *Modification, SolFSDateTime *LastAccess);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetOpenedFileTimes(
  SolFSHandle File, SolFSDateTime Creation,
  SolFSDateTime Modification, SolFSDateTime LastAccess);

//

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileSize(SolFSHandle File,
	SolFSLongWord *FileSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileSizeLong(SolFSHandle File,
	SolFSLongLongWord *FileSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileDataSizeLong(SolFSHandle File,
	SolFSLongLongWord *FileDataSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileDataSizeLong(SolFSHandle File,
	SolFSLongLongWord FileDataSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileEndLong(SolFSHandle File,
	SolFSLongLongWord FileSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileSize(SolFSHandle File,
	SolFSLongWord FileSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileSizeLong(SolFSHandle File,
	SolFSLongLongWord FileSize);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetEndOfFile(SolFSHandle File);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSeekFile(SolFSHandle File,
	SolFSLongInt Offset, TSeekOrigin Origin, SolFSLongWord *Position);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSeekFileLong(SolFSHandle File,
	SolFSLongLongInt Offset, TSeekOrigin Origin, SolFSLongLongWord *Position);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageTellFile(SolFSHandle File,
	SolFSLongWord *Position);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageTellFileLong(SolFSHandle File,
	SolFSLongLongWord *Position);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageReadFile(SolFSHandle File,
	SolFSPointer Buffer, SolFSLongInt BufferSize, SolFSLongWord *Read);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageWriteFile(SolFSHandle File,
	SolFSPointer Buffer, SolFSLongInt BufferSize, SolFSLongWord *Written);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageReadFileWithSeek(SolFSHandle File,
	SolFSLongLongInt FileOffset, SolFSPointer Buffer, SolFSLongInt BufferSize,
	SolFSLongWord *Read);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageWriteFileWithSeek(SolFSHandle File,
	SolFSLongLongInt FileOffset, SolFSPointer Buffer, SolFSLongInt BufferSize,
	SolFSLongWord *Written);

// Queries related functions

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAddTagName(SolFSHandle Storage,
	SolFSWord TagID, SolFSWord TagValueType, PSolFSWideChar TagName);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteTagName(SolFSHandle Storage,
	SolFSWord TagID);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetTagNamesCount(SolFSHandle Storage,
	SolFSLongWord *Count);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetTagName(SolFSHandle Storage,
	SolFSLongInt Index, SolFSWord *TagID, SolFSWord *TagValueType, PSolFSWideChar TagNameBuffer,
	SolFSLongWord *TagNameBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTagAsBool(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSBool Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagAsBool(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSBool *Value);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTagAsString(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, PSolFSWideChar Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagAsString(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, PSolFSWideChar ValueBuffer,
	SolFSLongWord *ValueBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTagAsAnsiString(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, PSolFSChar Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagAsAnsiString(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, PSolFSChar ValueBuffer,
	SolFSLongWord *ValueBufferSize);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTagAsDateTime(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSDateTime Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagAsDateTime(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSDateTime *Value);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetFileTagAsNumber(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSLongLongInt Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetFileTagAsNumber(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName, SolFSWord TagID, SolFSLongLongInt *Value);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDeleteFileTagByName(SolFSHandle Storage,
	PSolFSWideChar FileName, PSolFSWideChar TagName);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindByQueryFirst(SolFSHandle Storage,
	PSolFSWideChar Directory, PStorageSearch Search, PSolFSWideChar Query, SolFSLongWord Flags);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindByQueryNext(SolFSHandle Storage,
	PStorageSearch Search);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFindByQueryClose(SolFSHandle Storage,
	PStorageSearch Search);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageAnsiToWideChar(PSolFSChar Src, PSolFSWideChar *Buffer,
  SolFSLongWord *BufferSize, SolFSBool AllocateBuffer);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageWideCharToAnsi(PSolFSWideChar Src, PSolFSChar *Buffer,
  SolFSLongWord *BufferSize, SolFSBool AllocateBuffer);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageUnicodeToWideChar(PSolFSUnicodeChar Src, PSolFSWideChar *Buffer,
  SolFSLongWord *BufferSize, SolFSBool AllocateBuffer);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageWideCharToUnicode(PSolFSWideChar Src, PSolFSUnicodeChar *Buffer,
  SolFSLongWord *BufferSize, SolFSBool AllocateBuffer);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageFreeBuffer(SolFSPointer Buffer);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageDecodeDateTime(SolFSDateTime Value,
    SolFSWord *Year, SolFSWord *Month, SolFSWord *Day,
    SolFSWord *Hour, SolFSWord *Min, SolFSWord *Sec, SolFSWord *MSec);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageEncodeDateTime(
    SolFSLongInt Year, SolFSLongInt Month, SolFSLongInt Day,
	SolFSLongInt Hour, SolFSLongInt Min, SolFSLongInt Sec, SolFSLongInt MSec,
    SolFSDouble *Result);

DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMaxNonPagedNameLength(
  SolFSHandle Storage, PSolFSLongWord Value);
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageSetMaxNonPagedNameLength(
  SolFSLongWord Value);

#if (defined(SOLFS_WINDOWS) && defined(SOLFS_DEBUG) && !defined(SOLFS_WINCE))
DLLEXPORTSPEC SolFSError CALLCONVSPEC StorageGetMemUsage(
	SolFSLongWord **MemoryAllocated, SolFSLongWord **MemoryAllocatedCount,
	SolFSLongWord **MaxMemoryAllocated, SolFSLongWord **MaxMemoryAllocatedCount);
#endif

#if defined(__cplusplus)
}
#endif

#if defined(SOLFS_DEBUG) && defined(SOLFS_CRASH)

#define returnIOResult return IOResult
#define initIOResult

//void CheckIOResult(SolFSError *IOResult);
//#define returnIOResult CheckIOResult(&IOResult); return IOResult
//void InitIOResult(SolFSError *IOResult);
//#define initIOResult InitIOResult(&IOResult); if (IOResult != 0) return IOResult

#else
#define returnIOResult return IOResult
#define initIOResult
#endif // defined(SOLFS_DEBUG) && defined(SOLFS_CRASH)

#endif

#ifdef SOLFS_UNIX
#	ifndef SOLFS_ANDROID
#		define SOLFS_HAS_ICONV
#	endif //SOLFS_UNIX
#endif

#ifdef SOLFS_HAS_ICONV
typedef struct IConvStruct {
  iconv_t FAnsiToWide;
  iconv_t FWideToAnsi;
  iconv_t FUnicodeToWide;
  iconv_t FWideToUnicode;
  SolFSHandle FLock;
} TIConv, *PIConv;

extern TIConv GIConv;

void IConvInit();
void IConvDone();

SolFSError IConvConvert(iconv_t conv, char *in, size_t in_size,
  int mul, char **out, size_t *out_size);

#endif

#endif //!defined(STORE_DECL_H)


