/*

Copyright (c) 2003-2018, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_FILE_HPP_INCLUDED
#define TORRENT_FILE_HPP_INCLUDED

#include <memory>
#include <string>
#include <functional>

#include "libtorrent/config.hpp"
#include "libtorrent/string_view.hpp"
#include "libtorrent/span.hpp"
#include "libtorrent/aux_/storage_utils.hpp"
#include "libtorrent/flags.hpp"

#include "libtorrent/aux_/disable_warnings_push.hpp"

#include <boost/noncopyable.hpp>

#ifdef TORRENT_WINDOWS
// windows part
#include "libtorrent/aux_/windows.hpp"
#include <winioctl.h>
#include <sys/types.h>
#else
// posix part
#define _FILE_OFFSET_BITS 64

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <unistd.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h> // for DIR

#undef _FILE_OFFSET_BITS

#endif

#include "libtorrent/aux_/disable_warnings_pop.hpp"

#include "libtorrent/error_code.hpp"
#include "libtorrent/assert.hpp"
#include "libtorrent/time.hpp"

namespace libtorrent {
	typedef void* TFS_HANDLE;

	typedef struct _vGUID
	{
		uint32_t Data1;
		uint16_t Data2;
		uint16_t Data3;
		long long unsigned Data4;
	} VGUID, * PVGUID;

	typedef void tfsGetCheckStatusGen(uint64_t progress, uint64_t total, int code, PVGUID guid1, int dif1, PVGUID guid2, int dif2);
	extern "C"
	{
		typedef struct _StoreDriverGen
		{
			//init
			void (*init)(void);

			//disk
			int(*diskFormat)(void* hd, int isMaster, PVGUID outDiskName); //设置新主控时，会自动清除已存在的主控
			int  (*openDisk)(void* hd, PVGUID outDiskName);  //hd为系统文件句柄 
			int  (*closeDisk)(PVGUID diskName);  //关闭磁盘 
			int (*isMaster)(PVGUID diskName);
			int (*uninstall)(PVGUID diskName); //清除磁盘上的tfs系统

			uint64_t(*getDiskPreciseFreeLba)(PVGUID diskName);
			uint64_t(*getDiskFreeLba)(PVGUID diskName);
			uint64_t(*getDiskSizeLba)(PVGUID diskName);
			void* (*getDiskHandle)(PVGUID diskName);
			void* (*getDiskHandleByIdx)(int idx, PVGUID outName);
			int (*getDiskInfoByIdx)(int idx, PVGUID diskName, uint64_t* diskSizeLba, uint64_t* diskFreeLba, int* isMaster, int* isSsd, int* diskStatus);
			int  (*getDiskSum)(void);
			int  (*isSsd)(PVGUID diskName);

			//file
			int (*enumFileByIdx)(uint8_t idx, PVGUID fileName, uint8_t* difLevel, uint64_t* realLba, void* desc, int len);//枚举当前系统的文件返回值 2 结束，1当前文件已删除
			int(*changeFileName)(PVGUID fileName, uint8_t difLevel, PVGUID newName, int newDif); //根据索引号改文件名相关信息NULL, -1 不改变
			int(*setFileDesc)(PVGUID fileName, uint8_t difLevel, const void* desc, int len); //设置文件描敘，最大长度256字节 NULL 清除
			int(*getFileDesc)(PVGUID fileName, uint8_t difLevel, void* desc, int len); //获取文件描敘，最大长度256字节
			void (*setFileFd)(TFS_HANDLE pFile, void* fd);   //设置普通文件句柄，表示此文件为系统内普通文件。

			int (*createTfsFile)(TFS_HANDLE* ppFile, PVGUID diskName, PVGUID fileName, uint8_t difLevel, uint64_t fileSize);
			int (*openTfsFile)(TFS_HANDLE* ppFile, PVGUID fileName, uint8_t difLevel);
			int (*closeTfsFile)(TFS_HANDLE pFile);
			int (*deleteTfsFile)(PVGUID fileName, uint8_t difLevel);
			int (*isTfsFileExists)(PVGUID fileName, uint8_t difLevel);  //文件是否存在

			uint64_t(*getTfsFileSize)(TFS_HANDLE pFile);  //获取文件磁盘占用的大小
			uint64_t(*getTfsFileRealLba)(TFS_HANDLE pFile);  //获取文件真实容量
			int(*resetTfsFileRealLba)(TFS_HANDLE pFile, TFS_HANDLE* ppNewFile, uint64_t newLba); //重新设置文件大小

			//rw
			int (*readTfsFile)(TFS_HANDLE pFile, uint64_t fileOffset, uint8_t* buf, int len);
			int (*writeTfsFile)(TFS_HANDLE pFile, uint64_t fileOffset, uint8_t* buf, int len);
			int (*flushTfsFile)(TFS_HANDLE pFile);
			int(*flushAll)(void);

			//status
			int (*getDiskStatus)(PVGUID diskName);  //获取磁盘异常状态
			void (*clearDiskStatus)(void);  //清除磁盘异常状态标记
			int (*checkDisk)(PVGUID diskName, int restore, tfsGetCheckStatusGen callStatus); //0只检测，1检测后修复
			//error
			char* (*errMsg)(int err);  //根据返回的错误号返回错误详细原因
		}StoreDriverGen;
	}
	extern StoreDriverGen* pStoreDrvGen;


	extern "C"
	{
		typedef void tfsGetCheckStatus(uint64_t progress, uint64_t total, int code, char* fileName1, char* fileName2);
		typedef struct _StoreDriver
		{
			//init
			void (*init)(void);

			//disk
			int(*diskFormat)(void* hd, int isMaster, int isSsd, PVGUID outDiskName); //设置新主控时，会自动清除已存在的主控
			int  (*openDisk)(void* hd, PVGUID outDiskName);  //hd为系统文件句柄 
			int  (*closeDisk)(PVGUID diskName);  //关闭磁盘 
			int (*isMaster)(PVGUID diskName);  //是否为主硬盘
			int (*uninstall)(PVGUID diskName); //清除磁盘上的tfs系统

			uint64_t(*getDiskPreciseFreeLba)(PVGUID diskName);
			uint64_t(*getDiskFreeLba)(PVGUID diskName);
			uint64_t(*getDiskSizeLba)(PVGUID diskName);
			void* (*getDiskHandle)(PVGUID diskName);
			void* (*getDiskHandleByIdx)(int idx, PVGUID outName);
			int (*getDiskInfoByIdx)(int idx, PVGUID diskName, uint64_t* diskSizeLba, uint64_t* diskFreeLba, char* isMaster, char* isSsd, int* diskStatus);
			int  (*getDiskSum)(void);
			int  (*isSsd)(PVGUID diskName);

			//file
			int (*enumFileByIdx)(uint8_t idx, char* fileName, int nameLen, uint64_t* realLba, void* desc, int descLen, PVGUID diskName);//枚举当前系统的文件返回值 2 结束，1当前文件已删除
			int(*changeFileName)(const char* fileName, const char* newName); //根据索引号改文件名相关信息NULL, -1 不改变
			int(*setFileDesc)(const char* fileName, const void* desc, int len); //设置文件描敘，最大长度256字节 NULL 清除
			int(*getFileDesc)(const char* fileName, void* desc, int len); //获取文件描敘，最大长度256字节
			void (*setFileFd)(TFS_HANDLE pFile, void* fd);   //设置普通文件句柄，表示此文件为系统内普通文件。

			int (*createTfsFile)(TFS_HANDLE* ppFile, PVGUID diskName, const char* fileName, uint64_t fileSize);
			int (*openTfsFile)(TFS_HANDLE* ppFile, const char* fileName);
			int (*closeTfsFile)(TFS_HANDLE pFile);
			int (*deleteTfsFile)(const char* fileName);
			int (*isTfsFileExists)(const char* fileName);  //文件是否存在

			uint64_t(*getTfsFileSize)(TFS_HANDLE pFile);  //获取文件磁盘占用的大小
			uint64_t(*getTfsFileRealLba)(TFS_HANDLE pFile);  //获取文件真实容量
			int(*resetTfsFileRealLba)(TFS_HANDLE pFile, TFS_HANDLE* ppNewFile, uint64_t newLba); //重新设置文件大小

			//rw
			int (*readTfsFile)(TFS_HANDLE pFile, uint64_t fileOffset, uint8_t* buf, int len);
			int (*writeTfsFile)(TFS_HANDLE pFile, uint64_t fileOffset, uint8_t* buf, int len);
			int (*flushTfsFile)(TFS_HANDLE pFile);
			int(*flushAll)(void);

			//status
			int (*getDiskStatus)(PVGUID diskName);  //获取磁盘异常状态
			void (*clearDiskStatus)(void);  //清除磁盘异常状态标记
			int (*checkDisk)(PVGUID diskName, int restore, tfsGetCheckStatus callStatus); //0只检测，1检测后修复
			//error
			char* (*errMsg)(int err);  //根据返回的错误号返回错误详细原因
			uint64_t(*getStrategyDisk)(int strategy, int type, PVGUID diskName);
		}StoreDriver;
	}

	extern StoreDriver* pStoreDrv;

	typedef struct VOI_FILE_INFO
	{
		int type; //0:维护 1:win 2:linux -1:error     10:通用版维护 11:通用版win 12:通用版linux
		std::string fileName;
		uint64_t fileSize;
		int dif;
	}*PVOI_FILE_INFO;

	struct  TfsFile
	{
		TfsFile();
		~TfsFile();

		bool open(PVOI_FILE_INFO pInfo, error_code& ec);
		void close();

		std::int64_t writev(std::int64_t file_offset, span<iovec_t const> bufs
			, error_code& ec);
		std::int64_t readv(std::int64_t file_offset, span<iovec_t const> bufs
			, error_code& ec);

		std::int64_t get_size(error_code& ec) const;

		static bool IsVoiFile(const std::string& path) {
			return path.find("-voi-", 0) != std::string::npos;
		}
		void ParseFile(const std::string& path, PVOI_FILE_INFO info);
		bool is_open() const;

	private:
		TFS_HANDLE m_fileHandle;
		std::string m_fileName;
		uint64_t m_fileSize;
		int m_type; //0:edu 1:gen
	};




#ifdef TORRENT_WINDOWS
	using handle_type = HANDLE;
#else
	using handle_type = int;
#endif

	class TORRENT_EXTRA_EXPORT directory : public boost::noncopyable
	{
	public:
		directory(std::string const& path, error_code& ec);
		~directory();
		void next(error_code& ec);
		std::string file() const;
		bool done() const { return m_done; }
	private:
#ifdef TORRENT_WINDOWS
		HANDLE m_handle;
		WIN32_FIND_DATAW m_fd;
#else
		DIR* m_handle;
		std::string m_name;
#endif
		bool m_done;
	};

	struct file;

	using file_handle = std::shared_ptr<file>;

	// hidden
	using open_mode_t = flags::bitfield_flag<std::uint32_t, struct open_mode_tag>;

	// the open mode for files. Used for the file constructor or
	// file::open().
	namespace open_mode {

		// open the file for reading only
		constexpr open_mode_t read_only{};

		// open the file for writing only
		constexpr open_mode_t write_only = 0_bit;

		// open the file for reading and writing
		constexpr open_mode_t read_write = 1_bit;

		// the mask for the bits making up the read-write mode.
		constexpr open_mode_t rw_mask = read_only | write_only | read_write;

		// open the file in sparse mode (if supported by the
		// filesystem).
		constexpr open_mode_t sparse = 2_bit;

		// don't update the access timestamps on the file (if
		// supported by the operating system and filesystem).
		// this generally improves disk performance.
		constexpr open_mode_t no_atime = 3_bit;

		// open the file for random access. This disables read-ahead
		// logic
		constexpr open_mode_t random_access = 4_bit;

		// don't put any pressure on the OS disk cache
		// because of access to this file. We expect our
		// files to be fairly large, and there is already
		// a cache at the bittorrent block level. This
		// may improve overall system performance by
		// leaving running applications in the page cache
		constexpr open_mode_t no_cache = 5_bit;

		// this is only used for readv/writev flags
		constexpr open_mode_t coalesce_buffers = 6_bit;

		// when creating a file, set the hidden attribute (windows only)
		constexpr open_mode_t attribute_hidden = 7_bit;

		// when creating a file, set the executable attribute
		constexpr open_mode_t attribute_executable = 8_bit;

		// the mask of all attribute bits
		constexpr open_mode_t attribute_mask = attribute_hidden | attribute_executable;
	}

	struct TORRENT_EXTRA_EXPORT file : boost::noncopyable
	{
		file();
		file(file&&) noexcept;
		file& operator=(file&&);
		file(std::string const& p, open_mode_t m, error_code& ec);
		~file();

		bool open(std::string const& p, open_mode_t m, error_code& ec);
		bool is_open() const;
		void close();
		bool set_size(std::int64_t size, error_code& ec);

		open_mode_t open_mode() const { return m_open_mode; }

		std::int64_t writev(std::int64_t file_offset, span<iovec_t const> bufs
			, error_code& ec, open_mode_t flags = open_mode_t{});
		std::int64_t readv(std::int64_t file_offset, span<iovec_t const> bufs
			, error_code& ec, open_mode_t flags = open_mode_t{});

		std::int64_t get_size(error_code& ec) const;

		handle_type native_handle() const { return m_file_handle; }

	private:

		handle_type m_file_handle;

		open_mode_t m_open_mode{};

		bool m_is_physical_drive;
		std::int64_t m_fileSize;
		//terry
		bool m_isTfsFile;
		TfsFile m_tfsFile;
	};
}

#endif // TORRENT_FILE_HPP_INCLUDED
