#!/bin/bash

APP_PATH="/usr/bin/SavvyCAN"
DESKTOP_PATH="/usr/share/applications/savvycan.desktop"
ICON_PATH="/usr/share/pixmaps/SavvyCAN.png"

require_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "This script must be run as root."
        exit 1
    fi
}

show_help() {
    echo "Usage: $0 [option]"
    echo ""
    echo "Options:"
    echo "  --help        Show this help message"
    echo "  --uninstall   Remove SavvyCAN from the system"
    echo "  (no option)   Install or reinstall SavvyCAN"
}

install_error() {
    echo "SavvyCAN installation failed."
    exit 1
}

uninstall_error() {
    echo "SavvyCAN uninstallation failed."
    exit 1
}

if [ "$1" = "--help" ]; then
    show_help
    exit 0
fi

require_root

# Uninstall
if [ "$1" = "--uninstall" ]; then
    echo "Uninstalling SavvyCAN..."
    trap uninstall_error ERR
    rm -f "$APP_PATH" "$DESKTOP_PATH" "$ICON_PATH"
    trap - ERR
    echo "SavvyCAN has been uninstalled."
    exit 0
fi

# Check required files
if [ ! -f "SavvyCAN" ]; then
    echo "Missing file \"SavvyCAN\". You need to build first."
    install_error
fi

if [ ! -f "SavvyCAN.desktop" ]; then
    echo "Missing file \"SavvyCAN.desktop\"."
    install_error
fi

if [ ! -f "icons/SavvyIcon.png" ]; then
    echo "Missing file \"icons/SavvyIcon.png\"."
    install_error
fi

# Install
if [ -f "$APP_PATH" ]; then
    echo "Re-installing SavvyCAN..."
else
    echo "Installing SavvyCAN..."
fi

trap install_error ERR
install -Dm755 SavvyCAN "$APP_PATH"
install -Dm644 SavvyCAN.desktop "$DESKTOP_PATH"
install -Dm644 icons/SavvyIcon.png "$ICON_PATH"
trap - ERR

echo "SavvyCAN is installed."
