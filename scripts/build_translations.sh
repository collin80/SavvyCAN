#!/bin/bash

PROJECT_ROOT="$(pwd)"

# To add this tool in Qt Creator:
# Category: Linguist
# Name: Release Translations (lrelease)
# Executable: lrelease
# Arguments: SavvyCAN.pro
# Working Directory: %{ActiveProject:Path}

echo "Generating .qm files..."

lrelease -verbose "$PROJECT_ROOT"/SavvyCAN.pro

echo ".qm files generated."
