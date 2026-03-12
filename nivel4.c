#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define DEBUGN1 0
#define DEBUGN2 0
#define DEBUGN3 0
#define DEBUGN4 1
#define PROMPT '$'
#define N_JOBS 64

struct info_job {
   pid_t pid;
   char estado; // ‘N’, ’E’, ‘D’, ‘F’ (‘N’: Ninguno, ‘E’: Ejecutándose y ‘D’: Detenido, ‘F’: Finalizado) 
   char cmd[COMMAND_LINE_SIZE]; // línea de comando asociada
};

static struct info_job jobs_list[N_JOBS];
static char mi_shell[COMMAND_LINE_SIZE]; // Variable global para el nombre del minishell

int internal_cd(char **args) {
    char current_path[1024]; // Buffer para almacenar el directorio actual.

    if (args[1] == NULL) {
        // Si no se pasa argumento, cambia al directorio "/home".
        if (chdir("/home") == 0) {
            // Obtiene y muestra el directorio actual.
            if (getcwd(current_path, sizeof(current_path)) != NULL) {
                #if DEBUGN2
                printf("\033[90m[internal_cd()→ PWD: %s]\033[0m\n", current_path); // Gris
                #endif
            } else {
                fprintf(stderr, "\033[31mgetcwd() error: %s\033[0m\n", strerror(errno));
            }
        } else {
            fprintf(stderr, "\033[31mchdir() error: %s\033[0m\n", strerror(errno));
        }
    } else {
        char *new_path = args[1];
        // Cambia al directorio especificado.
        if (chdir(new_path) == 0) {
            // Obtiene y muestra el directorio actual.
            if (getcwd(current_path, sizeof(current_path)) != NULL) {
                #if DEBUGN2
                printf("\033[90m[internal_cd()→ PWD: %s]\033[0m\n", current_path);
                #endif
            } else {
                fprintf(stderr, "\033[31mgetcwd() error: %s\033[0m\n", strerror(errno));
            }
        } else {
            fprintf(stderr, "\033[31mchdir() error: %s\033[0m\n", strerror(errno)); // Rojo
        }
    }
    return 1;
}
int execute_line(char *line);

// Función interna export
int internal_export(char **args) {
    // Verificación de que el formato sea adecuado (que exista args[1])
    if (args[1] == NULL) {
        fprintf(stderr, "\033[31mUso: export NOMBRE=VALOR\033[0m\n");
        return 1;  // Indica error
    }

    char *nombre;
    char *valor;
    char *token;

    // Separar nombre y valor
    token = strtok(args[1], "=");
    nombre = token;
    token = strtok(NULL, "=");
    valor = token;

    // Depuración: Mostrar el nombre y el valor
    #if DEBUGN2
    printf("\033[90m[internal_export()→ nombre: %s]\033[0m\n", nombre);
    printf("\033[90m[internal_export()→ valor: %s]\033[0m\n", valor ? valor : "(null)");
    #endif
    // Verificación de que el formato sea adecuado (NOMBRE=VALOR)
    if (nombre == NULL || valor == NULL) {
        fprintf(stderr, "\033[31mUso: export NOMBRE=VALOR\033[0m\n");
        return 1;  // Indica error
    }

    // Muestra el valor inicial de la variable usando getenv
    char *valor_inicial = getenv(nombre);
    #if DEBUGN2
    printf("\033[90m[internal_export()→ antiguo valor para %s: %s]\033[0m\n", 
           nombre, valor_inicial ? valor_inicial : "(null)");
    #endif
    // Modificar el valor de la variable de entorno usando setenv
    if (setenv(nombre, valor, 1) != 0) {  // '1' permite sobrescribir la variable si ya existe
        fprintf(stderr, "\033[31mError al asignar la variable de entorno: %s\033[0m\n", strerror(errno));
        return 1;
    }

    // Muestra el nuevo valor de la variable para confirmar el cambio
    char *nuevo_valor = getenv(nombre);
    #if DEBUGN2
    printf("\033[90m[internal_export()→ nuevo valor para %s: %s]\033[0m\n", nombre, nuevo_valor);
    #endif
    return 1;
}

