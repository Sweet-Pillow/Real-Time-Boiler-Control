    //Definição de Bibliotecas
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "time.h"
#include "tela.h"

//Monitores que leem e disponibilizam os dados dos sensores para as threads
#include "sensorfluxo.h"
#include "sensornivel.h"
#include "sensortemperatura.h"
#include "sensortemperaturaamb.h"
#include "sensortemperaturaentra.h"
#include "bufferDuplo.h"

//Monitor de acesso de conexão
#include "socket.h"
#include "referenciaT.h"

#define	NSEC_PER_SEC    (1000000000) 	// Numero de nanosegundos em um segundo
#define	N_AMOSTRAS	10000		// Numero de amostras coletadas
#define NUM_THREADS	9

// mutex para variavel nAmostras
static pthread_mutex_t excl_mutua = PTHREAD_MUTEX_INITIALIZER;
int nAmostras = 0;
int controlarT = 0;
int controlarH = 0;

long temp_exec[N_AMOSTRAS];		// Medicoes do tempo de execução da tarefa em microsegundos
static int terminar = 0;
static int valorLido = 0;

//Thread que exibe os valores do nível e Temperaturas na tela
void thread_mostra_status (void){
	double temperatura, nivel, fluxo, temp_amb, temp_entrada;
	while(1){
		
		double temperatura = sensor_get_temperatura();
		double nivel = sensor_get_nivel();
		double fluxo = sensor_get_fluxo();
		double temp_amb = sensor_get_temperatura_ambiente();
		double temp_ent = sensor_get_temperatura_entrada();
		
		aloca_tela();//Permite acesso exclusivo dos recursos para a tela do computador
		system("tput reset"); //limpa tela
		printf("---------------------------------------\n");
		printf("Temperatura (T)--> %.4lf\n", temperatura);
		printf("Nivel (H)--> %.4lf\n", nivel);
		
		printf("Fluxo (No)--> %.4lf\n", fluxo);
		printf("Temperatura Ambiente (Ta)--> %.4lf\n", temp_amb);
		printf("Temperatura Entrada (Ti)--> %.4lf\n", temp_ent);
		printf("\n---------------------------------------\n");
		printf("\n-> Total Amostras --> %d \n", nAmostras);
        printf("\n-> Temperatura de Ref: %.4lf \n", ref_getT());
		printf("\n-> Altura de Ref: %.4lf \n", ref_getH());
		
		printf("---------------------------------------\n");
		libera_tela();//Libera os recursos 
		sleep(1); //Executada a cada 1 segundo
	}		
		
}

void thread_le_sensor (void){ //Le Sensores periodicamente a cada 10ms
	char msg_enviada[1000];	
	struct timespec t, t_fim;
	long periodo = 10e6;

	clock_gettime(CLOCK_MONOTONIC, &t);

	while(1){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
		
		sensor_put_temperatura(msg_socket("st-0")); 			// ler o sensor de temperatura da caldeira
		sensor_put_nivel(msg_socket("sh-0"));					// Ler o sensor de altura da caldeira
		sensor_put_temperatura_ambiente(msg_socket("sta0"));	// Ler o sensor da Temperatura Ambiente
		sensor_put_fluxo(msg_socket("sno0"));					// Ler o sensor do fluxo de saida kg/s
		sensor_put_temperatura_entrada(msg_socket("sti0"));		// Ler o sensor de temperatura da água que entra na caldeira

		t.tv_nsec += periodo;
		while(t.tv_nsec>= NSEC_PER_SEC){
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_nsec ++;
		}
			
	}		
}

//aciona alarme na temperatura definida 
void thread_alarme (void){
	
	while(1){
		sensor_alarmeT(30); //Definindo a temperatura
		aloca_tela();
		printf("ALARME, TEMPERATURA LIMITE ATINGIDA !!!\n");
		libera_tela();
		sleep(1);	
	}
		
}

