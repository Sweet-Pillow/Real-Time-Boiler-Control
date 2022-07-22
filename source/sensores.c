/* Monitor sensor, no arquivo sensor.c */ 
#include <math.h>
#include <pthread.h>
#include <string.h>

static pthread_mutex_t exclusao_mutua = PTHREAD_MUTEX_INITIALIZER; 
static pthread_cond_t alarme = PTHREAD_COND_INITIALIZER; 
static double s_temp = 0, s_nivel = 0, s_temp_ambi = 0, s_temp_entr = 0, s_fluxo_said = 0; 
static double limite_atual = HUGE_VAL;

/* Chamado pela thread que le o sensor e disponibiliza aqui o valor lido */
 void sensor_put( double temp, double nivel, double temp_ambi, double temp_entr, double fluxo_said) {
	 pthread_mutex_lock( &exclusao_mutua); 
	 s_temp = temp;
	 s_nivel = nivel;
	 s_temp_ambi = temp_ambi;
	 s_temp_entr = temp_entr;
	 s_fluxo_said = fluxo_said;
	 if( s_temp >= limite_atual) 
	 	pthread_cond_signal( &alarme); 
	 pthread_mutex_unlock( &exclusao_mutua); 
 }
 
 /* Chamado por qualquer thread que precisa do valor lido do sensor */ 
 double sensor_get(char s[5]) {
	 double aux; 
	 pthread_mutex_lock( &exclusao_mutua); 
	 if (strncmp(s,"t",1)==0) //Valor da Temperatura
	 aux = s_temp;
	 else if(strncmp(s,"h",1)==0) //Valor do nivel da agua
	 aux = s_nivel;		
	 else if(strncmp(s,"ta",1)==0) //Valor da temperatura do ambiente
	 aux = s_temp_ambi;
	 else if(strncmp(s,"ti",1)==0) //Valor da temperatura da agua que entra no ambiente pelo atuador Ni
	 aux = s_temp_entr;
	 else if(strncmp(s,"no",1)==0) //Valor do fluxo de agua de saida do recipiente
	 aux = s_fluxo_said;
	 pthread_mutex_unlock( &exclusao_mutua); 
	 return aux;
 }

/* Thread fica bloqueada até o valor do sensor chegar em limite */ 
void sensor_alarmeT( double limite) {
	pthread_mutex_lock( &exclusao_mutua); 
	limite_atual = limite; 
	while( s_temp < limite_atual) 
		pthread_cond_wait( &alarme, &exclusao_mutua); 
	limite_atual = HUGE_VAL; 
	pthread_mutex_unlock( &exclusao_mutua); 
}