// Funciones internas vacías (aún no implementadas)
int internal_source(char **args){
    // Verificar si se proporcionó un argumento (nombre del archivo)
    if (args[1] == NULL) {
        fprintf(stderr, "\033[31mUso: source <archivo>\n");
        return -1;
    }

    // Abrir el archivo en modo lectura
    FILE *file = fopen(args[1], "r");
    if (file == NULL) {
        perror("\033[31mError al abrir el archivo");
        return 1;
    }

    char line[1024]; // Buffer para leer las líneas del archivo
    while (fgets(line, sizeof(line), file) != NULL) {
        // Eliminar el salto de línea si está presente
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Realizar un fflush antes de ejecutar la línea
        fflush(file);

        // Pasar la línea procesada a la función execute_line()
        if (execute_line(line) != 0) {
            fprintf(stderr, "\033[31mError al ejecutar la línea: %s\n", line);
            fclose(file);
            
        }
    }

    // Cerrar el archivo
    fclose(file);
    return 1;
}

int internal_jobs(){
    return 1;
}

int internal_fg(char **args){
    return 1;
}

int internal_bg(char **args){
    return 1;
}

// Función para verificar comandos internos
int check_internal(char **args){
    if(strcmp("exit",args[0])==0){
        exit(0);
    }
    if (strcmp("cd", args[0]) == 0) {
        return internal_cd(args);
    } else if (strcmp("export", args[0]) == 0) {
        return internal_export(args);
    } else if (strcmp("source", args[0]) == 0) {
        return internal_source(args);
    } else if (strcmp("jobs", args[0]) == 0) {
        return internal_jobs();
    } else if (strcmp("fg", args[0]) == 0) {
        return internal_fg(args);
    } else if (strcmp("bg", args[0]) == 0) {
        return internal_bg(args);
    }
    return 0;
}

// Función para dividir la línea en argumentos
int parse_args(char **args, char *line){
    int palabras=0;
    char *token;
    token=strtok(line," ");
    while(token!=NULL){
        if(token[0]=='#'){
            args[palabras]=NULL;
            break;;
        }
        args[palabras]=token;
        palabras++;
        token=strtok(NULL," ");
    }
    #if DEBUGN1
    printf("\033[90m[Número de tokens: %d]\033[0m\n", palabras);
    for (int i = 0; i < palabras; i++) {
    printf("\033[90m[parse_args Token %d: %s]\033[0m\n", i, args[i]);
    }
    #endif
    args[palabras]=NULL;
    return palabras;
}

// Función para imprimir el prompt con colores
void imprimir_prompt() {
    char *user = getenv("USER");
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    printf("\033[1;32m%s\033[0m:\033[1;34m%s\033[0m%c ", user, cwd, PROMPT);
    fflush(stdout);  // Forzar la impresión inmediata del prompt
}

// Función para leer la línea de entrada
char *read_line(char *line) {
    imprimir_prompt();  // Muestra el prompt personalizado

    if (fgets(line, COMMAND_LINE_SIZE, stdin) != NULL) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (strlen(line) == 0) {
            return NULL;
        }
        return line;
    } else {
        if (feof(stdin)) {
            printf("\n");
            exit(0);
        }
        return NULL;  // En caso de error en la lectura
    }
}

// Función reaper para manejar la señal SIGCHLD
void reaper(int signum) {
    signal(SIGCHLD, reaper);  // Reasociar la señal a reaper
    pid_t pid;
    int status;

    // Bucle para manejar todos los hijos finalizados
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == jobs_list[0].pid) {
            jobs_list[0].estado = 'F';  // Cambia el estado a 'F' (Finalizado)
            jobs_list[0].pid = 0;
            memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);  // Limpia el comando
            #if DEBUGN4
            printf("\033[1;30m[reaper()→ Proceso hijo con PID %d finalizado]\033[0m\n", pid);
            #endif
        }

    }
}

