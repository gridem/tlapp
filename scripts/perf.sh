#!/bin/sh

sample perf_test -wait > ../.build/perf.out &
GLOG_minloglevel=1 ../.build/RelWithDebInfo/samples/perf_test
