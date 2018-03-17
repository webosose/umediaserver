#!/bin/bash

load () {
	echo "luna-send -n 1 palm://com.webos.media/load {\"args\":[\"file://$1\",\"file\"]}"
	luna-send -n 1 palm://com.webos.media/load '{"args":["file:///media/internal/bolt.mp4","file"]}'
}

play () {
	echo luna-send -n 1 palm://com.webos.media/play '{"args":[]}'
	luna-send -n 1 palm://com.webos.media/play '{"args":[]}'
}

pause () {
	echo luna-send -n 1 palm://com.webos.media/pause '{"args":[]}'
	luna-send -n 1 palm://com.webos.media/pause '{"args":[]}'
}

# ====================================

load /media/internal/bolt.mp4
play
sleep 20
pause

fitfill fit



