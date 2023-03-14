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

#include <fileio.hpp>
#include <sstream>

mender::io::FileReader::FileReader(mender::io::File f) :
	fd_(f) {
}

mender::io::FileReader::~FileReader() {
	if (fd_ != mender::io::GetInvalidFile()) {
		mender::io::Close(fd_);
	}
}

ExpectedSize mender::io::FileReader::Read(
	vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) {
	return mender::io::Read(fd_, &*start, end - start);
}

ExpectedSize mender::io::FileReader::Tell() const {
	return mender::io::Tell(fd_);
}

mender::io::InputStreamReader::InputStreamReader() :
	FileReader(GetInputStream()) {
}

ExpectedSize mender::io::InputStreamReader::Read(
	vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) {
	auto res = FileReader::Read(start, end);
	if (!res) {
		return res;
	}
	mReadBytes += res.value();
	return res;
}

ExpectedSize mender::io::InputStreamReader::Tell() const {
	return mReadBytes;
}

mender::io::LimitedFlushingWriter::LimitedFlushingWriter(
	mender::io::File f, size_t limit, uint32_t flushInterval) :
	FileWriter(f),
	mWritingLimit(limit),
	mFlushIntervalBytes(flushInterval) {
}

ExpectedSize mender::io::LimitedFlushingWriter::Write(
	vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) {
	auto pos = mender::io::Tell(fd_);
	if (!pos) {
		return pos.error();
	}
	auto dataLen = end - start;
	if (mWritingLimit && pos.value() + dataLen > mWritingLimit) {
		std::stringstream ss;
		ss << "Error writing beyound the limit of " << mWritingLimit << " bytes";
		return Error(std::error_condition(std::errc::io_error), ss.str());
	}
	auto res = FileWriter::Write(start, end);
	if (res) {
		mUnflushedBytesWritten += res.value();
		if (mUnflushedBytesWritten >= mFlushIntervalBytes) {
			auto flushRes = mender::io::Flush(fd_);
			if (NoError != flushRes) {
				return Error(std::error_condition(std::errc::io_error), flushRes.message);
			} else {
				mUnflushedBytesWritten -= mFlushIntervalBytes;
			}
		}
	}
	return res;
}

mender::io::FileWriter::FileWriter(File f) :
	fd_(f) {
}

mender::io::FileWriter::~FileWriter() {
	if (fd_ != mender::io::GetInvalidFile()) {
		mender::io::Close(fd_);
	}
}

ExpectedSize mender::io::FileWriter::Write(
	vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) {
	return mender::io::Write(fd_, &*start, end - start);
}

mender::io::FileReadWriter::FileReadWriter(File f) :
	fd_(f) {
}

mender::io::FileReadWriter::~FileReadWriter() {
	if (fd_ != mender::io::GetInvalidFile()) {
		mender::io::Close(fd_);
	}
}

ExpectedSize mender::io::FileReadWriter::Read(
	vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) {
	return mender::io::Read(fd_, &*start, end - start);
}

ExpectedSize mender::io::FileReadWriter::Write(
	vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) {
	return mender::io::Write(fd_, &*start, end - start);
}

mender::io::FileReadWriterSeeker::FileReadWriterSeeker(FileWriter &writer) :
	FileReadWriter(writer.fd_),
	writer_(writer) {
}

ExpectedSize mender::io::FileReadWriterSeeker::Write(
	vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) {
	return writer_.Write(start, end);
}

Error mender::io::FileReadWriterSeeker::SeekSet(uint64_t pos) {
	return mender::io::SeekSet(fd_, pos);
}

ExpectedSize mender::io::FileReadWriterSeeker::Tell() const {
	return mender::io::Tell(fd_);
}
