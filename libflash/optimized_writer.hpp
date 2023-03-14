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

#pragma once

#include <fileio.hpp>

namespace mender {

class OptimizedWriter {
public:
	OptimizedWriter(
		io::FileReader &reader,
		io::FileReadWriterSeeker &writer,
		size_t blockSize = 1024 * 1024,
		size_t volumeSize = 0);
	common::error::Error Copy(bool optimized);

	void PrintStatistics() const;

	struct Statistics {
		uint32_t blocksWritten_ {0};
		uint32_t blocksOmitted_ {0};
		size_t bytesWritten_ {0};
	};

	const Statistics &GetStatistics() const {
		return statistics_;
	}

private:
	size_t blockSize_;
	io::FileReader &reader_;
	io::FileReadWriterSeeker &readWriter_;
	size_t volumeSize_ {0};

	Statistics statistics_;
};

} // namespace mender
