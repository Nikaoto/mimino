#! /bin/bash

mkdir -p results/

rm -rf results/*

cd results/

curl -L localhost:8080/buckswood-agi.html > buckswood-agi.html 2>/dev/null &
curl -L localhost:8080/buckswood.jpg > buckswood.jpg 2>/dev/null &
curl -L localhost:8080/game-design-documents.jpg > game-design-documents.jpg 2>/dev/null &
curl -L localhost:8080/gamedev-conferences-and-casual-connect.jpg > gamedev-conferences-and-casual-connect.jpg 2>/dev/null &
curl -L localhost:8080/gasb.jpg > gasb.jpg 2>/dev/null &
curl -L localhost:8080/grab-yt-thumbnail.sh > grab-yt-thumbnail.sh 2>/dev/null &
curl -L localhost:8080/hacktoberfest-for-gamedevs.jpg > hacktoberfest-for-gamedevs.jpg 2>/dev/null &
curl -L localhost:8080/index.html > index.html 2>/dev/null &
curl -L localhost:8080/indiepocalypse.jpg > indiepocalypse.jpg 2>/dev/null &
curl -L localhost:8080/raycast-rendering.jpg > raycast-rendering.jpg 2>/dev/null &
curl -L localhost:8080/dinamo.pdf > dinamo.pdf 2>/dev/null &
curl -L localhost:8080/dir%20with%20spaces/ > 'dir with spaces' 2>/dev/null &
curl -L localhost:8080/emptyfile > emptyfile 2>/dev/null &
curl -L localhost:8080/file%20with%20spaces > 'file with spaces' 2>/dev/null &
curl -L localhost:8080/large_request > large_request 2>/dev/null &
curl -L localhost:8080/small_request > small_request 2>/dev/null &
curl -L localhost:8080/style.css > style.css 2>/dev/null &
curl -L localhost:8080/symlink-to-dir-outside > symlink-to-dir-outside 2>/dev/null &
curl -L localhost:8080/symlink-to-file-outside > symlink-to-file-outside 2>/dev/null &
curl -L localhost:8080/unfinished_request > unfinished_request 2>/dev/null &
#curl -L localhost:8080/ქართული_ფაილი > ქართული_ფაილი 2>/dev/null &

sleep 2
cd ..
diff -rq testdir/ results/
