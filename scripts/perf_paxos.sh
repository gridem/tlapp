#!/bin/sh

sample paxos -wait > ../.build/perf.out &
GLOG_minloglevel=1 ../.build/RelWithDebInfo/samples/paxos
