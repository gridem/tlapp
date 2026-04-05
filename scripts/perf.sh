#!/bin/sh

sample engine_perf -wait > ../.build/perf.out &
GLOG_minloglevel=1 ../.build/RelWithDebInfo/benchmarks/engine_perf --gtest_brief=1
