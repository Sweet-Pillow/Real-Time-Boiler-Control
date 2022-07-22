//Definição de Bibliotecas
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "socket.h"
#include "sensores.h"
#include "tela.h"
#include "bufduplo.h"
#include "bufduploT.h"
#include "bufduploN.h"
#include "referenciaT.h"
#include "referenciaN.h"


#define	NSEC_PER_SEC    (1000000000) 	// Numero de nanosegundos em um segundo
#define NUM_THREADS	9		// Quantidade de thread criadas
#define N_AMOSTRAS 	10000		// Quantidade de amostras coletadas

//Cores para estilizar as informações apresentadas no console
#define VERMELHO   	"\x1B[31m"
#define VERDE   	"\x1B[32m"
#define AMARELO   	"\x1B[33m"
#define MAGENTA   	"\x1B[35m"
#define RESET 		"\x1B[0m"


//Thread que exibi os valores dos sensores no console e atualiza o console
void thread_mostra_status (void){
	double t, h, ta, ti, no;
	while(1){
		t = sensor_get("t");
		h = sensor_get("h");
		ta = sensor_get("ta");
		ti = sensor_get("ti");
		no = sensor_get("no");
		aloca_tela();
		system("tput reset");
		printf(AMARELO "---------------------------------------\n" RESET);
		printf(VERDE "Referencia temperatura--> %.2lf\n", ref_getT());
		printf("Referencia nivel--> %.2lf\n", ref_getN());
		printf("Temperatura (T)--> %.2lf\n", t);
		printf("Nivel (H)--> %.2lf\n", h);
		printf("Temperatura do ambiente (Ta)--> %.2lf\n", ta);
		printf("Temperatura de entrada da agua(Ti)--> %.2lf\n", ti);
		printf("Fluxo de agua de saida (No)--> %.2lf\n" RESET, no);
		printf(AMARELO "---------------------------------------\n" RESET);
		libera_tela();
		sleep(1);
	}

}

//Thread que exibi uma mensagem de alarme caso a Temperatura chegue a 30 graus Celsius
void thread_alarme (void){
	while(1){
		sensor_alarmeT(30);
		aloca_tela();
		printf(VERMELHO "ALARME\n" RESET);
		libera_tela();
		sleep(1);
	}

}

