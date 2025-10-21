#!/usr/bin/env python3
"""
Generador de programas assembly para producto punto paralelo
Genera archivos .asm parametrizados basados en config.h
"""

import sys
import re

def read_config_h():
    """Lee la configuración desde config.h"""
    config = {}
    try:
        with open('src/include/config.h', 'r') as f:
            content = f.read()
            
            # Buscar VECTOR_SIZE
            match = re.search(r'#define\s+VECTOR_SIZE\s+(\d+)', content)
            if match:
                config['VECTOR_SIZE'] = int(match.group(1))
            
            # Buscar NUM_PES
            match = re.search(r'#define\s+NUM_PES\s+(\d+)', content)
            if match:
                config['NUM_PES'] = int(match.group(1))
            
            # Buscar otras constantes
            for var in ['VECTOR_A_BASE', 'VECTOR_B_BASE', 'RESULTS_BASE', 
                        'FLAGS_BASE', 'BLOCK_SIZE', 'CONSTANTS_BASE', 'FINAL_RESULT_ADDR']:
                match = re.search(rf'#define\s+{var}\s+(\d+)', content)
                if match:
                    config[var] = int(match.group(1))
            
    except FileNotFoundError:
        print("Error: No se encontró src/include/config.h")
        print("Usando valores por defecto...")
        return None
    
    return config

# Leer configuración desde config.h
config = read_config_h()
if config:
    VECTOR_SIZE = config['VECTOR_SIZE']
    NUM_PES = config['NUM_PES']
    VECTOR_A_BASE = config.get('VECTOR_A_BASE', 0)
    VECTOR_B_BASE = config.get('VECTOR_B_BASE', 100)
    RESULTS_BASE = config.get('RESULTS_BASE', 200)
    FLAGS_BASE = config.get('FLAGS_BASE', 220)
    BLOCK_SIZE = config.get('BLOCK_SIZE', 4)
    CONSTANTS_BASE = config.get('CONSTANTS_BASE', 232)
    FINAL_RESULT_ADDR = config.get('FINAL_RESULT_ADDR', 216)
else:
    # Valores por defecto
    VECTOR_SIZE = 16
    NUM_PES = 4
    VECTOR_A_BASE = 0
    VECTOR_B_BASE = 100
    RESULTS_BASE = 200
    FLAGS_BASE = 220
    BLOCK_SIZE = 4
    CONSTANTS_BASE = 232
    FINAL_RESULT_ADDR = 216

SEGMENT_SIZE = VECTOR_SIZE // NUM_PES

def generate_worker_pe(pe_id):
    """Genera código assembly para PE trabajador (PE0-PE2) con LOOP"""
    
    start_idx = pe_id * SEGMENT_SIZE
    result_addr = RESULTS_BASE + pe_id * BLOCK_SIZE
    flag_addr = FLAGS_BASE + pe_id * BLOCK_SIZE
    
    code = f"""# ============================================================================
# Producto Punto Paralelo - PE{pe_id} (GENERADO AUTOMÁTICAMENTE CON LOOP)
# ============================================================================
# Segmento: elementos [{start_idx} to {start_idx + SEGMENT_SIZE - 1}]
# Configuración: VECTOR_SIZE={VECTOR_SIZE}, SEGMENT_SIZE={SEGMENT_SIZE}
# ============================================================================

# ============================================================================
# Inicialización
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (resultado parcial)
MOV R1, {float(start_idx)}         # R1 = índice de elemento actual
MOV R2, {float(VECTOR_B_BASE)}       # R2 = base de vector B
MOV R3, {float(SEGMENT_SIZE)}        # R3 = contador del loop (SEGMENT_SIZE)

# ============================================================================
# LOOP: Procesar SEGMENT_SIZE elementos
# ============================================================================
LOOP_START:
# Cargar A[i] usando addressing indirecto
LOAD R4, [R1]       # R4 = A[i]

# Calcular dirección de B[i] = VECTOR_B_BASE + i
FADD R5, R2, R1     # R5 = VECTOR_B_BASE + i
LOAD R6, [R5]       # R6 = B[i]

# Multiplicar A[i] * B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]

# Acumular resultado
FADD R0, R0, R7     # acum += A[i] * B[i]

# Incrementar índice
INC R1              # i++

# Decrementar contador y verificar si continuar
DEC R3              # contador--
JNZ LOOP_START      # Si R3 != 0, repetir loop

# ============================================================================
# Guardar resultado parcial
# ============================================================================
MOV R5, {float(result_addr)}       # RESULTS_BASE + {pe_id}*BLOCK_SIZE
STORE R0, [R5]      # Guardar resultado parcial de PE{pe_id}

# ============================================================================
# Señalizar finalización (barrier)
# ============================================================================
MOV R6, 1.0         # Flag value
MOV R7, {float(flag_addr)}       # FLAGS_BASE + {pe_id}*BLOCK_SIZE
STORE R6, [R7]      # Flag PE{pe_id} = 1.0

HALT
"""
    return code

