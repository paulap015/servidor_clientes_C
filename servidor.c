#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netutil.h"
#include "split.h"
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/*Funciones*/
void  * process(void *  p_client_sd);
void remove_client(int client_sd);
bool fileInUse(int client_sd, char * path);
void updateClient(int client_sd, char * new_path);
int x=0;
pthread_mutex_t mutex =PTHREAD_MUTEX_INITIALIZER; 
struct sockaddr_in *addr;
pthread_t hilo[50];
typedef struct{
  int  client_sd;
  int client_sock;
  char * client_path;
  pthread_mutex_t client_mutex;
}clients;
clients array_clients[50];
clients client;
int cant_clients = 0;
int sd;
bool flag = false; //tiene algun archivo asignado? no 
char c;
int i;
int main(int argc,char *argv[]){
  if (argc != 2) {
    fprintf(stderr, "Debe especificar el puerto a escuchar\n");
    exit(EXIT_FAILURE);
  }
  /* Creando el socket con el servidor */
  sd = socket(PF_INET, SOCK_STREAM, 0);
  addr = server_address(atoi(argv[1]));
  bind(sd, (struct sockaddr *) addr,sizeof(struct sockaddr_in));
  /* Escuchando el socket abierto */
  listen(sd, 10);
  
  i = 0; // cantidad hilos 
  printf("Server listening at port %s\n", argv[1]);
  /* Se pasa a atender todas las peticiones hasta que se finalice el proceso */
  while (1)
  {
    int client_sd;  
    printf("Now accepting connections...\n");
    client_sd = accept(sd, 0, 0); 
    printf("Cliente Connected: %d Sock: %d \n", client_sd, sd);
    int * p_client_sd = malloc(sizeof(int));
    *p_client_sd = client_sd;
    if(pthread_create(&hilo[i++], NULL, process, p_client_sd)!=0){ //crear los hilos cliente
      printf("Error creating thread %d ",i);
    }
  } 
}
void  * process(void *  p_client_sd){
      /*Variables de uso general*/
      FILE * fp;
      char buf[BUFSIZ];
      int client_sd = * ((int *) p_client_sd);
      struct stat sb;
      char * tamanio_file;
      int file_size; 
      int lines_read = 0;
      char * name_file;
      int var_aux=-1; //1 si es send 2 si es recv 

      free(p_client_sd); //liberar memoria del puntero

      while(1){ //Mentras sea verdadero
        int nread = read(client_sd, buf, BUFSIZ);
        
        if(nread == 0){//Salga cuando no se lea ningún byte.
          break;
        }
        //Validar tipo de mensaje
        split_list * sp = split(buf, " \r\n"); //header -> info recv/send nombre tamanio
        
        if(lines_read == 0){
          if( strcmp(sp->parts[0],"send")==0){
            var_aux=1;
            lines_read=1;
          }else if( strcmp(sp->parts[0],"recv")==0){
            var_aux=2;
          }
        }
        //llegada del header SEND -> preparar el buf para el flujo que viene
        if(sp->count > 1 && var_aux==1 && (strcmp(sp->parts[1], "info") == 0)){
            //Crear Archivo
            file_size = atoi(sp->parts[3]);
            char base_file[200] = "resources_server/";
            char * name_file = sp->parts[2];
            strcat(base_file,name_file);

            updateClient(client_sd, base_file);/*GUARDAR USUARIOS O ACTUALIZAR INFO*/
            // [IMP] seccion critica 
            
            while(fileInUse(client_sd, base_file)){//ESTÁ EN USO EL ARCHIVO?
                send(client_sd, "Wait", BUFSIZ,0);//Se envía una señal de espera al cliente
                usleep(700);
            }
            send(client_sd, "Ready", BUFSIZ,0);//El cliente puede empezar a envíar fin seccion critica
            
            fp = fopen(base_file, "wt"); //crear archivo y escribir en el 
            flag = true; //el archivo ahora esta en uso
        }else if(sp->count <=1 && var_aux==1){ // llegada de cuerpo SEND 
            if(lines_read<=file_size){
              
              fputs(buf, fp);
              usleep(700);
              //printf("Lines read: %d of : %d, Value %s \n", lines_read, file_size, buf);
              if(lines_read == file_size){
                  //printf("Terminé %d Buf: %s\n",lines_read, buf);
                  printf("\nPetición completada :)\n");  
                  memset(buf,0,sizeof buf);
                  lines_read=0;
                  fclose(fp);
                  updateClient(client_sd," ");//No está con ningún archivo asignado
                  flag = false;
              }else{
                lines_read ++;
              } 
              memset(buf,0,sizeof buf);
            }   
          }//fin cuerpo send // header de recv
          else if(sp->count >= 1 && var_aux==2 && (strcmp(sp->parts[1], "info") == 0)){ 
          
            char ruta_s[100]= "resources_server/"; //Ruta base archivos del cliente;
            name_file= sp->parts[2];
            strcat(ruta_s, name_file);//Concatenar con el nombre del archivo
            
            // buscar si el archivo existe
            if (stat(ruta_s, &sb) == -1) { //Si el archivo no está
                memset(buf,0,sizeof buf);
                printf("Archivo inconsistente \n");strcpy(buf,"FAIL");
                write(client_sd, buf, BUFSIZ); //NOTIFICAR al cliente que no existe
                //printf("\n Esperando nueva instruccion ...\n ");
                continue;
            }else{ 
              //Existe el archivo -> enviar header al cliente
              memset(buf,0,sizeof buf);
              if (asprintf(&tamanio_file, "%d", sb.st_size) == -1) { //Si no es posible verificar el tamaño del archivo
                  perror("asprintf");
              }
              strcat(buf,"recv"); strcat(buf," ");
              strcat(buf,"info");strcat(buf," ");
              strcat(buf,name_file);strcat(buf," ");
              strcat(buf,tamanio_file);

              pthread_mutex_lock(&mutex); // me bloqueo antes de entrar a la sección critica - cliente no ha abierto nada

              write(client_sd,buf,BUFSIZ); //enviar encabezado            
              memset(buf,0,sizeof buf); //limpiar buf
              
              //pthread_mutex_lock(&mutex);
              x ++;
              
              fp = fopen(ruta_s,"r,"); //abrir el archivo para leer
              //printf("archivo abierto %d veces\n",x);
              int cont = 1;
              if(fp!=NULL){ //si   no esta vacio
                while(!feof(fp)){
                  if(lines_read == atoi(tamanio_file)){ //ya enviamos todos los caracteres ? 
                    printf("Petición completada :)\n");
                    memset(buf,0,sizeof buf);
                    lines_read=0;
                    break;
                  }
                  char c;
                  c = fgetc(fp);
                  buf[0] = c;
                  send(client_sd, buf, BUFSIZ,0); //enviar al cliente el caracter
                  //printf("Lines read: %d of : %d, Value %s \n", cont, atoi(tamanio_file), buf);
                  memset(buf,0,sizeof buf);
                  lines_read ++;
                  cont++;
                }
                memset(buf,0,sizeof buf);
              } 
              fclose(fp);
              x--;
              pthread_mutex_unlock(&mutex); //fin seccion critica ->libero 
            }

          } //fin recv

    }//fin while servidor
    remove_client(client_sd);
    close(client_sd);
    printf("Connection terminated\n");  
    printf("Now accepting connections... \n");
    return NULL;
}