void thread_controle_temperatura(void){

	char msg_enviada[1000];
	long atraso_fim;
	struct timespec t, t_fim;
	long periodo = 50e6; //50ms
	double ta, tr, na, ni,q;

	clock_gettime(CLOCK_MONOTONIC ,&t);

	while(1){

		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		ta = sensor_get_temperatura();
		tr = ref_getT();

		
    	if(controlarT == 1){
    		controlarH == 1;
    		
    		if(ta < tr){ // esquenta
    			sprintf( msg_enviada, "aq-%lf", 1000000.0);
	        		msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ana%lf", 5.0);
	        			msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ani%lf", sensor_get_fluxo() - 5.0);
	        			msg_socket(msg_enviada);
    			
			}else{ // esfriar
				sprintf( msg_enviada, "aq-%lf", 0.0);
	        		msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ana%lf", 0.0);
	        			msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ani%lf", sensor_get_fluxo());
	        			msg_socket(msg_enviada);	
			}
		}

		// Le a hora atual, coloca em t_fim
		clock_gettime(CLOCK_MONOTONIC ,&t_fim);

		// Calcula o tempo de resposta observado em microsegundos
		atraso_fim = 1000000*(t_fim.tv_sec - t.tv_sec)   +   (t_fim.tv_nsec - t.tv_nsec)/1000;

		bufduplo_insereLeitura(atraso_fim);

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC) {
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}

	}
}


void thread_grava_temp_resp(void){
    FILE* dados_f;
	dados_f = fopen("tempo_resposta_ctrl_tmp.txt", "w");
	
    if(dados_f == NULL){
        printf("Erro, nao foi possivel abrir o arquivo\n");
        exit(1);
    }
    
	int amostras = 1;
	
	while(amostras++ <= N_AMOSTRAS/200){
		
		long * buf = bufduplo_esperaBufferCheio();
		int n2 = tamBuf();
		int tam = 0;
		
		while(tam < n2){
			fprintf(dados_f, "%4ld\n", buf[tam++]);	
		}
			
		fflush(dados_f);
		
		pthread_mutex_lock( &excl_mutua);
			nAmostras += n2; // armazena a quantidade de medicoes já gravadas no arquivo (amostras) para depois ser mostrada na thread "mostra status"
		pthread_mutex_unlock( &excl_mutua);
		
		aloca_tela();
			printf("\n [-- Info --] Gravando no arquivo...\n");
		libera_tela();
	}

	fclose(dados_f);	
}

void thread_grava_sensor_nivel(void){
    FILE* dados_f;
	dados_f = fopen("dados_nivel.txt", "w");
	
	if(dados_f == NULL){
		printf("Erro, nao foi possivel abrir o arquivo\n");
		exit(1);
	}
	while(terminar == 0){
		fprintf(dados_f, "%4ld\n", sensor_get_nivel());
		sleep(1); //Executada a cada 1 segundo
	}
	
	fclose(dados_f);	
}

void thread_grava_sensor_temperatura(void){
    FILE* dados_f;
	dados_f = fopen("dados_temperatura.txt", "w");
	
	if(dados_f == NULL){
		printf("Erro, nao foi possivel abrir o arquivo\n");
		exit(1);
	}
	while(terminar == 0){
		fprintf(dados_f, "%4ld\n", sensor_get_nivel());
		sleep(1); //Executada a cada 1 segundo
	}
	
	fclose(dados_f);	
}

void thread_altera_referencia(){
	char tbuffer[10];
	
	while(terminar == 0){
		int modoEntrada;
		int lido;
		
		fgets(tbuffer,10,stdin);
		
		aloca_tela();
		
		modoEntrada = 1;
		
		while(modoEntrada){
			printf("---[ 'c' para cancelar ou 'x' para sair do programa]---\n\n");
			printf("-> Digite um novo valor para a temperatura de referência: ");
			fgets(tbuffer,2000,stdin);
			
			if(tbuffer[0] == 'c'){
				break;
			}		
			if(tbuffer[0] == 'x'){
				terminar = 1;
				break;
			}
			lido = atoi(tbuffer);
			ref_putT(lido);
			
			printf("-> Digite um novo valor para a altura de referência: ");
			fgets(tbuffer,2000,stdin);
			
			if(tbuffer[0] == 'c'){
				break;
			}		
			if(tbuffer[0] == 'x'){
				terminar = 1;
				break;
			}
			lido = atoi(tbuffer);
			ref_putH(lido);	
            break;
		}
        libera_tela();
	}
}