///Thread que controla a temperatura e adiciona o valor dos tempos de resposta em um buffer
void thread_controle_temperatura (void){
	char msg_enviada[1000];
	long atraso_fim;
	struct timespec t, t_fim;
	long periodo = 50e6; //50ms
	double temp, ref_temp;
	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC ,&t);

	while(1){

		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		temp = sensor_get("t");
		ref_temp = ref_getT();
		
	    	if(temp > ref_temp) { //Diminui temperatura
	    	
			sprintf( msg_enviada, "ana%lf", 0.0 );
			msg_socket(msg_enviada);
			
			if (sensor_get("ti") < ref_temp){
				sprintf( msg_enviada, "ani%lf", ref_temp * (100.0 - ((temp) * 100.0 / ref_temp)));
				msg_socket(msg_enviada);
				
				sprintf( msg_enviada, "anf%lf", ref_temp * (100.0 - ((temp) * 100.0 / ref_temp)));
				msg_socket(msg_enviada);
			} else {
				sprintf( msg_enviada, "anf%lf", ref_temp * (100.0 - ((temp) * 100.0 / ref_temp)));
				msg_socket(msg_enviada);
			}	
		}


		if(temp <= ref_temp) {    //Aumenta temperatura
			
			sprintf( msg_enviada, "ana%lf", 10.0);
			msg_socket(msg_enviada);
			sprintf( msg_enviada, "aq-%lf", 10000.0 * (100.0 - ((temp) * 100.0 / ref_temp)));
			msg_socket(msg_enviada);
            sprintf( msg_enviada, "anf%lf", 10.0);
			msg_socket(msg_enviada);
			
			if (sensor_get("ti") + 5 > ref_temp){
				sprintf( msg_enviada, "ani%lf", ref_temp * ((temp) * 100.0 / ref_temp));
				msg_socket(msg_enviada);
				
			} else {
				sprintf( msg_enviada, "ani%lf", 0.0);
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

//Thread que controla o nivel da água
void thread_controle_nivel(void){
	char msg_enviada[1000];
	long atraso_fim;
	struct timespec t, t_fim;
	long periodo = 70e6; //70ms
	double nivel, ref_nivel, no;

	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC ,&t);
	
	while (1){
		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		nivel = sensor_get("h");
		ref_nivel = ref_getN();
		no = sensor_get("no");
		
		 
		if (nivel > ref_nivel){ //Diminuir nivel
				
			sprintf( msg_enviada, "anf%lf", 100.0);
	        msg_socket(msg_enviada);
	        	sprintf( msg_enviada, "ani%lf", 00.0);
	        msg_socket(msg_enviada);
			
		}
		
		if (nivel <= ref_nivel){ //Aumentar nivel
            
            if (no > 10.0){
                    sprintf( msg_enviada, "ani%lf", no);
	            msg_socket(msg_enviada);
			        sprintf( msg_enviada, "anf%lf", 00.0);
	            msg_socket(msg_enviada);
            }
			
			    sprintf( msg_enviada, "ani%lf", 100.0);
	        msg_socket(msg_enviada);
			    sprintf( msg_enviada, "anf%lf", 00.0);
	        msg_socket(msg_enviada);
	        
		}
		
		
		// Le a hora atual, coloca em t_fim
		clock_gettime(CLOCK_MONOTONIC ,&t_fim);

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC) {
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

//Thread que lê todos os sensores a cada 10ms
void thread_le_sensor (void){ 
	char msg_enviada[1000];
	struct timespec t, t_fim;
	long periodo = 10e6; //10e6ns ou 10ms

	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC ,&t);
	while(1){
		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		//Envia mensagem via canal de comunicação para receber valores de sensores
		sensor_put(msg_socket("st-0"), msg_socket("sh-0"), msg_socket("sta0"), msg_socket("sti0"), msg_socket("sno0"));

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC) {
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

//Thread usada para ler os valores de nivel e temperatura no intervalo de 1 segundo e salva-los nos seus respectivos buffers
void thread_le_nivel_temp(void){
	struct timespec t;
	long periodo = 1e9; 
	double temp, nivel;
	
	clock_gettime(CLOCK_MONOTONIC ,&t);
	
	while (1){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
		
		temp = sensor_get("t");
		nivel = sensor_get("h");
		
		bufduplo_insereLeituraT(temp);
		bufduplo_insereLeituraN(nivel);
		
		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC) {
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

//Thread que grava no arquivo dados.txt os valores salvos no buffer da thread de controle de temperatura  
void thread_grava_temp_resp(void){
	FILE* dados_f;
	dados_f = fopen("dados.txt", "w");
	
	if(dados_f == NULL){
		printf("Erro, nao foi possivel abrir o arquivo dados.txt\n");
		exit(1);
	}
	
	int amostras = 1;
	while(amostras++<=N_AMOSTRAS/200){
		long * buf = bufduplo_esperaBufferCheio();
		int n2 = tamBuf();
		int tam = 0;
		
		while(tam<n2)
			fprintf(dados_f, "%4ld\n", buf[tam++]);
		fflush(dados_f);
		aloca_tela();
		printf("Gravando no arquivo...\n");
		libera_tela();

	}

	fclose(dados_f);
}

//Thread que descarrega os valores do buffer de temperatura e salva no arquivo temperatura.txt
void thread_gravar_temp(void){
		
	FILE* dados_temp;
	dados_temp = fopen("temperatura.txt", "w");
	
	if(dados_temp == NULL){
		printf("Erro, nao foi possivel abrir o arquivo temperatura.txt\n");
		exit(1);
	}
	
	int amostras = 1;
	while(amostras++<=N_AMOSTRAS/5){
		float * buf = bufduplo_esperaBufferCheioT();
		int n2 = tamBufT();
		int tam = 0;
		while(tam<n2)
			fprintf(dados_temp, "%4lf\n", buf[tam++]);
		fflush(dados_temp);
		aloca_tela();
		printf("Gravando no arquivo temperatura...\n");

		libera_tela();

	}

	fclose(dados_temp);
}

//Thread que descarrega os valores do buffer de nivel e salva no arquivo nivel.txt
void thread_gravar_nivel(void){

	FILE* dados_nivel;
	dados_nivel = fopen("nivel.txt", "w");
	
	if(dados_nivel == NULL){
		printf("Erro, nao foi possivel abrir o arquivo nivel.txt\n");
		exit(1);
	}
	
	int amostras = 1;
	while(amostras++<=N_AMOSTRAS/5){
		float * buf = bufduplo_esperaBufferCheioN();
		int n2 = tamBufN();
		int tam = 0;
		while(tam<n2)
			fprintf(dados_nivel, "%4lf\n", buf[tam++]);
		fflush(dados_nivel);
		aloca_tela();
		printf("Gravando no arquivo nivel...\n");

		libera_tela();

	}

	fclose(dados_nivel);
}

//Thread para receber os valores iniciais de referencia de temperatura e nivel, definidos pelo usuario 
void parametros_iniciais(void){
	int aux = 1;	
	double ref_temp = 0;
	double ref_nivel = 0;
	
	printf(MAGENTA "Digite um valor de referencia para a Temperatura:\n" RESET);
	scanf("%lf", &ref_temp);
	ref_putT(ref_temp);
	
	while (aux){
		printf(MAGENTA "Digite um valor de referencia para o nivel de agua(Valor entre 0.1 a 3.0):\n" RESET);
		scanf("%lf", &ref_nivel);
		
		if (ref_nivel >= 0.1 && ref_nivel <= 3.0){
			aux = 0;	
		}
	}
	ref_putN(ref_nivel);
}


void main( int argc, char *argv[]) {

	cria_socket(argv[1], atoi(argv[2]));			// Cria o socket para fazer a comunicação entre os processos

	int ord_prio[NUM_THREADS]={1,59,1,99,1,99,59,1,1}; 	// Vetor que define a prioridade de cada thread criada respectivamente
	pthread_t threads[NUM_THREADS];				// Vetor de threads
	pthread_t ler_parametros;				// Thread definida isolada, pois só será usada uma vez
	pthread_attr_t pthread_custom_attr[NUM_THREADS];	// Configura atributos para threads
	struct sched_param priority_param[NUM_THREADS];		// Configura a prioridade de execução das threads

	//Configura escalonador do sistema
	for(int i=0;i<NUM_THREADS;i++){
		pthread_attr_init(&pthread_custom_attr[i]);
		pthread_attr_setscope(&pthread_custom_attr[i], PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setinheritsched(&pthread_custom_attr[i], PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&pthread_custom_attr[i], SCHED_FIFO);
		priority_param[i].sched_priority = ord_prio[i];
		if (pthread_attr_setschedparam(&pthread_custom_attr[i], &priority_param[i])!=0)
	  		fprintf(stderr,"pthread_attr_setschedparam failed\n");
	}
	
	//A thread só será executada no inicio da aplicação, por conta disso ela não esta junto com as outras threads criadas posteriormente
	pthread_create(&ler_parametros, NULL, (void *) parametros_iniciais, NULL);
	pthread_join(ler_parametros, NULL);
	
	//Threads usadas na aplicação
	pthread_create(&threads[0], &pthread_custom_attr[0], (void *) thread_mostra_status, NULL);
	pthread_create(&threads[1], &pthread_custom_attr[1], (void *) thread_le_sensor, NULL);
	pthread_create(&threads[2], &pthread_custom_attr[2], (void *) thread_alarme, NULL);
	pthread_create(&threads[3], &pthread_custom_attr[3], (void *) thread_controle_temperatura, NULL);
	pthread_create(&threads[4], &pthread_custom_attr[4], (void *) thread_grava_temp_resp, NULL);
	pthread_create(&threads[5], &pthread_custom_attr[5], (void *) thread_controle_nivel, NULL);
	pthread_create(&threads[6], &pthread_custom_attr[6], (void *) thread_le_nivel_temp, NULL);
	pthread_create(&threads[7], &pthread_custom_attr[7], (void *) thread_gravar_temp, NULL);
	pthread_create(&threads[8], &pthread_custom_attr[8], (void *) thread_gravar_nivel, NULL);

	for(int i=0;i<NUM_THREADS;i++){
		pthread_join( threads[i], NULL);

	}

}
