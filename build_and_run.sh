#!/bin/bash
set -e
if [[ $# -ne 1 ]]; then
  echo "需要build目录作为参数"
  exit 1;
fi

cd $(dirname $0)
build_dir="$1"
${build_dir}/practice_Kaleidoscope < code.txt > ${build_dir}/code.ll
clang -g ${build_dir}/code.ll runtime.c -lm -o ${build_dir}/kaleido_test
gdb ${build_dir}/kaleido_test