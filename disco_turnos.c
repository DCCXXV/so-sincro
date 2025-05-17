#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define CAPACITY 5
#define VIPSTR(vip) ((vip) ? "  vip  " : "not vip")

/** 
 *  Esta implementación añade un atributo para el turno en vez de usar el id del cliente
 */
typedef struct {
    int id;
    int is_vip;
    int turno;
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

void enter_vip_client(int id, int turno) {
    pthread_mutex_lock(&mu); 
    numero_clientes_vips_cola++;
    // Esperar si:
    // 1. No es mi turno
    // 2. Esta lleno
    while (turno != turno_actual_vips 
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

void enter_normal_client(int id, int turno) {
    pthread_mutex_lock(&mu); 
    numero_clientes_normales_cola++;
    // Esperar si:
    // 1. Hay vips primero
    // 2. No es mi turno
    // 3. Esta lleno
    while (numero_clientes_vips_cola != 0 
            || turno != turno_actual_normales 
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
        enter_vip_client(cliente->id, cliente->turno);
    }
    else {
        enter_normal_client(cliente->id, cliente->turno);
    }
    dance(cliente->id, cliente->is_vip);
    disco_exit(cliente->id, cliente->is_vip);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pthread_mutex_init(&mu, NULL);
    pthread_cond_init(&sala_espera_normales, NULL);
    pthread_cond_init(&sala_espera_vips, NULL);

    FILE *fichero = fopen(argv[1], "r");
    if (fichero == NULL) {
        perror("Error abriendo el archivo.");
        exit(EXIT_FAILURE);
    }
    
    int total_cola;
    fscanf(fichero, "%d", &total_cola);
    
    t_cliente clientes[total_cola];
    pthread_t hilos[total_cola];
    
    for (int i = 0; i < total_cola; i++) {
        fscanf(fichero, "%d", &clientes[i].is_vip); 
        if (clientes[i].is_vip) {
            clientes[i].turno = turno_actual_vips++;
        } else {
            clientes[i].turno = turno_actual_normales++;
        }
        clientes[i].id = i;
        printf("Cliente leído: %s con id: %d\n", VIPSTR(clientes[i].is_vip), clientes[i].id);
    }

    turno_actual_normales = 0;
    turno_actual_vips = 0;
    
    printf("TOTAL-------------------------%2d\n", total_cola);
    
    // siempre un bucle para crearlos a todos
    for (int i = 0; i < total_cola; i++) {
        pthread_create(&hilos[i], NULL, client, (void*) &clientes[i]);
    }
    // y otro para el join IMPORTANTE
    for (int i = 0; i < total_cola; i++) {
        pthread_join(hilos[i], NULL);
    }

    pthread_cond_destroy(&sala_espera_normales);
    pthread_cond_destroy(&sala_espera_vips);
    pthread_mutex_destroy(&mu);
	return 0;
}
