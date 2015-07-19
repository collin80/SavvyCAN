#!/bin/sh

CURDIR=`pwd`
APPDIR="$(dirname -- "$(readlink -f -- "${0}")" )"

cd "$APPDIR"

# Set absolute path work around
sed -e "s,FULLPATH,$PWD,g" SavvyCAN.desktop > SavvyCAN.desktop.temp

cp SavvyCAN.desktop.temp ~/.local/share/applications/SavvyCAN.desktop
cp SavvyCAN.desktop.temp ~/Desktop/SavvyCAN.desktop
rm SavvyCAN.desktop.temp

echo "Installed SavvyCAN icons on menu and desktop !"