void updateClient(int client_sd, char * new_path){ //actualizar ruta de cliente o si ya temrino ponerla en ""
    bool existe = false;
    for(int i = 0; i<cant_clients; i++){
    if(array_clients[i].client_sd == client_sd){//No es cliente nuevo
      array_clients[i].client_path = new_path;
      existe = true;
    }
    if(array_clients[i].client_sd == 0){
      break;
    }
  }
    if(existe == false){
      client.client_sd = client_sd;
      client.client_path = new_path; 
      array_clients[cant_clients]=client;
      cant_clients ++;
    }

}

bool fileInUse(int client_sd, char * path){//el archivo con path x esta siendo usado
  for(int i = 0; i<cant_clients; i++){
    if(strcmp(array_clients[i].client_path, path) == 0 && array_clients[i].client_sd != client_sd){
      return true;
    }
  }
  return false;
}
void remove_client(int client_sd){//quita al usuario con id sd_client de la lista de clientes 
  bool flag = false;
  for(int i = 0; i<cant_clients; i++){
    //printf("Usuario Id: %d -> Path: %s \n", array_clients[i].client_sd, array_clients[i].client_path);
  if(array_clients[i].client_sd == client_sd || flag == true){
      array_clients[i] = array_clients[i+1];  
      flag = true;                             
  }
  if(array_clients[i].client_sd == 0){
    cant_clients --;
    break;
  }
}
}



