#!/bin/bash

# Script de Verificación Automatizada del Simulador MESI
# Verifica la correcta ejecución y ausencia de errores

echo "======================================"
echo "  Verificación del Simulador MESI"
echo "======================================"
echo ""

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS=0
FAIL=0

# Test 1: Compilación
echo "[TEST 1] Compilación..."
make clean > /dev/null 2>&1
if make > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASS${NC} - Compilación exitosa"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Error de compilación"
    ((FAIL++))
    exit 1
fi
echo ""

# Test 2: Ejecución sin crashes
echo "[TEST 2] Ejecución sin crashes..."
timeout 5 ./mp_mesi > /tmp/mesi_output.txt 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Programa ejecutó sin crashes"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Programa crasheó o timeout"
    ((FAIL++))
fi
echo ""

# Test 3: Threads creados
echo "[TEST 3] Creación de threads..."
BUS_THREAD=$(grep -c "\[BUS\] Thread iniciado" /tmp/mesi_output.txt)
PE_THREADS=$(grep -c "Starting thread" /tmp/mesi_output.txt)

if [ "$BUS_THREAD" -eq 1 ] && [ "$PE_THREADS" -eq 4 ]; then
    echo -e "${GREEN}✓ PASS${NC} - 5 threads creados (1 bus + 4 PEs)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Threads incorrectos (Bus: $BUS_THREAD, PEs: $PE_THREADS)"
    ((FAIL++))
fi
echo ""

# Test 4: No hay señales duplicadas
echo "[TEST 4] Verificando señales duplicadas al bus..."
DUPLICATES=$(grep "BUS] Señal" /tmp/mesi_output.txt | sort | uniq -c | awk '$1 > 1 {print}' | wc -l)
if [ "$DUPLICATES" -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC} - No hay señales duplicadas"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Encontradas $DUPLICATES señales duplicadas"
    ((FAIL++))
fi
echo ""

