#!/bin/bash
# Build the CI firmware variants from ../desk_node and ../room_node (arduino-cli, esp32 core 3.0.7).
# Each test group gets its own MQTT prefix so retained messages / LWT from one run
# never leak into another; change NONCE for every new round of runs.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); SRC=$(dirname "$HERE"); N=${NONCE:-r1}
B=$(mktemp -d)
build() { # src dst prefix timescale [deskid]
  rm -rf $B/sketch; mkdir -p $B/sketch; cp "$SRC/$1"/*.{ino,cpp,h} $B/sketch/
  c=$B/sketch/config.h
  sed -i "s|#define TIME_SCALE      60|#define TIME_SCALE      $4|" $c
  sed -i "s|\"swer-7k3q/r1/\"|\"swer-ci-$N-$3/r1/\"|" $c
  [ -n "$5" ] && sed -i "s|^#ifndef DESK_ID|#define DESK_ID $5\n#ifndef DESK_ID|" $c
  arduino-cli compile -b esp32:esp32:esp32 --output-dir $B/out $B/sketch >/dev/null
  mkdir -p "$HERE/$2/build"; cp $B/out/sketch.ino.bin $B/out/sketch.ino.elf "$HERE/$2/build/"
  echo "built $2 prefix=swer-ci-$N-$3 TIME_SCALE=$4 desk=${5:-1}"
}
build room_node room_node     room  1
build desk_node desk_node     unit  120     # x120: EYE 10 s, STAND 15 s, snooze 2.5 s
build room_node room_int      int   1
build desk_node desk_node_int int   1       # real time: no break alerts during the link tests
build room_node room_multi    multi 1
build desk_node desk_multi1   multi 1
build desk_node desk_node_2   multi 1 2
