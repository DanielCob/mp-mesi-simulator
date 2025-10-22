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
            
            # Buscar constantes básicas
            for var in ['SHARED_CONFIG_ADDR', 'RESULTS_ADDR', 'FLAGS_ADDR', 'FINAL_RESULT_ADDR']:
                match = re.search(rf'#define\s+{var}\s+(\d+)', content)
                if match:
                    config[var] = int(match.group(1))
            
            # Buscar offsets dentro de SHARED_CONFIG
            for var in ['CFG_VECTOR_A_ADDR', 'CFG_VECTOR_B_ADDR', 'CFG_RESULTS_ADDR',
                        'CFG_FLAGS_ADDR', 'CFG_FINAL_RESULT_ADDR', 'CFG_NUM_PES_ADDR', 
                        'CFG_BARRIER_CHECK_ADDR', 'CFG_PE_START_ADDR']:
                match = re.search(rf'#define\s+{var}\s+(\d+)', content)
                if match:
                    config[var] = int(match.group(1))
            
            # Calcular SEGMENT_SIZE para workers y master
            if 'VECTOR_SIZE' in config and 'NUM_PES' in config:
                config['SEGMENT_SIZE_WORKER'] = config['VECTOR_SIZE'] // config['NUM_PES']
                config['RESIDUE'] = config['VECTOR_SIZE'] % config['NUM_PES']
                config['SEGMENT_SIZE_MASTER'] = config['SEGMENT_SIZE_WORKER'] + config['RESIDUE']
            
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
    SEGMENT_SIZE_WORKER = config['SEGMENT_SIZE_WORKER']
    SEGMENT_SIZE_MASTER = config['SEGMENT_SIZE_MASTER']
    RESIDUE = config['RESIDUE']
    
    # Nuevo layout: shared config area
    SHARED_CONFIG_ADDR = config.get('SHARED_CONFIG_ADDR', 0)
    CFG_VECTOR_A_ADDR = config.get('CFG_VECTOR_A_ADDR', 0)
    CFG_VECTOR_B_ADDR = config.get('CFG_VECTOR_B_ADDR', 1)
    CFG_RESULTS_ADDR = config.get('CFG_RESULTS_ADDR', 2)
    CFG_FLAGS_ADDR = config.get('CFG_FLAGS_ADDR', 3)
    CFG_FINAL_RESULT_ADDR = config.get('CFG_FINAL_RESULT_ADDR', 4)
    CFG_NUM_PES_ADDR = config.get('CFG_NUM_PES_ADDR', 5)
    CFG_BARRIER_CHECK_ADDR = config.get('CFG_BARRIER_CHECK_ADDR', 6)
    CFG_PE_START_ADDR = config.get('CFG_PE_START_ADDR', 8)
    
    # Áreas de sincronización y resultados (compactas)
    RESULTS_ADDR = config.get('RESULTS_ADDR', 16)
    FLAGS_ADDR = config.get('FLAGS_ADDR', 20)
    FINAL_RESULT_ADDR = config.get('FINAL_RESULT_ADDR', 24)
else:
    # Valores por defecto
    VECTOR_SIZE = 16
    NUM_PES = 4
    SEGMENT_SIZE_WORKER = VECTOR_SIZE // NUM_PES
    RESIDUE = VECTOR_SIZE % NUM_PES
    SEGMENT_SIZE_MASTER = SEGMENT_SIZE_WORKER + RESIDUE
    
    SHARED_CONFIG_ADDR = 0
    CFG_VECTOR_A_ADDR = 0
    CFG_VECTOR_B_ADDR = 1
    CFG_RESULTS_ADDR = 2
    CFG_FLAGS_ADDR = 3
    CFG_FINAL_RESULT_ADDR = 4
    CFG_NUM_PES_ADDR = 5
    CFG_BARRIER_CHECK_ADDR = 6
    CFG_PE_START_ADDR = 8
    
    RESULTS_ADDR = 16
    FLAGS_ADDR = 20
    FINAL_RESULT_ADDR = 24

