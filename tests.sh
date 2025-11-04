#!/bin/sh
# Copyright 2025 Northern.tech AS
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

MEN_FLASH="./mender-flash"
if [ $# -gt 1 ]; then
  MEN_FLASH="$1/mender-flash"
fi

# whatever number that is unlikely to be returned by one of the tests by
# accident (e.g. by its last command)
SKIP_EXIT_CODE=66

BLOCK=1048576

total_tests=0
passing=0
failing=0
skipped=0

run_test() {
  local test="$1"

  total_tests=$((total_tests + 1))
  echo "$test: RUN"
  if $test; then
    echo "$test: PASS"
    passing=$((passing + 1))
  else
    if [ $? = $SKIP_EXIT_CODE ]; then
      echo "$test: SKIP"
      skipped=$((skipped + 1))
    else
      echo "$test: FAIL"
      failing=$((failing + 1))
    fi
  fi
}

print_summary() {
  echo "===================================="
  echo "Total number of tests: $total_tests"
  echo "Passing: $passing"
  echo "Failing: $failing"
  echo "Skipped: $skipped"
  echo "===================================="
}

help_test() {
  $MEN_FLASH -h 2> "${TEST_DIR}/help"
  echo 'Usage:' > "${TEST_DIR}/help.exp"
  echo '  mender-flash [-h|--help] [-w|--write-everything] [-s|--input-size <INPUT_SIZE>] [-f|--fsync-interval <FSYNC_INTERVAL>] -i|--input <INPUT_PATH> -o|--output <OUTPUT_PATH>' >> "${TEST_DIR}/help.exp"
  ret=0
  diff "${TEST_DIR}/help" "${TEST_DIR}/help.exp" > /dev/null || ret=1
  if [ $ret = 1 ]; then
    echo "Unexpected output from --help"
    diff "${TEST_DIR}/help" "${TEST_DIR}/help.exp"
  fi
  return $ret
}

bad_option_test() {
  $MEN_FLASH -Y 2> "${TEST_DIR}/help"
  ret=$?
  if [ $ret -ne 1 ]; then
    echo "Unexpected exit code from bad option"
    return $ret
  fi
  echo "$MEN_FLASH: invalid option -- 'Y'" > "${TEST_DIR}/help.exp"
  echo 'Usage:' >> "${TEST_DIR}/help.exp"
  echo '  mender-flash [-h|--help] [-w|--write-everything] [-s|--input-size <INPUT_SIZE>] [-f|--fsync-interval <FSYNC_INTERVAL>] -i|--input <INPUT_PATH> -o|--output <OUTPUT_PATH>' >> "${TEST_DIR}/help.exp"
  ret=0
  diff "${TEST_DIR}/help" "${TEST_DIR}/help.exp" > /dev/null || ret=1
  if [ $ret = 1 ]; then
    echo "Unexpected output from --help"
    diff "${TEST_DIR}/help" "${TEST_DIR}/help.exp"
  fi
  return $ret
}

basic_write_with_defaults_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

basic_write_with_size_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH --input-size $n_bytes -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
    ret=$?
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

basic_write_with_short_size_test() {
  local n_bytes=$BLOCK
  local short_size=$((n_bytes - 1000))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH --input-size $short_size -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$short_size\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$short_size\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  if [ $(stat -c %s "$output") -ne $short_size ]; then
    echo "Wrong output size"
    ret=1
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

double_write_with_defaults_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats" &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+0\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

basic_write_with_no_sync_test() {
  local n_bytes=$((BLOCK * 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  # Use '-f' here instead of '--fsync-interval' so that we test the short option
  # somewhere too.
  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -f 0 -i "$input" -o "$output" > "$stats"
  ret=$?
  sync "$output"

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

partial_match_with_defaults_test() {
  local n_bytes=$((BLOCK * 2))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$BLOCK count=1 seek=1 conv=notrunc >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Blocks written:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    grep "Blocks omitted:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks omitted' stats" && ret=1; }
    grep "Bytes written:\s\+$BLOCK\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

double_write_everything_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats" &&
    $MEN_FLASH -w -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes written: $n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

double_write_everything_with_no_sync_test() {
  local n_bytes=$((BLOCK * 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats" &&
    $MEN_FLASH -w --fsync-interval 0 -i "$input" -o "$output" > "$stats"
  ret=$?
  sync "$output"

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes written: $n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

partial_match_write_everything_test() {
  local n_bytes=$((BLOCK * 2))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$BLOCK count=1 seek=1 conv=notrunc >/dev/null 2>&1 &&
    $MEN_FLASH -w -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes written: $n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_basic_write_with_no_sync_test() {
  local n_bytes=$((BLOCK * 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --fsync-interval 0 --input-size $n_bytes -i - -o "$output" > "$stats"
  ret=$?
  sync "$output"

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_basic_write_with_size_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
    ret=$?
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_basic_write_with_short_size_test() {
  local n_bytes=$BLOCK
  local short_size=$((n_bytes - 1000))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $short_size -i - -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$short_size\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$short_size\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  if [ $(stat -c %s "$output") -ne $short_size ]; then
    echo "Wrong output size"
    ret=1
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_double_write_with_defaults_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > "$stats" &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+0\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_partial_match_with_defaults_test() {
  local n_bytes=$((BLOCK * 2))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$BLOCK count=1 seek=1 conv=notrunc >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Blocks written:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    grep "Blocks omitted:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks omitted' stats" && ret=1; }
    grep "Bytes written:\s\+$BLOCK\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_double_write_everything_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > "$stats" &&
    cat "$input" | $MEN_FLASH -w --input-size $n_bytes -i - -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes written: $n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_partial_match_write_everything_test() {
  local n_bytes=$((BLOCK * 2))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH --input-size $n_bytes -i - -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$BLOCK count=1 seek=1 conv=notrunc >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH -w --input-size $n_bytes -i - -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes written: $n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

pipe_write_no_size_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local err_out="${TEST_DIR}/err_out"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    cat "$input" | $MEN_FLASH -i - -o "$output" 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Input size not specified and cannot be determined from stat()" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

no_input_test() {
  local n_bytes=$BLOCK
  local output="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  $MEN_FLASH -o "$output" 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Wrong input parameters" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

bad_input_test() {
  local n_bytes=$BLOCK
  local output="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  $MEN_FLASH -i /non-existing/file -o "$output" 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Failed to open '/non-existing/file'" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

err_input_test() {
  local n_bytes=$BLOCK
  local output="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  # directory can be opened but read() gives EISDIR
  $MEN_FLASH -i / -o "$output" 2> "$err_out" >/dev/null
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Failed to read data" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

bad_output_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  $MEN_FLASH -i /dev/zero -o "/should/not/work" 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Failed to open '/should/not/work'" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

bad_sync_interval_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  $MEN_FLASH --fsync-interval not-a-number -i /dev/zero -o /dev/null 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Invalid fsync interval given: not-a-number" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

bad_size_test() {
  local n_bytes=$BLOCK
  local input="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  $MEN_FLASH --input-size not-a-number -i /dev/zero -o /dev/null 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Invalid input size given: not-a-number" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

pipe_fail_test() {
  local n_bytes=$BLOCK
  local output="${TEST_DIR}/test.out"
  local out="${TEST_DIR}/out"
  local err_out="${TEST_DIR}/err_out"

  cat /dev/null | $MEN_FLASH --input-size $n_bytes -i - -o >"$out" "$output" 2> "$err_out"
  if [ $? = 1 ]; then
    # we actually want to see a failure here
    ret=0
  fi

  if [ $ret = 0 ]; then
    grep "Unexpected end of input" "$err_out" >/dev/null || { echo "Wrong error message" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$err_out"
    fi
  fi
  if [ $ret = 0 ]; then
    grep "Total bytes written: 0\$" "$out" >/dev/null || { echo "Wrong output" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$out"
    fi
  fi

  rm -f "$output"
  rm -f "$err_out"
  return $ret
}

partial_match_in_the_middle_test() {
  local n_bytes=$((BLOCK * 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$BLOCK count=1 seek=1 conv=notrunc >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Blocks written:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    grep "Blocks omitted:\s\+2\$" "$stats" >/dev/null || { echo "Wrong 'Blocks omitted' stats" && ret=1; }
    grep "Bytes written:\s\+$BLOCK\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

partial_match_in_the_middle_block_overlap_test() {
  local n_bytes=$((BLOCK * 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > /dev/null &&
    dd if=/dev/urandom of="$input" bs=$((BLOCK / 2)) count=2 seek=1 conv=notrunc >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Blocks written:\s\+2\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    grep "Blocks omitted:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks omitted' stats" && ret=1; }
    grep "Bytes written:\s\+$((BLOCK * 2))\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

basic_write_with_no_block_multiple_test() {
  local n_bytes=$((2 * BLOCK + 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    grep "Blocks written:\s\+3\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

basic_write_shorter_than_block_test() {
  local n_bytes=$((BLOCK - 3))
  local input="${TEST_DIR}/test.img"
  local output="${TEST_DIR}/test.out"
  local stats="${TEST_DIR}/test.stats"

  dd if=/dev/urandom of="$input" bs=$n_bytes count=1 >/dev/null 2>&1 &&
    $MEN_FLASH -i "$input" -o "$output" > "$stats"
  ret=$?

  if [ $ret = 0 ]; then
    diff "$input" "$output" >/dev/null || { echo "Input and output differ" && ret=1; }
  fi

  if [ $ret = 0 ]; then
    grep "Total bytes:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Total bytes' stats" && ret=1; }
    grep "Bytes written:\s\+$n_bytes\$" "$stats" >/dev/null || { echo "Wrong 'Bytes written' stats" && ret=1; }
    grep "Blocks written:\s\+1\$" "$stats" >/dev/null || { echo "Wrong 'Blocks written' stats" && ret=1; }
    if [ $ret != 0 ]; then
      cat "$stats"
    fi
  fi

  rm -f "$input"
  rm -f "$output"
  rm -f "$stats"
  return $ret
}

if ! which cat >/dev/null; then
  echo "cat needs to be availble for these tests"
  exit 1
fi

if ! which dd >/dev/null; then
  echo "dd needs to be availble for these tests"
  exit 1
fi

if ! which diff >/dev/null; then
  echo "diff needs to be availble for these tests"
  exit 1
fi

if ! which grep >/dev/null; then
  echo "grep needs to be availble for these tests"
  exit 1
fi


TEST_DIR="$(mktemp -t -d mender-flash-test-dir-XXXXXX)"
trap "rm -rf $TEST_DIR" EXIT

run_test help_test
run_test bad_option_test

run_test basic_write_with_defaults_test
run_test basic_write_with_size_test
run_test basic_write_with_no_sync_test
run_test basic_write_with_short_size_test
run_test double_write_with_defaults_test
run_test partial_match_with_defaults_test
run_test double_write_everything_test
run_test double_write_everything_with_no_sync_test
run_test partial_match_write_everything_test

run_test pipe_basic_write_with_size_test
run_test pipe_basic_write_with_no_sync_test
run_test pipe_basic_write_with_short_size_test
run_test pipe_double_write_with_defaults_test
run_test pipe_partial_match_with_defaults_test
run_test pipe_double_write_everything_test
run_test pipe_partial_match_write_everything_test

run_test no_input_test
run_test bad_input_test
run_test err_input_test
run_test bad_output_test
run_test bad_sync_interval_test
run_test bad_size_test
run_test pipe_write_no_size_test
run_test pipe_fail_test

run_test partial_match_in_the_middle_test
run_test partial_match_in_the_middle_block_overlap_test

run_test basic_write_with_no_block_multiple_test
run_test basic_write_shorter_than_block_test

print_summary
exit $failing