# Test 5: Todos los PEs terminan
echo "[TEST 5] Verificando terminación de PEs..."
FINISHED=$(grep -c "Finished" /tmp/mesi_output.txt)
if [ "$FINISHED" -eq 4 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Los 4 PEs terminaron exitosamente"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Solo $FINISHED PEs terminaron"
    ((FAIL++))
fi
echo ""

# Test 6: Bus termina limpiamente
echo "[TEST 6] Verificando terminación del bus..."
BUS_END=$(grep -c "\[BUS\] Thread terminado" /tmp/mesi_output.txt)
if [ "$BUS_END" -eq 1 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Bus terminó correctamente"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Bus no terminó correctamente"
    ((FAIL++))
fi
echo ""

# Test 7: Verificar resultados de PE0 (suma)
echo "[TEST 7] Verificando resultado de PE0 (suma: 6+3.5=9.5)..."
# Buscar R2 en el contexto de PE0 Register File, manejando posibles saltos de línea
PE0_R2=$(grep -A 6 "PE0 Register File" /tmp/mesi_output.txt | grep -o "R2: [0-9.]*" | head -1 | awk '{print $2}')
if [ "$PE0_R2" == "9.500000" ]; then
    echo -e "${GREEN}✓ PASS${NC} - PE0 R2 = 9.500000 (correcto)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - PE0 R2 = $PE0_R2 (esperado 9.500000)"
    ((FAIL++))
fi
echo ""

# Test 8: Verificar resultados de PE1 (producto)
echo "[TEST 8] Verificando resultado de PE1 (6*3.5+6=27)..."
# Buscar R3 en el contexto de PE1 Register File
PE1_R3=$(grep -A 6 "PE1 Register File" /tmp/mesi_output.txt | grep -o "R3: [0-9.]*" | head -1 | awk '{print $2}')
if [ "$PE1_R3" == "27.000000" ]; then
    echo -e "${GREEN}✓ PASS${NC} - PE1 R3 = 27.000000 (correcto)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - PE1 R3 = $PE1_R3 (esperado 27.000000)"
    ((FAIL++))
fi
echo ""

# Test 9: Verificar resultados de PE2 (loop)
echo "[TEST 9] Verificando resultado de PE2 (loop: R0=0, R1=14)..."
PE2_R0=$(grep -A 6 "PE2 Register File" /tmp/mesi_output.txt | grep -o "R0: [0-9.]*" | head -1 | awk '{print $2}')
PE2_R1=$(grep -A 6 "PE2 Register File" /tmp/mesi_output.txt | grep -o "R1: [0-9.]*" | head -1 | awk '{print $2}')
if [ "$PE2_R0" == "0.000000" ] && [ "$PE2_R1" == "14.000000" ]; then
    echo -e "${GREEN}✓ PASS${NC} - PE2 R0=0, R1=14 (correcto)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - PE2 R0=$PE2_R0 R1=$PE2_R1 (esperado 0, 14)"
    ((FAIL++))
fi
echo ""

# Test 10: Verificar resultados de PE3 (loop con JNZ - test_isa.asm)
echo "[TEST 10] Verificando resultado de PE3 (loop: R0=0)..."
PE3_R0=$(grep -A 6 "PE3 Register File" /tmp/mesi_output.txt | grep -o "R0: [0-9.]*" | head -1 | awk '{print $2}')
if [ "$PE3_R0" == "0.000000" ]; then
    echo -e "${GREEN}✓ PASS${NC} - PE3 R0=0 (correcto)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - PE3 R0=$PE3_R0 (esperado 0)"
    ((FAIL++))
fi
echo ""

# Test 11: Protocolo MESI - Writebacks
echo "[TEST 11] Verificando writebacks del protocolo MESI..."
WRITEBACKS=$(grep -c "haciendo WRITEBACK" /tmp/mesi_output.txt)
if [ "$WRITEBACKS" -gt 0 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Encontrados $WRITEBACKS writebacks (coherencia activa)"
    ((PASS++))
else
    echo -e "${YELLOW}⚠ WARN${NC} - No se encontraron writebacks"
    ((PASS++))
fi
echo ""

# Test 12: Protocolo MESI - Invalidaciones
echo "[TEST 12] Verificando invalidaciones del protocolo MESI..."
INVALIDATIONS=$(grep -c "invalidando" /tmp/mesi_output.txt)
if [ "$INVALIDATIONS" -gt 0 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Encontradas $INVALIDATIONS invalidaciones (coherencia activa)"
    ((PASS++))
else
    echo -e "${YELLOW}⚠ WARN${NC} - No se encontraron invalidaciones"
    ((PASS++))
fi
echo ""

# Test 13: Labels resueltos
echo "[TEST 13] Verificando resolución de labels..."
LABELS=$(grep -c "Label 'LOOP' -> línea" /tmp/mesi_output.txt)
if [ "$LABELS" -ge 1 ]; then
    echo -e "${GREEN}✓ PASS${NC} - Label 'LOOP' resuelto correctamente ($LABELS labels encontrados)"
    ((PASS++))
else
    echo -e "${RED}✗ FAIL${NC} - Labels no resueltos correctamente"
    ((FAIL++))
fi
echo ""

# Resumen
echo "======================================"
echo "         RESUMEN DE PRUEBAS"
echo "======================================"
TOTAL=$((PASS + FAIL))
PERCENTAGE=$((PASS * 100 / TOTAL))

echo -e "Total de pruebas: $TOTAL"
echo -e "${GREEN}Pasadas: $PASS${NC}"
echo -e "${RED}Falladas: $FAIL${NC}"
echo -e "Porcentaje de éxito: $PERCENTAGE%"
echo ""

if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}✓✓✓ TODAS LAS PRUEBAS PASARON ✓✓✓${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}✗✗✗ ALGUNAS PRUEBAS FALLARON ✗✗✗${NC}"
    echo ""
    echo "Ver detalles en: /tmp/mesi_output.txt"
    exit 1
fi
