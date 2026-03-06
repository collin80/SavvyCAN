#!/bin/bash

PROJECT_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"
TRANSLATIONS_DIR="$PROJECT_ROOT/translations"

echo "Gerando arquivos .qm..."

for file in "$TRANSLATIONS_DIR"/*.ts; do
    lrelease "$file"
done

echo "Arquivos .qm gerados."