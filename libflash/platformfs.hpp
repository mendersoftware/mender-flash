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

#ifndef PLATFORMFS_H
#define PLATFORMFS_H

#include <string>
#include <common/error.hpp>
#include <common/expected.hpp>

using namespace std;
namespace expected = mender::common::expected;
using mender::common::error::Error;
using mender::common::error::NoError;
using ExpectedSize = expected::expected<size_t, Error>;
using ExpectedString = expected::expected<std::string, Error>;
using ExpectedBool = expected::expected<bool, Error>;

namespace mender {
namespace io {


using File = int;
using ExpectedFile = expected::expected<File, Error>;
using Bytes = std::vector<uint8_t>;

const int DEFAULT_FILE_PERMISSION = 0644;

string BaseName(const string &path);

///
/// \brief Create
/// \param p: path
/// \return
///
Error Create(const std::string &p, int filePermission = DEFAULT_FILE_PERMISSION);

///
/// \brief Open
/// \param p: path
/// \param read: open for reading
/// \param write: open for writing
/// \return
///
ExpectedFile Open(const std::string &p, bool read = true, bool write = false);

///
/// \brief Close
/// \param f
/// \return
///
Error Close(File f);

///
/// \brief GetSize
/// \param f: file
/// \return size of the file
///
ExpectedSize GetSize(File f);

///
/// \brief Read
/// \param f: file
/// \param dataPtr: pointer to store read data
/// \param dataLen: length of data to read
/// \return number of bytes read or error on failure
///
ExpectedSize Read(File f, uint8_t *dataPtr, size_t dataLen);

///
/// \brief Write
/// \param f: file
/// \param dataPtr: pointer to data
/// \param dataLen: length of data to write
/// \return number of bytes written or error on failure
///
ExpectedSize Write(File f, const uint8_t *dataPtr, size_t dataLen);

///
/// \brief Flush: flushes written data
/// \param f: file
/// \return
///
Error Flush(File f);

///
/// \brief SeekSet
/// \param f: file
/// \param pos: position to be set on the file
/// \return
///
Error SeekSet(File f, uint64_t pos);

///
/// \brief Tell
/// \param f: file
/// \return current position of the file seek
///
ExpectedSize Tell(File f);

///
/// \brief GetInputStream
/// \return SDTIN
///
File GetInputStream();

///
/// \brief GetInputStream
/// \return invalid file
///
File GetInvalidFile();

///
/// \brief IsSpecialBlockDevice
/// \param f: file
/// \return true if the file descriptor is special block device
///
ExpectedBool IsSpecialBlockDevice(File f);

///
/// \brief IsUBIDevice
/// \param path
/// \return: true if path points to an UBI volume
///
ExpectedBool IsUBIDevice(const std::string &path);

///
/// \brief SetUbiUpdateVolume
/// \param f
/// \param size
/// \return
///
Error SetUbiUpdateVolume(File f, size_t size);

///
/// \brief WriteFile: opens a file (creates if doesn't exist), writes the data and closes the file
/// \param path: path to the file
/// \param data: data that's will be written to the file
/// \return bytes written on an error
///
ExpectedSize WriteFile(const string &path, const Bytes &data);

} // namespace io
} // namespace mender

#endif // PLATFORMFS_H