def generate_worker_pe(pe_id):
    """Genera código assembly GENÉRICO para PE trabajador (PE0-PE2)
    CARGA TODOS LOS PARÁMETROS DESDE SHARED_CONFIG EN MEMORIA"""
    
    # Calcular offset dentro de CFG_PE para este PE
    # CFG_PE(pe, 0) = CFG_PE_START_ADDR + pe*2 (start_index)
    # CFG_PE(pe, 1) = CFG_PE_START_ADDR + pe*2 + 1 (segment_size)
    cfg_start_idx_addr = CFG_PE_START_ADDR + pe_id * 2
    cfg_segment_size_addr = CFG_PE_START_ADDR + pe_id * 2 + 1
    
    code = f"""# ============================================================================
# Producto Punto Paralelo - PE{pe_id} (CÓDIGO GENÉRICO REUTILIZABLE)
# ============================================================================
# Este código carga todas las constantes desde SHARED_CONFIG (addr 0-15)
# NO requiere regeneración al cambiar VECTOR_SIZE, NUM_PES, etc.
# Solo recompilar para actualizar el área de configuración compartida
#
# SHARED_CONFIG Layout:
#   [0] = VECTOR_A_ADDR
#   [1] = VECTOR_B_ADDR
#   [2] = RESULTS_ADDR
#   [3] = FLAGS_ADDR
#   [8+{pe_id}*2] = PE{pe_id}_START_INDEX
#   [9+{pe_id}*2] = PE{pe_id}_SEGMENT_SIZE
# ============================================================================

# ============================================================================
# CARGA DE CONSTANTES DESDE SHARED_CONFIG
# ============================================================================

# Cargar VECTOR_A_ADDR (addr 0)
MOV R4, {float(CFG_VECTOR_A_ADDR)}
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR (para cálculo de direcciones)

# Cargar VECTOR_B_ADDR (addr 1)
MOV R4, {float(CFG_VECTOR_B_ADDR)}
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

# Cargar start_index para PE{pe_id} (addr {cfg_start_idx_addr})
MOV R4, {float(cfg_start_idx_addr)}
LOAD R1, [R4]        # R1 = start_index (índice, no dirección)

# Cargar segment_size para PE{pe_id} (addr {cfg_segment_size_addr})
MOV R4, {float(cfg_segment_size_addr)}
LOAD R3, [R4]        # R3 = segment_size (contador del loop)

# ============================================================================
# INICIALIZACIÓN
# ============================================================================
MOV R0, 0.0         # R0 = acumulador (resultado parcial)

# ============================================================================
# LOOP: Procesar segment_size elementos
# ============================================================================
LOOP_START:
FADD R4, R5, R1     # R4 = VECTOR_A_ADDR + i (dirección de A[i])
LOAD R4, [R4]       # R4 = A[i]
FADD R6, R2, R1     # R6 = VECTOR_B_ADDR + i
LOAD R6, [R6]       # R6 = B[i]
FMUL R4, R4, R6     # R4 = A[i] * B[i]
FADD R0, R0, R4     # acumulador += producto
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Repetir si contador != 0

# ============================================================================
# GUARDAR RESULTADO Y SEÑALIZAR
# ============================================================================
# Resultado parcial: RESULTS_ADDR + pe_id (direccionamiento directo)
MOV R5, {float(RESULTS_ADDR + pe_id)}
STORE R0, [R5]      # Guardar resultado parcial

# Flag de sincronización: FLAGS_ADDR + pe_id
MOV R7, {float(FLAGS_ADDR + pe_id)}
MOV R6, 1.0         # Flag value
STORE R6, [R7]      # Señalizar finalización

HALT
"""
    return code

