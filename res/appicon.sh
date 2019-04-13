#!/bin/sh -e

SCRIPT_PATH=$(dirname $(realpath -s $0))

convert -background transparent -define icon:auto-resize -resize 256x256 -density 1440 $SCRIPT_PATH/appicon.svg appicon-256.png
convert -background transparent -define icon:auto-resize -resize 128x128 -density 720  $SCRIPT_PATH/appicon.svg appicon-128.png
convert -background transparent -define icon:auto-resize -resize 96x96   -density 540  $SCRIPT_PATH/appicon.svg appicon-96.png
convert -background transparent -define icon:auto-resize -resize 64x64   -density 360  $SCRIPT_PATH/appicon.svg appicon-64.png
convert -background transparent -define icon:auto-resize -resize 48x48   -density 270  $SCRIPT_PATH/appicon.svg appicon-48.png
convert -background transparent -define icon:auto-resize -resize 32x32   -density 180  $SCRIPT_PATH/appicon.svg appicon-32.png
convert -background transparent -define icon:auto-resize -resize 16x16   -density 90   $SCRIPT_PATH/appicon.svg appicon-16.png
convert appicon-16.png appicon-32.png appicon-48.png appicon-64.png appicon-96.png appicon-128.png appicon-256.png appicon.ico