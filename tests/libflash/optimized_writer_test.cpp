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
#include <fileio.hpp>
#include <testing.hpp>
#include <optimized_writer.hpp>

class OptimizedWriterTest : public testing::Test {};

class StringFileReader : public mender::io::FileReader {
public:
	StringFileReader(const std::string &str) :
		mender::io::FileReader(-1),
		ss_(str),
		reader_(ss_) {
	}
	virtual ExpectedSize Tell() const override {
		return bytesRead_;
	}
	virtual ExpectedSize Read(
		vector<uint8_t>::iterator start, vector<uint8_t>::iterator end) override {
		auto ret = reader_.Read(start, end);
		if (ret) {
			bytesRead_ += ret.value();
		}
		return ret;
	}

private:
	std::stringstream ss_;
	mender::common::io::StreamReader reader_;
	size_t bytesRead_ {0};
};

TEST_F(OptimizedWriterTest, TestFlushingLimitWriterWrite) {
	// prepare a temp dir
	mender::common::testing::TemporaryDirectory tempDir;
	auto path = tempDir.Path() + "/foo";

	// Write some junk to the file
	auto s = mender::io::WriteFile(path, {'a', 'b', 'x', 'd', 'r', 'z', '1', '2', '3', '4'});
	ASSERT_TRUE(s) << s.error().message;

	auto f = mender::io::Open(path, true, true);
	ASSERT_TRUE(f) << f.error().message;
	mender::io::File fd = f.value();

	// set a limit to 10bytes
	mender::io::LimitedFlushingWriter writer(fd, 10);

	mender::io::Bytes payloadBuf {'f', 'o', 'o', 'b', 'a', 'r'};
	auto expectedBytesWritten = payloadBuf.size();

	auto res = writer.Write(payloadBuf.begin(), payloadBuf.end());
	ASSERT_TRUE(res) << res.error().message;
	ASSERT_EQ(res.value(), expectedBytesWritten);
}

TEST_F(OptimizedWriterTest, TestFlushingLimitWriterWriteNegative) {
	// prepare a temp dir
	mender::common::testing::TemporaryDirectory tempDir;
	auto path = tempDir.Path() + "/foo";

	auto createRes = mender::io::Create(path);
	ASSERT_EQ(createRes, NoError) << createRes;

	auto f = mender::io::Open(path, true, true);
	ASSERT_TRUE(f) << f.error().message;
	mender::io::File fd = f.value();

	// set a limit to 10bytes
	mender::io::LimitedFlushingWriter writer(fd, 10);

	// create a 12 byte buffer
	mender::io::Bytes payloadBuf {'f', 'o', 'o', 'b', 'a', 'r', 'f', 'o', 'o', 'b', 'a', 'r'};

	auto res = writer.Write(payloadBuf.begin(), payloadBuf.end());
	ASSERT_FALSE(res) << "Data written beyound the device limit";

	// check if the file is empty
	auto sizeRes = mender::io::GetSize(fd);
	ASSERT_TRUE(sizeRes);
	ASSERT_EQ(0, sizeRes.value());
}

TEST_F(OptimizedWriterTest, TestOptimizedWriter) {
	// prepare a temp dir
	mender::common::testing::TemporaryDirectory tempDir;
	auto path = tempDir.Path() + "/foo";

	std::stringstream ss;
	ss << "dd if=/dev/urandom of=" << path << " bs=1M count=10 status=none";
	system(ss.str().c_str());

	// create a reader
	auto f = mender::io::Open(path, true, false);
	ASSERT_TRUE(f) << f.error().message;
	mender::io::File fd = f.value();
	mender::io::FileReader reader(fd);

	// create a writer
	auto path2 = path + ".copy";
	auto createRes = mender::io::Create(path2);
	ASSERT_EQ(createRes, NoError) << createRes;
	auto f2 = mender::io::Open(path2, true, true);
	ASSERT_TRUE(f2) << f2.error().message;
	mender::io::File fd2 = f2.value();
	mender::io::FileWriter writer(fd2);

	// create read-writer
	mender::io::FileReadWriterSeeker readWriter(writer);

	// create optimized-writer
	mender::io::OptimizedWriter optWriter(reader, readWriter);
	auto copyRes = optWriter.Copy(true);
	ASSERT_EQ(copyRes, NoError) << copyRes.message;

	auto stats = optWriter.GetStatistics();
	ASSERT_EQ(stats.blocksWritten_, 10);
	ASSERT_EQ(stats.blocksOmitted_, 0);
	ASSERT_EQ(stats.bytesWritten_, 10 * 1024 * 1024);

	// rewind the files
	mender::io::SeekSet(fd, 0);
	mender::io::SeekSet(fd2, 0);

	// copy the data
	auto copyRes2 = optWriter.Copy(true);
	ASSERT_EQ(copyRes2, NoError) << copyRes.message;

	auto stats2 = optWriter.GetStatistics();
	ASSERT_EQ(stats2.blocksWritten_, 0);
	ASSERT_EQ(stats2.blocksOmitted_, 10);
	ASSERT_EQ(stats2.bytesWritten_, 0);

	// rewind the files
	mender::io::SeekSet(fd, 0);
	mender::io::SeekSet(fd2, 0);

	// copy the data (not optimized)
	auto copyRes3 = optWriter.Copy(false);
	ASSERT_EQ(copyRes3, NoError) << copyRes.message;

	auto stats3 = optWriter.GetStatistics();
	ASSERT_EQ(stats3.blocksWritten_, 10);
	ASSERT_EQ(stats3.blocksOmitted_, 0);
	ASSERT_EQ(stats3.bytesWritten_, 10 * 1024 * 1024);
}

