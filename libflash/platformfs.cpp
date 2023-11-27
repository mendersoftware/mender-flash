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
#include <cstring>

#include <sys/sysmacros.h>
#include <mtd/ubi-user.h>

static const int InvalidFileDescriptor = -1;
static const int UBIMajorDevNo = 250;

static Error MakeErrorFromErrno(int err, std::stringstream &str) {
	str << ": " << strerror(err);
	return mender::common::error::Error(
		std::error_code(err, std::generic_category()).default_error_condition(), str.str());
}

Error mender::io::Create(const string &p, int filePermission) {
	errno = 0;
	mender::io::File fd = open(p.c_str(), O_WRONLY | O_CREAT, filePermission);

	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to open file: " << p;
		return MakeErrorFromErrno(errno, ss);
	}
	return Close(fd);
}

mender::io::ExpectedFile mender::io::Open(const string &p, bool read, bool write) {
	if (!read && !write) {
		return expected::unexpected(Error(
			std::error_condition(std::errc::invalid_argument), "Wrong access flags provided"));
	}
	int flags = (read && write) ? O_RDWR : write ? O_WRONLY : O_RDONLY;

	errno = 0;
	mender::io::File fd = open(p.c_str(), flags);

	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to open file: " << p;
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
	}
	return fd;
}

Error mender::io::Close(File f) {
	errno = 0;
	close(f);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Failed to close the file";
		return MakeErrorFromErrno(errno, ss);
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
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
	}

	size_t size;
	if (S_ISBLK(statbuf.st_mode)) {
		ioctl(f, BLKGETSIZE64, &size);
		if (errno != 0) {
			std::stringstream ss;
			ss << "Failed to get file size";
			return expected::unexpected(MakeErrorFromErrno(errno, ss));
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
		if (errno == EINTR) {
			continue;
		}
		if (errno != 0) {
			std::stringstream ss;
			ss << "Error while reading data";
			return expected::unexpected(MakeErrorFromErrno(errno, ss));
		}
		bytesRead += readRes;
		if (readRes == 0 || bytesRead == dataLen) {
			break;
		}
	}
	return bytesRead;
}

ExpectedSize mender::io::Write(File f, const uint8_t *dataPtr, size_t dataLen) {
	ssize_t bytesWritten = 0;
	while (true) {
		errno = 0;
		bytesWritten = write(f, dataPtr, dataLen);
		if (errno == EINTR) {
			continue;
		}
		if (errno != 0) {
			std::stringstream ss;
			ss << "Error while writing data";
			return expected::unexpected(MakeErrorFromErrno(errno, ss));
		}
		break;
	}
	return bytesWritten;
}

Error mender::io::Flush(File f) {
	errno = 0;
	fsync(f);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error while flushing data";
		return MakeErrorFromErrno(errno, ss);
	}
	return NoError;
}

Error mender::io::SeekSet(mender::io::File f, uint64_t pos) {
	errno = 0;
	lseek64(f, pos, SEEK_SET);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Can't set seek on the file";
		return MakeErrorFromErrno(errno, ss);
	}
	return NoError;
}

ExpectedSize mender::io::Tell(mender::io::File f) {
	errno = 0;
	ssize_t pos = lseek64(f, 0, SEEK_CUR);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error while getting file position";
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
	}
	return pos;
}

mender::io::File mender::io::GetInputStream() {
	return STDIN_FILENO;
}

mender::io::File mender::io::GetInvalidFile() {
	return InvalidFileDescriptor;
}

ExpectedBool mender::io::IsSpecialBlockDevice(File f) {
	errno = 0;
	struct stat statbuf;
	fstat(f, &statbuf);
	if (errno != 0) {
		std::stringstream ss;
		ss << "IsSpecialBlockDevice: Failed to obtain stats of the file";
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
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
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
	}
	ssize_t bytesWritten = write(fd, data.data(), data.size());
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error writing data: " << path;
		auto err = expected::unexpected(MakeErrorFromErrno(errno, ss));
		Close(fd);
		return err;
	} else {
		Close(fd);
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
		return expected::unexpected(MakeErrorFromErrno(errno, ss));
	}

	return S_ISCHR(statbuf.st_mode) && major(statbuf.st_rdev) == UBIMajorDevNo;
}

Error mender::io::SetUbiUpdateVolume(File f, size_t size) {
	errno = 0;
	// Argument is 64-bit, even on 32-bit platforms.
	int64_t size64 {static_cast<int64_t>(size)};
	ioctl(f, UBI_IOCVOLUP, &size64);
	if (errno != 0) {
		std::stringstream ss;
		ss << "Error updating UBI volume";
		return MakeErrorFromErrno(errno, ss);
	}
	return NoError;
}
