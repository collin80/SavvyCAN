#!/bin/bash

PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"
TRANSLATIONS_DIR="$PROJECT_ROOT/translations"

linguist "$TRANSLATIONS_DIR"/SavvyCAN_*.ts