TEST_F(OptimizedWriterTest, TestOptimizedWriterFailure) {
	// prepare a temp dir
	mender::common::testing::TemporaryDirectory tempDir;
	auto path = tempDir.Path() + "/foo";

	std::stringstream ss;
	ss << "dd if=/dev/urandom of=" << path << " bs=1M count=10 status=none";
	system(ss.str().c_str());

	// create a reader
	auto f = mender::io::Open(path, true, false);
	ASSERT_TRUE(f) << f.error().message;
	mender::io::File fd = f.value();
	mender::io::FileReader reader(fd);

	auto path2 = path + ".copy";
	auto createRes = mender::io::Create(path2);
	ASSERT_EQ(createRes, NoError) << createRes;
	auto f2 = mender::io::Open(path2, true, true);
	ASSERT_TRUE(f2) << f2.error().message;
	mender::io::File fd2 = f2.value();

	// TEST 1
	{
		mender::io::FileWriter writer(fd2);

		// create read-writer
		mender::io::FileReadWriterSeeker readWriter(writer);

		// create optimized-writer
		mender::io::OptimizedWriter optWriter(reader, readWriter);
		auto copyRes = optWriter.Copy(true);
		ASSERT_EQ(copyRes, NoError) << copyRes.message;

		auto stats = optWriter.GetStatistics();
		ASSERT_EQ(stats.blocksWritten_, 10);
		ASSERT_EQ(stats.blocksOmitted_, 0);
		ASSERT_EQ(stats.bytesWritten_, 10 * 1024 * 1024);
	}

	// TEST 2
	{
		// rewind the input file
		mender::io::SeekSet(fd, 0);

		// reopen the dst file (no write permission)
		f2 = mender::io::Open(path2, true, false);
		ASSERT_TRUE(f2) << f.error().message;
		fd2 = f2.value();
		mender::io::FileWriter writer2(fd2);

		// create read-writer
		mender::io::FileReadWriterSeeker readWriter2(writer2);

		// create optimized-writer
		mender::io::OptimizedWriter optWriter2(reader, readWriter2);
		auto copyRes2 = optWriter2.Copy(false);
		ASSERT_NE(copyRes2, NoError);
	}

	// TEST 3: source too short
	{
		// rewind the input file
		mender::io::SeekSet(fd, 0);

		// reopen the dst file (no write permission)
		f2 = mender::io::Open(path2, true, true);
		ASSERT_TRUE(f2) << f.error().message;
		fd2 = f2.value();
		mender::io::FileWriter writer3(fd2);

		// create read-writer
		mender::io::FileReadWriterSeeker readWriter3(writer3);

		// create optimized-writer
		mender::io::OptimizedWriter optWriter3(reader, readWriter3, 1024 * 1024, 11 * 1024 * 1024);
		auto copyRes3 = optWriter3.Copy(false);
		ASSERT_NE(copyRes3, NoError);
		ASSERT_EQ(
			copyRes3.message, "Size of the destination volume not reached, source too short.");
	}

	// TEST 4: source too big
	{
		// rewind the input file
		mender::io::SeekSet(fd, 0);

		// reopen the dst file (no write permission)
		f2 = mender::io::Open(path2, true, true);
		ASSERT_TRUE(f2) << f.error().message;
		fd2 = f2.value();
		mender::io::FileWriter writer4(fd2);

		// create read-writer
		mender::io::FileReadWriterSeeker readWriter4(writer4);

		// create optimized-writer
		mender::io::OptimizedWriter optWriter4(reader, readWriter4, 1024 * 1024, 9 * 1024 * 1024);
		auto copyRes4 = optWriter4.Copy(false);
		ASSERT_NE(copyRes4, NoError);
		ASSERT_EQ(copyRes4.message, "Reached size of the destination volume, source too big.");
	}
}

TEST_F(OptimizedWriterTest, TestOptimizedWriterLimit) {
	// prepare a temp dir
	mender::common::testing::TemporaryDirectory tempDir;

	struct {
		std::string input;
		int inputSize;
		int blockSize;
		int expectedBlockWritten;
		int expectedBytesWritten;
	} tests[] = {{"foobarfoobarfoobar", 10, 6, 1, 6}, {"fo", 0, 2, 1, 2}, {"foobar", 0, 4, 2, 6}};

	for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
		auto path = tempDir.Path() + "/foo" + std::to_string(i);
		StringFileReader reader(tests[i].input);

		// create writer
		auto createRes = mender::io::Create(path);
		ASSERT_EQ(createRes, NoError) << createRes;
		auto f = mender::io::Open(path, true, true);
		ASSERT_TRUE(f) << f.error().message;
		mender::io::File fd = f.value();
		mender::io::FileWriter writer(fd);

		// create read-writer
		mender::io::FileReadWriterSeeker readWriter(writer);

		// create optimized-writer
		mender::io::OptimizedWriter optWriter(
			reader, readWriter, tests[i].blockSize, tests[i].inputSize);
		optWriter.Copy(true);

		auto stats = optWriter.GetStatistics();
		ASSERT_EQ(stats.blocksWritten_, tests[i].expectedBlockWritten);
		ASSERT_EQ(stats.blocksOmitted_, 0);
		ASSERT_EQ(stats.bytesWritten_, tests[i].expectedBytesWritten);
	}
}
