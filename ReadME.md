Compilar:
```bash
gcc inicializador.c -o inicializador -lrt -lpthread
gcc emisor.c -o emisor -lrt -lpthread
gcc receptor.c -o receptor -lrt -lpthread
gcc finalizador.c -o finalizador -lrt -lpthread
```

Ejecutar:
```bash
./build/inicializador
./build/emisor <shm_id> <modo> <clave> <num_emisores>
./build/receptor <shm_id> <modo> <clave> <num_receptores>
./build/finalizador <shm_id>
```

Ver los recursos creados
```bash
ls -l /dev/shm

# Borrar la memoria
rm /dev/shm/<shm_id>

# Borrar los semáforos
rm /dev/shm/sem.<shm_id>_mutex
rm /dev/shm/sem.<shm_id>_empty
rm /dev/shm/sem.<shm_id>_full
```