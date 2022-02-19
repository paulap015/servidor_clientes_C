#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "netutil.h"
#include "split.h"
#include <sys/stat.h>


FILE *fp;
char *files_opened[50];
int pos=0;

int main(int argc,char *argv[])
{
   struct sockaddr_in * addr;
   split_list * sp;
   
   struct stat sb;
   char comando[80];
   char buf[BUFSIZ];
   int sd;
   char s[200];
   
   char * name_file ;
   char * tamanio_file;
   int lines_read = 0;
   /* Primero se obtiene la direccion IP del host a conectar */
   if (argc<3)
   {
      printf("\nFaltan parametros de llamada.\nUso:\n\n%s nombre_host\n\n", argv[0]);
      exit(EXIT_FAILURE);
   }
   /* Creando el socket con el servidor */
   sd = socket(PF_INET, SOCK_STREAM, 0); printf("Conectado al socket %d \n",sd);
   //obtener la direccion del servidor
   addr = address_by_ip(argv[1], atoi(argv[2]));
   connect(sd, (struct sockaddr *) addr, sizeof(struct sockaddr_in)); //conectando al socket

   while(1){
      
      printf(">");
      fgets(comando, BUFSIZ, stdin);
      sp = split(comando, " \r\n"); //comando que ingresamos por consola stdin
      //printf("esto es sp %d",sp->count);
      if(sp->count == 0){
         continue;
      }
      if(strcmp(sp->parts[0],"exit") == 0){
         break;
      }
      
      if(strcmp(sp->parts[0],"send") == 0 || strcmp(sp->parts[0],"enviar") == 0 || strcmp(sp->parts[0],"put") == 0) {
         memset(buf,0,sizeof buf);
         char ruta[100]= "resources_client/"; //Ruta base archivos del cliente;
         strcat(ruta, sp->parts[1]);//Concatenar con el nombre del archivo
         
         printf("Ruta: %s \n", ruta);
         
         if (stat(ruta, &sb) == -1) { //Si el archivo no está
            printf("Archivo inconsistente \n");
            continue;
         }
         name_file= sp->parts[1]; //Nombre del archivo
         
         if (asprintf(&tamanio_file, "%d", sb.st_size) == -1) { //Si no es posible verificar el tamaño del archivo
            perror("asprintf");
         }else {

            strcat(buf,"send");strcat(buf, " ");
            strcat(buf, "info");strcat(buf," ");
            strcat(buf,name_file);strcat(buf," ");
            strcat(buf,tamanio_file);
         }
            //printf("Este es el buf antes de irse cliente %s ",buf);
            send(sd, buf, BUFSIZ, 0); //Se envía información anticipada al servidor
            
            printf("Loading... \n");
            memset(buf,0,sizeof buf);

         while (1)//Mientras el archivo esté ocupado en el lado del servidor
         {
            read(sd, buf, BUFSIZ); //leyendo respuesta del servidor
            if(strcmp(buf,"Ready") == 0){
               break;
            }
            usleep(700);
         }
         memset(buf,0,sizeof buf);
         int cont=1;
         lines_read=0;   
         fp = fopen(ruta,"r,");
         if(fp!=NULL)
         {
            while(!feof(fp))
            {
               if(lines_read == atoi(tamanio_file)){
                  break;
               }
               char c;
               c = fgetc(fp);
               buf[0] = c;
               send(sd, buf, BUFSIZ,0); //enviar caracter por caracter al servidor
               //printf("Lines send: %d of : %d, Value %s \n", cont, atoi(tamanio_file), buf);
               cont ++;
               usleep(650);
               memset(buf,0,sizeof buf);
               lines_read ++;
            }
               
            printf("\n");
            
         } 
         printf("Sucess! \n");
         fclose(fp);
         
      }else if(strcmp(sp->parts[0],"recv") == 0|| strcmp(sp->parts[0],"recibir") == 0 || strcmp(sp->parts[0],"get") == 0){     
         memset(buf,0,sizeof buf);
         char ruta_s[100]= "resources_server/"; //Ruta base archivos del cliente;
         int file_size;
         name_file= sp->parts[1]; //Nombre del archivo

         strcat(ruta_s, name_file);//Concatenar con el nombre del archivo
         strcat(buf,"recv");strcat(buf," ");
         strcat(buf, "info");strcat(buf," ");
         strcat(buf, name_file);

         send(sd, buf, BUFSIZ, 0); //decir al servidor QUE ARCHIVO NECESITO : lo tienes?
         //mutex del otro lado si existe solo entra 1 hilo a la vez -> aqui solo llega 1 respuesta no abro mas files 
         int valread = read(sd,buf,BUFSIZ); //servidor responde FAIL si no existe ; header si existes
         printf("Loading... \n");  
         if(strcmp(buf,"FAIL")==0){
            printf("Archivo [%s] no existe en el sevidor. \n",name_file);
            continue;
         }else{//llega info del archivo
            
            split_list * buf_split = split(buf, " \r\n");
            char ruta_sc[100]= "resources_client/";
            file_size = atoi(buf_split->parts[3]);
            name_file= buf_split->parts[2];
            strcat(ruta_sc,name_file);

            fp = fopen(ruta_sc, "wt");
            //perror("en open");
            int lines_read=1;

            while(1){ //escuchar caracteres que le llegan no sale hasta lines_read=fileS
               int nread = read(sd,buf,BUFSIZ);
               if(nread == 0){
                  break;
               }
               buf_split = split(buf, " \r\n");
               if(lines_read <= file_size){
                  fputs(buf, fp);
                  usleep(700);
                  //printf("Lines send: %d of : %d, Value  [%s] \n", lines_read, file_size, buf);
                  if(lines_read == file_size){
                     //printf("Terminé %d Buf: %s\n",lines_read, buf);
                     lines_read = 0;
                     
                     fclose(fp);
                     printf("\nSucess! \n");
                     break;
                  }else{
                     lines_read ++;
                  }
                  memset(buf,0,sizeof buf);
               }

            }
            
            
         }
      }

   } 
   close(sd);
   printf("Connection Finished \n");
   
}