def generate_master_pe(pe_id=3):
    """Genera código assembly para PE master (PE3) que hace reducción con LOOP"""
    
    start_idx = pe_id * SEGMENT_SIZE
    result_addr = RESULTS_BASE + pe_id * BLOCK_SIZE
    
    code = f"""# ============================================================================
# Producto Punto Paralelo - PE{pe_id} (MASTER - GENERADO CON LOOP)
# ============================================================================
# Segmento: elementos [{start_idx} to {start_idx + SEGMENT_SIZE - 1}] + REDUCCIÓN
# Configuración: VECTOR_SIZE={VECTOR_SIZE}, SEGMENT_SIZE={SEGMENT_SIZE}
# ============================================================================

# ============================================================================
# FASE 1: Calcular producto parcial propio con LOOP
# ============================================================================

# Inicialización
MOV R0, 0.0         # R0 = acumulador (resultado parcial)
MOV R1, {float(start_idx)}         # R1 = índice de elemento actual
MOV R2, {float(VECTOR_B_BASE)}       # R2 = base de vector B
MOV R3, {float(SEGMENT_SIZE)}        # R3 = contador del loop (SEGMENT_SIZE)

# LOOP: Procesar segmento propio
LOOP_START:
LOAD R4, [R1]       # R4 = A[i]
FADD R5, R2, R1     # R5 = VECTOR_B_BASE + i
LOAD R6, [R5]       # R6 = B[i]
FMUL R7, R4, R6     # R7 = A[i] * B[i]
FADD R0, R0, R7     # acum += A[i] * B[i]
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Si R3 != 0, repetir

# Guardar resultado parcial de PE{pe_id}
MOV R5, {float(result_addr)}       # RESULTS_BASE + {pe_id}*BLOCK_SIZE
STORE R0, [R5]      # Guardar resultado parcial de PE{pe_id}

# ============================================================================
# FASE 2: BARRIER - Esperar a que otros PEs terminen
# ============================================================================
WAIT_LOOP:
"""
    
    # Cargar flags de otros PEs
    for pe in range(NUM_PES - 1):  # PE0, PE1, PE2
        flag_addr = FLAGS_BASE + pe * BLOCK_SIZE
        code += f"MOV R{pe+1}, {float(flag_addr)}      # Dirección flag PE{pe}\n"
        code += f"LOAD R{pe+1}, [R{pe+1}]  # R{pe+1} = flag PE{pe}\n"
    
    # Sumar todos los flags
    code += f"""
# Sumar flags (deben sumar {NUM_PES - 1}.0)
FADD R6, R1, R2     # R6 = flag0 + flag1
"""
    if NUM_PES > 3:
        code += f"FADD R6, R6, R3     # R6 += flag2\n"
    
    code += f"""
# Restar {NUM_PES - 1}.0 para verificar
MOV R7, {float(CONSTANTS_BASE)}       # Dirección de constante -{NUM_PES - 1}.0
LOAD R7, [R7]       # R7 = -{NUM_PES - 1}.0
FADD R6, R6, R7     # R6 = suma_flags - {NUM_PES - 1}.0
JNZ WAIT_LOOP       # Si R6 != 0, repetir

# ============================================================================
# FASE 3: REDUCCIÓN - Sumar todos los productos parciales con LOOP
# ============================================================================

MOV R0, 0.0         # R0 = acumulador final
MOV R1, {float(RESULTS_BASE)}        # R1 = base de resultados parciales
MOV R2, {float(NUM_PES)}             # R2 = contador (NUM_PES)
MOV R3, {float(BLOCK_SIZE)}          # R3 = BLOCK_SIZE (offset entre resultados)

REDUCE_LOOP:
# Cargar resultado parcial
LOAD R4, [R1]       # R4 = resultado_parcial[i]
FADD R0, R0, R4     # acum += resultado_parcial[i]

# Avanzar a siguiente resultado (dirección += BLOCK_SIZE)
FADD R1, R1, R3     # R1 += BLOCK_SIZE

# Decrementar contador
DEC R2              # contador--
JNZ REDUCE_LOOP     # Si R2 != 0, repetir

# ============================================================================
# Guardar resultado final
# ============================================================================
MOV R2, {float(FINAL_RESULT_ADDR)}       # Dirección resultado final
STORE R0, [R2]      # Guardar producto punto final

HALT
"""
    return code

def main():
    """Genera todos los archivos .asm"""
    
    print(f"Generando archivos assembly para:")
    print(f"  VECTOR_SIZE = {VECTOR_SIZE}")
    print(f"  NUM_PES = {NUM_PES}")
    print(f"  SEGMENT_SIZE = {SEGMENT_SIZE}")
    print()
    
    # Generar PEs trabajadores (PE0 - PE2)
    for pe_id in range(NUM_PES - 1):
        filename = f"asm/dotprod_pe{pe_id}.asm"
        code = generate_worker_pe(pe_id)
        with open(filename, 'w') as f:
            f.write(code)
        print(f"✓ Generado: {filename}")
    
    # Generar PE master (PE3)
    filename = f"asm/dotprod_pe{NUM_PES - 1}.asm"
    code = generate_master_pe(NUM_PES - 1)
    with open(filename, 'w') as f:
        f.write(code)
    print(f"✓ Generado: {filename}")
    
    print(f"\n✅ {NUM_PES} archivos generados correctamente")

if __name__ == "__main__":
    main()
