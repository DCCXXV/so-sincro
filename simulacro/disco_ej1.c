/*
 *  Marco Antonio Pérez Neira
 */
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define CAPACITY 5
#define VIPSTR(vip) ((vip) ? "  vip  " : "not vip")

typedef struct {
    int id;
    int is_vip;
}t_cliente;

// recordar inicializar y destruir en el main SIEMPRE
pthread_mutex_t mu;

// una variable condicional por cada "tipo" de usuario
pthread_cond_t sala_espera_normales;
pthread_cond_t sala_espera_vips;

int numero_clientes_normales_cola = 0;
int numero_clientes_vips_cola = 0;

int turno_actual_normales = 0;
int turno_actual_vips = 0;

int numero_clientes_bailando = 0;

void enter_vip_client(int id) {
    pthread_mutex_lock(&mu); 
    numero_clientes_vips_cola++;
    // Esperar si:
    // 1. No es mi turno
    // 2. Esta lleno
    while (id != turno_actual_vips 
             || numero_clientes_bailando >= CAPACITY) {
	    printf("#  Cliente esperando:   vip   con id:%2d\n", id);
        pthread_cond_wait(&sala_espera_vips, &mu);
    }
    numero_clientes_bailando++;
    numero_clientes_vips_cola--;
    turno_actual_vips++;
    printf("-> Cliente entra    :   vip   con id: %d\n", id);
    pthread_mutex_unlock(&mu);
}

void enter_normal_client(int id) {
    pthread_mutex_lock(&mu); 
    numero_clientes_normales_cola++;
    // Esperar si:
    // 1. Hay vips primero
    // 2. No es mi turno
    // 3. Esta lleno
    while (numero_clientes_vips_cola != 0 
            || id != turno_actual_normales 
            || numero_clientes_bailando >= CAPACITY) {
	    printf("#  Cliente esperando: not vip con id:%2d\n", id);
        pthread_cond_wait(&sala_espera_normales, &mu);
    }
    numero_clientes_bailando++;
    numero_clientes_normales_cola--;
    turno_actual_normales++;
	printf("-> Cliente entra    : not vip con id:%2d\n", id);
    pthread_mutex_unlock(&mu);
}

void dance(int id, int isvip) {
	printf("&  Cliente bailando : %s con id:%2d\n", VIPSTR(isvip), id);
	sleep((rand() % 5) + 1);
}

void disco_exit(int id, int isvip) {
    pthread_mutex_lock(&mu); 
    numero_clientes_bailando--;
    printf("<- Cliente fuera    : %s con id: %d\n", VIPSTR(isvip), id);
    if (numero_clientes_vips_cola > 0) {
        pthread_cond_broadcast(&sala_espera_vips); 
    }
    else if(numero_clientes_normales_cola > 0) {
        pthread_cond_broadcast(&sala_espera_normales);
    }
    pthread_mutex_unlock(&mu); 
}

void *client(void *arg) {
    t_cliente *cliente = (t_cliente*) arg;
    if (cliente->is_vip) {
        enter_vip_client(cliente->id);
    }
    else {
        enter_normal_client(cliente->id);
    }
    dance(cliente->id, cliente->is_vip);
    disco_exit(cliente->id, cliente->is_vip);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pthread_mutex_init(&mu, NULL);
    pthread_cond_init(&sala_espera_normales, NULL);
    pthread_cond_init(&sala_espera_vips, NULL);

    if (argc < 2) {
        fprintf(stderr, "Uso %s <archivo_entrada>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo el archivo");
        exit(EXIT_FAILURE);
    }

    int total_cola;
    char num_str_buffer[12];
    char current_char;
    int buffer_idx = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, &current_char, 1)) == 1) {
        if (current_char == '\n') {
            break;
        }
        if (buffer_idx < sizeof(num_str_buffer) - 1 && isdigit(current_char)) {
            num_str_buffer[buffer_idx++] = current_char;
        } else {
            fprintf(stderr, "Error: Formato inválido para total_cola (carácter no numérico).\n");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read <= 0 || buffer_idx == 0) {
        fprintf(stderr, "Error leyendo total_cola o archivo vacío/formato incorrecto.\n");
        if (bytes_read < 0) perror("Error de lectura");
        close(fd);
        exit(EXIT_FAILURE);
    }
    num_str_buffer[buffer_idx] = '\0';
    total_cola = atoi(num_str_buffer);

    if (total_cola <= 0) {
        fprintf(stderr, "Error: total_cola debe ser positivo (leido: %d).\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    t_cliente *clientes = malloc(total_cola * sizeof(t_cliente));
    pthread_t *hilos = malloc(total_cola * sizeof(pthread_t));

    if (clientes == NULL || hilos == NULL) {
        perror("Error alocando memoria");
        close(fd);
        if (clientes) free(clientes);
        if (hilos) free(hilos);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < total_cola; i++) {
        if (read(fd, &current_char, 1) != 1) {
            fprintf(stderr, "Error leyendo is_vip para cliente %d (EOF o error de lectura).\n", i);
            close(fd);
            free(clientes);
            free(hilos);
            exit(EXIT_FAILURE);
        }

        if (current_char == '0') {
            clientes[i].is_vip = 0;
            clientes[i].id = turno_actual_normales++;
        } else if (current_char == '1') {
            clientes[i].is_vip = 1;
            clientes[i].id = turno_actual_vips++;
        }

        read(fd, &current_char, 1); // leer \n

        printf("Cliente leído: %s con id %d\n", VIPSTR(clientes[i].is_vip), clientes[i].id);
    }
    turno_actual_normales = 0;
    turno_actual_vips = 0;
    
    close(fd);
    
    // siempre un bucle para crearlos a todos
    for (int i = 0; i < total_cola; i++) {
        pthread_create(&hilos[i], NULL, client, (void*) &clientes[i]);
    }
    // y otro para el join IMPORTANTE
    for (int i = 0; i < total_cola; i++) {
        pthread_join(hilos[i], NULL);
    }

    free(clientes);
    free(hilos);
    pthread_cond_destroy(&sala_espera_normales);
    pthread_cond_destroy(&sala_espera_vips);
    pthread_mutex_destroy(&mu);
	return 0;
}
