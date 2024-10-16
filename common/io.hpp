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

#ifndef MENDER_COMMON_IO_HPP
#define MENDER_COMMON_IO_HPP

#include <common/error.hpp>
#include <common/expected.hpp>

#include <memory>
#include <system_error>
#include <vector>
#include <istream>
#include <sstream>
#include <string>

namespace mender {
namespace common {
namespace io {

using namespace std;
namespace expected = mender::common::expected;

using mender::common::error::Error;
using mender::common::error::NoError;

using ExpectedSize = expected::ExpectedSize;
using ExpectedSize64 = expected::ExpectedSize64;

class Reader {
public:
	virtual ~Reader() {};

	virtual ExpectedSize Read(vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) = 0;
};
using ReaderPtr = shared_ptr<Reader>;
using ExpectedReaderPtr = expected::expected<ReaderPtr, Error>;

class Writer {
public:
	virtual ~Writer() {};

	virtual ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) = 0;
};
using WriterPtr = shared_ptr<Writer>;
using ExpectedWriterPtr = expected::expected<WriterPtr, Error>;

class ReadWriter : virtual public Reader, virtual public Writer {};
using ReadWriterPtr = shared_ptr<ReadWriter>;
using ExpectedReadWriterPtr = expected::expected<ReadWriterPtr, Error>;

/**
 * Stream the data from `src` to `dst` until encountering EOF or an error.
 */
Error Copy(Writer &dst, Reader &src);

/**
 * Stream the data from `src` to `dst` until encountering EOF or an error, using `buffer` as an
 * intermediate. The block size will be the size of `buffer`.
 */
Error Copy(Writer &dst, Reader &src, vector<uint8_t> &buffer);

class StreamReader : virtual public Reader {
private:
	std::istream &is_;

public:
	StreamReader(std::istream &stream) :
		is_ {stream} {
	}
	StreamReader(std::istream &&stream) :
		is_ {stream} {
	}
	ExpectedSize Read(vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override {
		is_.read(reinterpret_cast<char *>(&*start), end - start);
		if (is_.bad()) {
			int int_error = errno;
			return expected::unexpected(Error(
				std::error_code(int_error, std::generic_category()).default_error_condition(), ""));
		}
		return is_.gcount();
	}
};

/* Discards all data written to it */
class Discard : virtual public Writer {
	ExpectedSize Write(
		vector<uint8_t>::const_iterator start, vector<uint8_t>::const_iterator end) override {
		return end - start;
	}
};

class StringReader : virtual public Reader {
private:
	std::stringstream s_;
	StreamReader reader_;

public:
	StringReader(string &str) :
		s_ {str},
		reader_ {s_} {
	}
	StringReader(string &&str) :
		s_ {str},
		reader_ {s_} {
	}

	ExpectedSize Read(vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override {
		return reader_.Read(start, end);
	}
};

} // namespace io
} // namespace common
} // namespace mender

#endif // MENDER_COMMON_IO_HPP
