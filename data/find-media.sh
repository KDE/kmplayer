#!/bin/sh

echo '<playlist>'

find "$1" -name '*.mp3' -o -name '*.avi' -o -name '*.mp4' -o -name '*.mpg' -o -name '*.ogg' -o -name '*.m3u' -o -name '*.flv' | sed 's,^\(.*\)$,  <item url="\1"/>,'

echo '</playlist>'
