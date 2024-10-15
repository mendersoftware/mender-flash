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

#ifndef FILEIO_HPP
#define FILEIO_HPP
#include <common/io.hpp>
#include <platformfs.hpp>

namespace mender {
namespace io {

class FileWriter : public common::io::Writer {
public:
	FileWriter(File f);
	virtual ~FileWriter();
	virtual ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) override;
	///
	/// \brief GetFile
	/// \return Takes ownership of the file
	///
	File GetFile() {
		File ret = fd_;
		fd_ = mender::io::GetInvalidFile();
		return ret;
	}

protected:
	File fd_;

	friend class FileReadWriterSeeker;
};

class LimitedFlushingWriter : public FileWriter {
public:
	LimitedFlushingWriter(File f, int64_t limit, ssize_t flushInterval = 1);
	virtual ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) override;

protected:
	int64_t writingLimit_ {0};
	ssize_t flushIntervalBytes_;
	int64_t unflushedBytesWritten_ {0};
};

class FileReader : public common::io::Reader {
public:
	FileReader(File fd);
	virtual ~FileReader();
	virtual ExpectedSize Read(
		vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override;
	virtual ExpectedSize64 Tell() const;

	///
	/// \brief GetFile
	/// \return Takes ownership of the file
	///
	File GetFile() {
		File ret = fd_;
		fd_ = mender::io::GetInvalidFile();
		return ret;
	}

protected:
	File fd_;
};

class InputStreamReader : public FileReader {
public:
	InputStreamReader();
	virtual ExpectedSize Read(
		vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override;
	virtual ExpectedSize64 Tell() const override;

protected:
	int64_t readBytes_;
};

class FileReadWriter : public common::io::ReadWriter {
public:
	FileReadWriter(File f);
	virtual ~FileReadWriter();
	virtual ExpectedSize Read(
		vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override;
	virtual ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) override;
	///
	/// \brief GetFile
	/// \return Takes ownership of the file
	///
	File GetFile() {
		File ret = fd_;
		fd_ = mender::io::GetInvalidFile();
		return ret;
	}

protected:
	File fd_;
};

class FileReadWriterSeeker : public FileReadWriter {
public:
	FileReadWriterSeeker(FileWriter &writer);

	virtual ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) override;
	Error SeekSet(uint64_t pos);
	virtual ExpectedSize64 Tell() const;

protected:
	FileWriter &writer_;
};

} // namespace io
} // namespace mender
#endif // FILEIO_H
