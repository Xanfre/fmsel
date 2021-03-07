#ifndef __LIB_7ZIP_H__
#define __LIB_7ZIP_H__

#define LIB_7ZIP_VER_MAJOR 1
#define LIB_7ZIP_VER_MINOR 6
#define LIB_7ZIP_VER_BUILD 2
#define LIB_7ZIP_VERSION "1.62"
#define LIB_7ZIP_7ZIP_VERSION "lib7Zip 1.62"
#define LIB_7ZIP_DATE "2012-07"
#define LIB_7ZIP_COPYRIGHT "Copyright (c) 2009-2012"
#define LIB_7ZIP_VERSION_COPYRIGHT_DATE MY_VERSION "  " MY_COPYRIGHT "  " MY_DATE

#include <string>
#include <vector>

#ifndef _WIN32
#define __int64 long long int
typedef std::basic_string<wchar_t> wstring;
typedef std::basic_string<char> string;
#ifndef CLASS_E_CLASSNOTAVAILABLE
#define CLASS_E_CLASSNOTAVAILABLE (0x80040111L)
#endif
#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2
#define S_OK 				 0
#else
typedef std::basic_string<wchar_t> wstring;
typedef std::basic_string<char> string;
#endif

typedef std::vector<wstring> WStringArray;

class C7ZipObject
{
public:
	C7ZipObject() {}
	virtual ~C7ZipObject() {}
};

class C7ZipObjectPtrArray : public std::vector<C7ZipObject *>
{
public:
	C7ZipObjectPtrArray(bool auto_release = true);
	virtual ~C7ZipObjectPtrArray();

public:
	void clear();

private:
    bool m_bAutoRelease;
};

namespace lib7zip {
	enum PropertyIndexEnum {
		PROP_INDEX_BEGIN,

		kpidPackSize = PROP_INDEX_BEGIN, //(Packed Size)
		kpidAttrib, //(Attributes)
		kpidCTime, //(Created)
		kpidATime, //(Accessed)
		kpidMTime, //(Modified)
		kpidSolid, //(Solid)
		kpidEncrypted, //(Encrypted)
		kpidUser, //(User)
		kpidGroup, //(Group)
		kpidComment, //(Comment)
		kpidPhySize, //(Physical Size)
		kpidHeadersSize, //(Headers Size)
		kpidChecksum, //(Checksum)
		kpidCharacts, //(Characteristics)
		kpidCreatorApp, //(Creator Application)
		kpidTotalSize, //(Total Size)
		kpidFreeSpace, //(Free Space)
		kpidClusterSize, //(Cluster Size)
		kpidVolumeName, //(Label)
		kpidPath, //(FullPath)
		kpidIsDir, //(IsDir)
		kpidSize, //(Uncompressed Size)

		PROP_INDEX_END
	};

	// custom modification of libzip 1.6.2
	enum ExtractErrorEnum {
		EXTRACT_ERROR_BEGIN,

		kxerrNone = EXTRACT_ERROR_BEGIN, //(no error)
		kxerrUnSupportedMethod	= 1<<0, //(Unsupported Method)
		kxerrDataError			= 1<<1, //(Data Error)
		kxerrCRCError			= 1<<2, //(Checksum Error)
		kxerrBadIndex			= 1<<3, //(Bad Item Index)
		kxerrUnknown			= 1<<4, //(Unknown Error)

		EXTRACT_ERROR_END
	};
};

class C7ZipArchiveItem : public virtual C7ZipObject
{
public:
	C7ZipArchiveItem();
	virtual ~C7ZipArchiveItem();

public:
	virtual wstring GetFullPath() const  = 0;
	virtual unsigned __int64 GetSize() const = 0;
	virtual bool IsDir() const  = 0;
	virtual bool IsEncrypted() const  = 0;

	virtual unsigned int GetArchiveIndex() const  = 0;

