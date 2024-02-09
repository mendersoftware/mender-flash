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

#include <gtest/gtest.h>
#include <filesystem>
#include <libflash/platformfs.hpp>
#include <testing.hpp>


class PlatformFsTest : public testing::Test {};


TEST_F(PlatformFsTest, OpenFile) {
	mender::common::testing::TemporaryDirectory tempDir;

	// fail to open file - non-existing
	auto res = mender::io::Open(tempDir.Path() + "non-existing-file", true, false);
	ASSERT_FALSE(res);

	// create a file
	auto testFileName = tempDir.Path() + "test_file";
	mender::io::Bytes vec = {'f', 'o', 'o', 'b', 'a', 'r'};
	auto size = mender::io::WriteFile(testFileName, vec);
	ASSERT_TRUE(size);
	ASSERT_EQ(size.value(), vec.size());

	// fail to open file - wrong flags
	auto testFileFail = mender::io::Open(testFileName, false, false);
	ASSERT_FALSE(testFileFail);

	auto testFile = mender::io::Open(testFileName, true, false);
	ASSERT_TRUE(testFile);

	mender::io::File testFd = testFile.value();

	auto blockRes = mender::io::IsSpecialBlockDevice(testFd);
	ASSERT_TRUE(blockRes);
	ASSERT_FALSE(blockRes.value());

	auto ubiRes = mender::io::IsUBIDevice(testFileName);
	ASSERT_TRUE(ubiRes);
	ASSERT_FALSE(ubiRes.value());

	auto fileSize = mender::io::GetSize(testFd);
	ASSERT_TRUE(fileSize);
	ASSERT_EQ(fileSize.value(), vec.size());

	auto tellRes = mender::io::Tell(testFd);
	ASSERT_TRUE(tellRes);
	ASSERT_EQ(tellRes.value(), 0);

	auto ubiUptadeRes = mender::io::SetUbiUpdateVolume(testFd, 10);
	ASSERT_NE(ubiUptadeRes, NoError);

	auto closeRes = mender::io::Close(testFile.value());
	ASSERT_EQ(NoError, closeRes);
}
