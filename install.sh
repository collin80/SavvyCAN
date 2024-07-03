#!/bin/sh

CURDIR=`pwd`

# Set absolute path
sed -e "s,<DIR>,$CURDIR,g" SavvyCAN.desktop > SavvyCAN.desktop.temp

cp SavvyCAN.desktop.temp ~/.local/share/applications/SavvyCAN.desktop
cp SavvyCAN.desktop.temp ~/Desktop/SavvyCAN.desktop
rm SavvyCAN.desktop.temp

echo "Installed SavvyCAN icons on menu and desktop !"
