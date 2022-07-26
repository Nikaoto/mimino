#! /bin/bash

mkdir -p results/

rm -rf results/*

cd results/

curl -v localhost:8080/buckswood-agi.html > buckswood-agi.html &
curl -v localhost:8080/buckswood.jpg > buckswood.jpg &
curl -v localhost:8080/game-design-documents.jpg > game-design-documents.jpg &
curl -v localhost:8080/gamedev-conferences-and-casual-connect.jpg > gamedev-conferences-and-casual-connect.jpg &
curl -v localhost:8080/gasb.jpg > gasb.jpg &
curl -v localhost:8080/grab-yt-thumbnail.sh > grab-yt-thumbnail.sh &
curl -v localhost:8080/hacktoberfest-for-gamedevs.jpg > hacktoberfest-for-gamedevs.jpg &
curl -v localhost:8080/index.html > index.html &
curl -v localhost:8080/indiepocalypse.jpg > indiepocalypse.jpg &
curl -v localhost:8080/raycast-rendering.jpg > raycast-rendering.jpg &