// Función ctrlc para manejar la señal SIGINT
void ctrlc(int signum) {
    signal(SIGINT, ctrlc);  // Reasociar la señal a ctrlc
    printf("\n");
    // Verifica si hay un proceso en primer plano
    if (jobs_list[0].pid > 0) {
        // Envía SIGTERM al proceso en foreground, excepto si es el propio shell
        if (strcmp(jobs_list[0].cmd, mi_shell) != 0) {
            kill(jobs_list[0].pid, SIGTERM);
            #if DEBUGN4
            printf("\033[1;30m[ctrlc()→ Señal SIGTERM enviada al proceso con PID %d]\033[0m\n", jobs_list[0].pid);
            #endif
        } else {
            #if DEBUGN4
            printf("\033[1;30m[ctrlc()→ No se envió SIGTERM al shell]\033[0m\n");
            #endif
        }
    } else {
        #if DEBUGN4
        printf("\033[1;30m[ctrlc()→ No hay proceso en primer plano]\033[0m\n");
        #endif
        imprimir_prompt();
    }
    
}


// Función para ejecutar la línea de comando
int execute_line(char *line) {
    char *troceada[ARGS_SIZE];
    parse_args(troceada, line);

    if (check_internal(troceada) == 0) {  // Verifica si el comando es interno
        pid_t proceso = fork();
        if (proceso < 0) {  // Error en fork
            fprintf(stderr, "\033[1;31m[ERROR EJECUCIÓN PROCESO HIJO]\033[0m\n"); // Error en rojo
            exit(-1);
        }
        
        if (proceso == 0) {  // Proceso hijo
            signal(SIGINT, SIG_IGN);  // Ignorar SIGINT en el hijo
            execvp(troceada[0], troceada);
            fprintf(stderr, "\033[1;31m%s: No such file or directory\033[0m\n", troceada[0]); // Error en rojo
            exit(-1);
            
        }
        
        if (proceso > 0) {  // Proceso padre (minishell)
            jobs_list[0].estado = 'E';  // Establecer estado en 'E' (Ejecutándose)
            jobs_list[0].pid = proceso;  // Guardar PID del proceso hijo
            strncpy(jobs_list[0].cmd, line, COMMAND_LINE_SIZE - 1);  // Guardar el comando ejecutado
            jobs_list[0].cmd[COMMAND_LINE_SIZE - 1] = '\0';

            // Mensajes de seguimiento en gris entre corchetes
            #if DEBUGN3
            printf("\033[1;30m[PID del proceso padre: %d]\033[0m\n", getpid());
            printf("\033[1;30m[PID del proceso hijo: %d]\033[0m\n", proceso);
            printf("\033[1;30m[Shell: %s]\033[0m\n", mi_shell);
            printf("\033[1;30m[Comando en foreground: %s]\033[0m\n", jobs_list[0].cmd);
            #endif

            // Bloquear hasta recibir señal, espera a que el proceso en foreground termine
            while (jobs_list[0].pid != 0) {
                pause();  // Bloquear hasta recibir una señal (SIGCHLD o SIGINT)
            }
        }
    }
}

// Función main
int main(int argc, char *argv[]) {
    char linea_leida[COMMAND_LINE_SIZE];
    jobs_list[0].pid = 0;
    jobs_list[0].estado = 'N';
    memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);

    // Guardar el nombre del programa en la variable mi_shell
    strncpy(mi_shell, argv[0], COMMAND_LINE_SIZE - 1);
    mi_shell[COMMAND_LINE_SIZE - 1] = '\0';

    // Configuración de los manejadores de señal para SIGCHLD y SIGINT
    signal(SIGCHLD, reaper);
    signal(SIGINT, ctrlc);
    
    while (1) {
        if (read_line(linea_leida)) {  // Leer la línea de comando
            execute_line(linea_leida); // Ejecutar la línea de comando
        }
    }
    return 0;
}