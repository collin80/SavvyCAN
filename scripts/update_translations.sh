#!/bin/bash

PROJECT_ROOT="$(pwd)"

# To add this tool in Qt Creator:
# Category: Linguist
# Name: Update Translations (lupdate)
# Executable: lupdate
# Arguments: -project SavvyCAN.pro
# Working Directory: %{ActiveProject:Path}

echo "Updating translation files..."

lupdate -project "$PROJECT_ROOT"/SavvyCAN.pro

echo "Translation files updated."