// A pertubacao no sensor No é feita após:
/*
	- 200 amostras
	- 1000 amostras
	- 3000 amostras
	- 6000 amostras
	- 8000 amostras
*/


/* Controlar nivel da caldeira */
void thread_controle_nivel(){
	struct timespec t;
	long int periodo = 70e6; 	// 70ms
	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC ,&t);
	t.tv_sec++;

	while(1){	
		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
  
        double no = sensor_get_fluxo();
        double ha = sensor_get_nivel();
        double hr = ref_getH();
        
        if(ha < hr){ // aumenta a altura
        	controlarH = 1;
        	controlarT = 0;
        	sprintf( msg_enviada, "ani%lf", 100.0);
	        msg_socket(msg_enviada);
	        
	        sprintf( msg_enviada, "anf%lf", 0.0);
	        msg_socket(msg_enviada);
	        
	        sprintf( msg_enviada, "ana%lf", 10.0);
	        msg_socket(msg_enviada);
				
    	}
    	if(ha == hr){
    		controlarT = 1;
    		if(controlarH = 0){
    			sprintf( msg_enviada, "ani%lf", no);
        		msg_socket(msg_enviada);
			}
    		
        	sprintf( msg_enviada, "ana%lf", 0.0);
        	msg_socket(msg_enviada);
        	sprintf( msg_enviada, "anf%lf", 0.0);
        	msg_socket(msg_enviada);		
		}
		
		if(ha > hr){ // diminui a altura
			controlarH = 1;
			controlarT = 0;
        	if( no > 50){
        		sprintf( msg_enviada, "ani%lf", 0.0);
	        	msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ana%lf", 0.0);
	        	msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "anf%lf", 10.0);
	        	msg_socket(msg_enviada);
			}else{
				sprintf( msg_enviada, "ani%lf", 0.0);
	        	msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ana%lf", 0.0);
	        	msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "anf%lf", 0.0);
	        	msg_socket(msg_enviada);
			}
    	}

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC) {
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}	
}

void main( int argc, char *argv[]) {

	//temperatura de referência
    ref_putT(20);
    ref_putH(2);
    cria_socket(argv[1], atoi(argv[2]) ); 
    
	int ord_prio[NUM_THREADS]={80,81,82,99,83,84,85,86,99};
	pthread_t threads[NUM_THREADS];
	pthread_attr_t pthread_custom_attr[NUM_THREADS];
	struct sched_param priority_param[NUM_THREADS];

	int i;
	//Configura escalonador do sistema
	for(i=0;i<NUM_THREADS;i++){
		pthread_attr_init(&pthread_custom_attr[i]);
		pthread_attr_setscope(&pthread_custom_attr[i], PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setinheritsched(&pthread_custom_attr[i], PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&pthread_custom_attr[i], SCHED_FIFO);
		priority_param[i].sched_priority = ord_prio[i];
		if (pthread_attr_setschedparam(&pthread_custom_attr[i], &priority_param[i])!=0)
	  		fprintf(stderr,"pthread_attr_setschedparam failed\n");
	}

	pthread_create(&threads[0],  &pthread_custom_attr[0], (void *) thread_mostra_status, NULL);  // concluido
	pthread_create(&threads[1],  &pthread_custom_attr[1], (void *) thread_le_sensor, NULL); // concluido
	pthread_create(&threads[2],  &pthread_custom_attr[2], (void *) thread_alarme, NULL); // concluido
	pthread_create(&threads[3],  &pthread_custom_attr[3], (void *) thread_controle_temperatura, NULL);
	pthread_create(&threads[4],  &pthread_custom_attr[4], (void *) thread_grava_temp_resp, NULL);
	pthread_create(&threads[5],  &pthread_custom_attr[5], (void *) thread_grava_sensor_nivel, NULL);
	pthread_create(&threads[6],  &pthread_custom_attr[6], (void *) thread_grava_sensor_temperatura, NULL);
	pthread_create(&threads[7],  &pthread_custom_attr[7], (void *) thread_altera_referencia, NULL);
	pthread_create(&threads[8],  &pthread_custom_attr[8], (void *) thread_controle_nivel, NULL);
	//pthread_create(&threads[9],  &pthread_custom_attr[9], (void *) thread_pertubacao, NULL);

	int j;
	
	pthread_join( threads[7], NULL);	
}