	virtual bool GetUInt64Property(lib7zip::PropertyIndexEnum propertyIndex,
											   unsigned __int64 & val) const = 0;
	virtual bool GetBoolProperty(lib7zip::PropertyIndexEnum propertyIndex,
								 bool & val) const = 0;
	virtual bool GetStringProperty(lib7zip::PropertyIndexEnum propertyIndex,
									  wstring & val) const = 0;
	virtual bool GetFileTimeProperty(lib7zip::PropertyIndexEnum propertyIndex,
									 unsigned __int64 & val) const = 0;
	virtual wstring GetArchiveItemPassword() const  = 0;
	virtual void SetArchiveItemPassword(const wstring & password) = 0;
	virtual bool IsPasswordSet() const = 0;
};

class C7ZipInStream
{
public:
	virtual wstring GetExt() const = 0;
	virtual int Read(void *data, unsigned int size, unsigned int *processedSize) = 0;
	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) = 0;
	virtual int GetSize(unsigned __int64 * size) = 0;
};

class C7ZipMultiVolumes
{
public:
	virtual wstring GetFirstVolumeName() = 0;
	virtual bool MoveToVolume(const wstring & volumeName) = 0;
	virtual unsigned __int64 GetCurrentVolumeSize() = 0;
	virtual C7ZipInStream * OpenCurrentVolumeStream() = 0;
};

class C7ZipOutStream
{
public:
	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize) = 0;
	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition) = 0;
	virtual int SetSize(unsigned __int64 size) = 0;
};

// custom modification of libzip 1.6.2
class C7ZipOutStreamFactory
{
public:
	virtual C7ZipOutStream* GetStream(C7ZipArchiveItem * pItem) = 0;
	virtual void CloseStream(C7ZipOutStream * pStream) = 0;
};

class C7ZipArchive : public virtual C7ZipObject
{
public:
	C7ZipArchive();
	virtual ~C7ZipArchive();

public:
	virtual bool GetItemCount(unsigned int * pNumItems) = 0;
	virtual bool GetItemInfo(unsigned int index, C7ZipArchiveItem ** ppArchiveItem) = 0;
	virtual bool Extract(unsigned int index, C7ZipOutStream * pOutStream) = 0;
	virtual bool Extract(unsigned int index, C7ZipOutStream * pOutStream, const wstring & pwd) = 0;
	virtual bool Extract(const C7ZipArchiveItem * pArchiveItem, C7ZipOutStream * pOutStream) = 0;
	virtual wstring GetArchivePassword() const  = 0;
	virtual void SetArchivePassword(const wstring & password) = 0;
	virtual bool IsPasswordSet() const = 0;
	
	virtual void Close() = 0;

	virtual bool GetUInt64Property(lib7zip::PropertyIndexEnum propertyIndex,
											   unsigned __int64 & val) const = 0;
	virtual bool GetBoolProperty(lib7zip::PropertyIndexEnum propertyIndex,
								 bool & val) const = 0;
	virtual bool GetStringProperty(lib7zip::PropertyIndexEnum propertyIndex,
									  wstring & val) const = 0;
	virtual bool GetFileTimeProperty(lib7zip::PropertyIndexEnum propertyIndex,
									 unsigned __int64 & val) const = 0;

	// custom modification of libzip 1.6.2
	virtual bool ExtractAll(C7ZipOutStreamFactory * pOutStreamFactory) = 0;
	virtual lib7zip::ExtractErrorEnum GetExtractError() = 0;
};

class C7ZipLibrary
{
public:
	C7ZipLibrary();
	~C7ZipLibrary();

private:
	bool m_bInitialized;

	C7ZipObjectPtrArray m_InternalObjectsArray;

public:
	bool Initialize();
	void Deinitialize();

	bool GetSupportedExts(WStringArray & exts);

	bool OpenArchive(C7ZipInStream * pInStream, C7ZipArchive ** ppArchive);
	bool OpenArchive(C7ZipInStream * pInStream, C7ZipArchive ** ppArchive, const wstring & pwd);
	bool OpenMultiVolumeArchive(C7ZipMultiVolumes * pVolumes, C7ZipArchive ** ppArchive);
	bool OpenMultiVolumeArchive(C7ZipMultiVolumes * pVolumes, C7ZipArchive ** ppArchive, const wstring & pwd);

    const C7ZipObjectPtrArray & GetInternalObjectsArray() { return m_InternalObjectsArray; }

    bool IsInitialized() const { return m_bInitialized; }
};

#endif
