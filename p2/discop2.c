#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>

#define CAPACITY 5
#define VIPSTR(vip) ((vip) ? "  vip  " : "not vip")

FILE* salida;

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

void escribir_salida(const char *msg) {
    if (salida == stdout) {
        fputs(msg, stdout);
    } else {
        fwrite(msg, 1, strlen(msg), salida);
    }
}

void enter_vip_client(int id) {
    pthread_mutex_lock(&mu); 
    numero_clientes_vips_cola++;
    // Esperar si:
    // 1. No es mi turno
    // 2. Esta lleno
    while (id != turno_actual_vips 
             || numero_clientes_bailando >= CAPACITY) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "#  Cliente esperando:   vip   con id:%2d\n", id);
        escribir_salida(buffer);
        pthread_cond_wait(&sala_espera_vips, &mu);
    } numero_clientes_bailando++;
    numero_clientes_vips_cola--;
    turno_actual_vips++;
    fprintf(salida, "-> Cliente entra    :   vip   con id: %d\n", id);
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
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "#  Cliente esperando: not vip con id:%2d\n", id);
        escribir_salida(buffer);
        pthread_cond_wait(&sala_espera_normales, &mu);
    }
    numero_clientes_bailando++;
    numero_clientes_normales_cola--;
    turno_actual_normales++;
	fprintf(salida, "-> Cliente entra    : not vip con id:%2d\n", id);
    pthread_mutex_unlock(&mu);
}

void dance(int id, int isvip) {
	fprintf(salida, "&  Cliente bailando : %s con id:%2d\n", VIPSTR(isvip), id);
	sleep((rand() % 5) + 1);
}

void disco_exit(int id, int isvip) {
    pthread_mutex_lock(&mu); 
    numero_clientes_bailando--;
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "<- Cliente fuera    : %s con id: %d\n", VIPSTR(isvip), id);
    escribir_salida(buffer);
    if (numero_clientes_vips_cola > 0) {
        pthread_cond_signal(&sala_espera_vips); 
    }
    else if(numero_clientes_normales_cola > 0) {
        pthread_cond_signal(&sala_espera_normales);
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

void usage(char* progname) {
    printf("Uso: %s -f <file_name> \n", progname);
    printf("Opciones:\n");
    printf("\t-f <file_name>: especificar el archivo de entrada\n");
    printf("\t-o <file_name>: especificar el archivo de salida\n");
    printf("\t-h: mostrar este mensaje\n");
    printf("\t<nombre_discoteca>: mostrará el nombre de la discoteca al inicio");
}

int main(int argc, char *argv[]) {
    pthread_mutex_init(&mu, NULL);
    pthread_cond_init(&sala_espera_normales, NULL);
    pthread_cond_init(&sala_espera_vips, NULL);
    
    int opt;
    FILE* entrada;
    salida = stdout;
    
    while ((opt = getopt(argc, argv, "i:o:h")) != -1) {
        switch(opt) {
            case 'i':
                entrada=fopen(optarg, "r");
                if (entrada == NULL) {
                    perror("Error abriendo el archivo de entrada.");
                    exit(EXIT_FAILURE);
                }

                int total;
                char char_actual;
                char num_str_buffer[12];
                int buffer_idx = 0;
                ssize_t bytes_read;

                while ((bytes_read = fread(&char_actual, sizeof(char_actual), 1, entrada) == 1)) {
                    if (char_actual == '\n') {
                        break;
                    }
                    if (buffer_idx < sizeof(num_str_buffer) && isdigit(char_actual)) {
                        num_str_buffer[buffer_idx++] = char_actual;
                    } else {
                        perror("Error al procesar el archivo de entrada");
                        fclose(entrada);
                        exit(EXIT_FAILURE);
                    }
                }

                if (buffer_idx == 0) {
                    perror("Error al profcesar el archivo de entrada");
                    fclose(entrada);
                    exit(EXIT_FAILURE);
                }

                num_str_buffer[buffer_idx] = '\0';
                total = atoi(num_str_buffer);

                t_cliente *clientes = malloc(total * sizeof(t_cliente));
                pthread_t *hilos = malloc(total * sizeof(pthread_t));

                for (int i = 0; i < total; i++) {
                    if ((bytes_read = fread(&char_actual, sizeof(char_actual), 1, entrada)) == 1) {
                        if (char_actual == '0') {
                            // no es vip
                            clientes[i].id = turno_actual_normales++;
                            clientes[i].is_vip = 0;
                        } else {
                            // es vip
                            clientes[i].id = turno_actual_vips++;
                            clientes[i].is_vip = 1;
                        }
                    } else {
                        perror("Error al profcesar el archivo de entrada");
                        free(clientes); free(hilos);
                        fclose(entrada);
                        exit(EXIT_FAILURE);
                    }
                    fprintf(salida, "Cliente leído: %s con id: %d\n", VIPSTR(clientes[i].is_vip), clientes[i].id);
                    fread(&char_actual, sizeof(char_actual), 1, entrada); // leer el salto de linea
                }

                turno_actual_normales = 0;
                turno_actual_vips = 0;

                // siempre un bucle para crearlos a todos
                for (int i = 0; i < total; i++) {
                    pthread_create(&hilos[i], NULL, client, (void*) &clientes[i]);
                }
                // y otro para el join IMPORTANTE
                for (int i = 0; i < total; i++) {
                    pthread_join(hilos[i], NULL);
                }

                break;
            case 'o':
                salida = fopen(optarg, "w");
                if (salida == NULL) {
                    perror("Error abriendo el archivo de salida.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    if (!entrada) {
        perror("Error es necesario proveer un archivo mediante -i <archivo>");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    fclose(entrada);
    if (salida != stdout) fclose(salida);

    pthread_cond_destroy(&sala_espera_normales);
    pthread_cond_destroy(&sala_espera_vips);
    pthread_mutex_destroy(&mu);
	return 0;
}
