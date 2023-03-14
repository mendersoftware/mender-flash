// Copyright 2023 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include <platformfs.hpp>

#include <unistd.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <sstream>

#include <sys/sysmacros.h>
#include <mtd/ubi-user.h>

static Error MakeErrorFromErrno(std::stringstream &str) {
	int int_error = errno;
	str << " errno: " << errno;
	return mender::common::error::Error(
		std::error_code(int_error, std::system_category()).default_error_condition(), str.str());
}

Error mender::io::Create(const string &p) {
	errno = 0;
	mender::io::File fd = open(p.c_str(), O_WRONLY | O_CREAT, 0644);

	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to open file: " << p;
		return MakeErrorFromErrno(ss);
	}
	return Close(fd);
}

mender::io::ExpectedFile mender::io::Open(const string &p, bool read, bool write) {
	if (!read && !write) {
		return Error(
			std::error_condition(std::errc::invalid_argument), "Wrong access flags provided");
	}
	int flags = (read && write) ? O_RDWR : write ? O_WRONLY : O_RDONLY;

	errno = 0;
	mender::io::File fd = open(p.c_str(), flags);

	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to open file: " << p;
		return MakeErrorFromErrno(ss);
	}
	return fd;
}

Error mender::io::Close(File f) {
	errno = 0;
	close(f);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to close the file";
		return MakeErrorFromErrno(ss);
	}
	return NoError;
}

ExpectedSize mender::io::GetSize(mender::io::File f) {
	struct stat statbuf;
	errno = 0;
	fstat(f, &statbuf);
	if (errno != 0) {
		std::stringstream ss;
		ss << "GetSize: Failed to obtain stats of the file";
		return MakeErrorFromErrno(ss);
	}

	size_t size;
	if (S_ISBLK(statbuf.st_mode)) {
		ioctl(f, BLKGETSIZE64, &size);
		if (errno != 0) {
			std::stringstream ss;
			ss << "Failed to get file size";
			return MakeErrorFromErrno(ss);
		}
	} else {
		size = statbuf.st_size;
	}
	return size;
}

ExpectedSize mender::io::Read(File f, uint8_t *dataPtr, size_t dataLen) {
	size_t bytesRead = 0;
	while (true) {
		errno = 0;
		ssize_t readRes = read(f, dataPtr + bytesRead, dataLen - bytesRead);
		if (errno != 0) {
			std::stringstream ss;
			ss << "Error while reading data";
			return MakeErrorFromErrno(ss);
		}
		bytesRead += readRes;
		if (readRes == 0 || bytesRead == dataLen) {
			break;
		}
	}
	return bytesRead;
}

ExpectedSize mender::io::Write(File f, const uint8_t *dataPtr, size_t dataLen) {
	errno = 0;
	ssize_t bytesWritten = write(f, dataPtr, dataLen);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error while writing data";
		return MakeErrorFromErrno(ss);
	}
	return bytesWritten;
}

Error mender::io::Flush(File f) {
	errno = 0;
	fsync(f);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error while flushing data";
		return MakeErrorFromErrno(ss);
	}
	return NoError;
}

Error mender::io::SeekSet(mender::io::File f, uint64_t pos) {
	errno = 0;
	lseek64(f, pos, SEEK_SET);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Can't set seek on the file";
		return MakeErrorFromErrno(ss);
	}
	return NoError;
}

ExpectedSize mender::io::Tell(mender::io::File f) {
	errno = 0;
	ssize_t pos = lseek64(f, 0, SEEK_CUR);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error while getting file position";
		return MakeErrorFromErrno(ss);
	}
	return pos;
}

mender::io::File mender::io::GetInputStream() {
	return STDIN_FILENO;
}

mender::io::File mender::io::GetInvalidFile() {
	return -1;
}

ExpectedBool mender::io::IsSpecialBlockDevice(File f) {
	errno = 0;
	struct stat statbuf;
	fstat(f, &statbuf);
	if (errno != 0) {
		std::stringstream ss;
		ss << "IsSpecialBlockDevice: Failed to obtain stats of the file";
		return MakeErrorFromErrno(ss);
	} else if (S_ISBLK(statbuf.st_mode)) {
		return true;
	}
	return false;
}

ExpectedSize mender::io::WriteFile(const string &path, const Bytes &data) {
	errno = 0;
	mender::io::File fd = open(path.c_str(), O_WRONLY | O_CREAT, 0644);

	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to open file: " << path;
		return MakeErrorFromErrno(ss);
	}
	ssize_t bytesWritten = write(fd, data.data(), data.size());
	Close(fd);
	if (bytesWritten <= 0) {
		return Error(std::error_condition(std::errc::io_error), "Error writing data");
	} else {
		return bytesWritten;
	}
}

ExpectedBool mender::io::IsUBIDevice(const string &path) {
	errno = 0;
	struct stat statbuf;
	stat(path.c_str(), &statbuf);
	if (errno != 0) {
		std::stringstream ss;
		ss << "IsSpecialBlockDevice: Failed to obtain stats of the file";
		return MakeErrorFromErrno(ss);
	}

	return S_ISBLK(statbuf.st_mode) && major(statbuf.st_rdev) == 10;
}

Error mender::io::SetUbiUpdateVolume(File f, size_t size) {
	errno = 0;
	ioctl(f, UBI_IOCVOLUP, &size);
	if (errno != 0) {
		return Error(std::error_condition(std::errc::io_error), "Error updating UBI volume");
	}
	return NoError;
}