def generate_master_pe(pe_id=3):
    """Genera código assembly GENÉRICO para PE master (PE3)
    CARGA CONSTANTES DESDE SHARED_CONFIG
    USA MOV IMMEDIATE para BARRIER_CHECK y NUM_PES (optimización anti-thrashing)"""
    
    cfg_start_idx_addr = CFG_PE_START_ADDR + pe_id * 2
    cfg_segment_size_addr = CFG_PE_START_ADDR + pe_id * 2 + 1
    
    # Valores para MOV immediate (optimizados para evitar cache thrashing)
    barrier_check_value = -(NUM_PES - 1)  # e.g., -3.0 para NUM_PES=4
    num_pes_value = float(NUM_PES)
    
    code = f"""# ============================================================================
# Producto Punto Paralelo - PE{pe_id} (MASTER - CÓDIGO GENÉRICO REUTILIZABLE)
# ============================================================================
# Este código carga constantes desde SHARED_CONFIG (addr 0-15)
# OPTIMIZACIÓN: Usa MOV immediate para BARRIER_CHECK y NUM_PES
#   (evita cache thrashing durante barrier/reducción)
# ============================================================================

# ============================================================================
# FASE 1: Calcular producto parcial propio
# ============================================================================

# Cargar VECTOR_A_ADDR (addr 0)
MOV R4, {float(CFG_VECTOR_A_ADDR)}
LOAD R5, [R4]        # R5 = VECTOR_A_ADDR (para cálculo de direcciones)

# Cargar VECTOR_B_ADDR (addr 1)
MOV R4, {float(CFG_VECTOR_B_ADDR)}
LOAD R2, [R4]        # R2 = VECTOR_B_ADDR

# Cargar start_index para PE{pe_id} (addr {cfg_start_idx_addr})
MOV R4, {float(cfg_start_idx_addr)}
LOAD R1, [R4]        # R1 = start_index (índice, no dirección)

# Cargar segment_size para PE{pe_id} (addr {cfg_segment_size_addr})
MOV R4, {float(cfg_segment_size_addr)}
LOAD R3, [R4]        # R3 = segment_size (contador)

# Inicialización
MOV R0, 0.0         # R0 = acumulador

# LOOP: Procesar segmento propio
LOOP_START:
FADD R4, R5, R1     # R4 = VECTOR_A_ADDR + i (dirección de A[i])
LOAD R4, [R4]       # R4 = A[i]
FADD R6, R2, R1     # R6 = VECTOR_B_ADDR + i
LOAD R6, [R6]       # R6 = B[i]
FMUL R4, R4, R6     # R4 = A[i] * B[i]
FADD R0, R0, R4     # acumulador += producto
INC R1              # i++
DEC R3              # contador--
JNZ LOOP_START      # Repetir si contador != 0

# Guardar resultado parcial (direccionamiento directo)
MOV R5, {float(RESULTS_ADDR + pe_id)}
STORE R0, [R5]      # Guardar en RESULTS_ADDR + {pe_id}

# ============================================================================
# FASE 2: BARRIER - Esperar a que otros PEs terminen
# ============================================================================

WAIT_LOOP:
# Cargar flags directamente (1 solo bloque de caché)
MOV R1, {float(FLAGS_ADDR)}
LOAD R2, [R1]        # R2 = flag[PE0]

MOV R1, {float(FLAGS_ADDR + 1)}
LOAD R4, [R1]        # R4 = flag[PE1]

MOV R1, {float(FLAGS_ADDR + 2)}
LOAD R5, [R1]        # R5 = flag[PE2]

# Sumar flags
FADD R6, R2, R4      # R6 = flag0 + flag1
FADD R6, R6, R5      # R6 += flag2

# OPTIMIZACIÓN: MOV immediate para BARRIER_CHECK (evita cache miss)
MOV R7, {barrier_check_value}  # R7 = -(NUM_PES-1) = {barrier_check_value}
FADD R6, R6, R7      # R6 = suma_flags - (NUM_PES-1)
JNZ WAIT_LOOP        # Si != 0, repetir

# ============================================================================
# FASE 3: REDUCCIÓN - Sumar resultados parciales
# ============================================================================

# OPTIMIZACIÓN: MOV immediate para NUM_PES (evita cache miss)
MOV R2, {num_pes_value}  # R2 = NUM_PES = {num_pes_value} (contador)

MOV R1, {float(RESULTS_ADDR)}  # R1 = dirección actual (empieza en RESULTS_ADDR)
MOV R0, 0.0          # R0 = acumulador final

REDUCE_LOOP:
LOAD R4, [R1]        # R4 = resultado_parcial[i]
FADD R0, R0, R4      # acumulador += resultado_parcial[i]
INC R1               # R1++ (siguiente resultado: compacto, +1 dirección)
DEC R2               # contador--
JNZ REDUCE_LOOP      # Repetir si contador != 0

# ============================================================================
# Guardar resultado final
# ============================================================================
MOV R2, {float(FINAL_RESULT_ADDR)}
STORE R0, [R2]       # Guardar producto punto final

HALT
"""
    return code

def main():
    """Genera todos los archivos .asm"""
    
    print(f"Generando archivos assembly para:")
    print(f"  VECTOR_SIZE = {VECTOR_SIZE}")
    print(f"  NUM_PES = {NUM_PES}")
    print(f"  SEGMENT_SIZE_WORKER = {SEGMENT_SIZE_WORKER}")
    print(f"  SEGMENT_SIZE_MASTER = {SEGMENT_SIZE_MASTER}")
    print(f"  RESIDUE = {RESIDUE}")
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
