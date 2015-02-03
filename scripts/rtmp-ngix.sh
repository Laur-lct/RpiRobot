#!/bin/bash
 on_die () {
   # kill all children
   pkill -KILL -P $$
 }
 trap 'on_die' TERM
raspivid -n -mm matrix -w 640 -h 480 -fps 25 -awb off -br 60 -g 100 -t 0 -b 4000000 -o - | ffmpeg -y -f h264 -i - -c:v copy -map 0:0 -f flv -rtmp_buffer 0 -rtmp_live l$
 wait