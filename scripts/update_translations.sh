#!/bin/bash

PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"
TRANSLATIONS_DIR="$PROJECT_ROOT/translations"

echo "Atualizando arquivos de tradução..."

lupdate "$PROJECT_ROOT" \
-ts "$TRANSLATIONS_DIR"/SavvyCAN_*.ts

echo "Traduções atualizadas."