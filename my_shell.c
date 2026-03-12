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
#define PROMPT '$'
#define ARGS_SIZE 64
#define DEBUGN1 0
#define DEBUGN2 0
#define DEBUGN3 0
#define DEBUGN4 0
#define DEBUGN5 0
#define DEBUGN6 0
#define N_JOBS 64

int n_job=0;


struct info_job {
   pid_t pid;
   char estado; // ‘N’, ’E’, ‘D’, ‘F’ (‘N’: Ninguno, ‘E’: Ejecutándose y ‘D’: Detenido, ‘F’: Finalizado) 
   char cmd[COMMAND_LINE_SIZE]; // línea de comando asociada
};

static struct info_job jobs_list[N_JOBS];
static char mi_shell[COMMAND_LINE_SIZE]; // Variable global para el nombre del minishell
int jobs_list_find(pid_t pid);
int jobs_list_add(pid_t pid, char estado, char *cmd);
int jobs_list_remove(int pos);

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
        fprintf(stderr, "\033[1;31mUso: source <archivo>\033[0m\n");
        return -1;
    }

    // Abrir el archivo en modo lectura
    FILE *file = fopen(args[1], "r");
    if (file == NULL) {
        perror("\033[1;31mError al abrir el archivo\033[0m\n");
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
            fprintf(stderr, "\033[31mError al ejecutar la línea: %s\033[0m\n", line);
            fclose(file);
            
        }
    }

    // Cerrar el archivo
    fclose(file);
    return 1;
}

int internal_jobs() {
    int found_jobs = 0;  // Contador para trabajos válidos

    // Recorrer la lista de trabajos
    for (int i = 0; i < N_JOBS; i++) {
        if (jobs_list[i].pid > 0) {  // Solo procesar entradas válidas
            found_jobs = 1;  // Marcar que hay al menos un trabajo

            // Imprimir directamente el estado y el comando
            printf("[%d] PID: %d, Estado: %c, Comando: \"%s\"\n",
                   i , jobs_list[i].pid, jobs_list[i].estado, jobs_list[i].cmd);
        }
    }

    // Si no se encontraron trabajos, imprimir mensaje
    if (!found_jobs) {
        #if DEBUGN5
        printf("\033[1;31m[No hay trabajos en la lista]\033[0m\n");
        #endif
    }

    return 1;
}

// NIVEL 6, FG Y BG
// Enviar a primer plano el trabajo indicado
int internal_fg(char **args) {
    if (args[1] == NULL) { // Si no se pasa argumento, error
        fprintf(stderr, "\033[1;31mError: fg necesita un argumento.\033[0m\n");
        return 1;
    }

    int pos = atoi(args[1]); // Convertir el índice del trabajo a un entero
    if (pos <= 0 || pos > n_job) { // Validar que el índice esté dentro del rango
        fprintf(stderr, "\033[1;31mError: no existe ese trabajo.\033[0m\n");
        return 1;
    }

    struct info_job job = jobs_list[pos];
    if (job.estado == 'D') { // Si el trabajo está detenido, enviar SIGCONT
        kill(job.pid, SIGCONT);
        #if DEBUGN6
        printf("\033[1;30m[internal_fg()→ Señal 18 (SIGCONT) enviada a %d (%s)]\033[0m\n", job.pid, job.cmd);
        #endif
        printf("%s\n",job.cmd);
    }

    // Mover el trabajo al foreground
    jobs_list[0] = job;
    jobs_list[0].estado = 'E'; // Cambiar estado a ejecutándose
    jobs_list_remove(pos); // Eliminar el trabajo de la lista

    // Bloquear el shell hasta que termine el proceso en foreground
    while (jobs_list[0].pid != 0) {
        pause();
    }
    return 1;
}

// Reactivar un proceso detenido para que siga ejecutándose pero en segundo plano
int internal_bg(char **args) {
    if (args[1] == NULL) { // Validar que se pase un argumento
        fprintf(stderr, "\033[1;31mError: bg necesita un argumento.\033[0m\n");
        return 1;
    }

    int pos = atoi(args[1]); // Convertir el índice del trabajo a un entero
    if (pos <= 0 || pos > n_job) { // Validar que el índice esté dentro del rango
        fprintf(stderr, "\033[1;31mError: no existe ese trabajo.\033[0m\n");
        return 1;
    }

    struct info_job *job = &jobs_list[pos];
    if (job->estado == 'E') { // Si ya está ejecutándose, error
        fprintf(stderr, "\033[1;31mError: el trabajo ya está en segundo plano.\033[0m\n");
        return 1;
    }

    job->estado = 'E'; // Cambiar el estado a ejecutándose
    strcat(job->cmd, " &"); // Añadir '&' al final del comando
    kill(job->pid, SIGCONT); // Enviar la señal SIGCONT para reanudar
    #if DEBUGN6
    printf("\033[1;30m[internal_bg()→ Señal 18 (SIGCONT) enviada a %d (%s)]\033[0m\n", job->pid, job->cmd);
    #endif

    return 1;
}

// Redireccionamiento de la salida de un comando externo a un fichero externo, indicando “ >  fichero” al final de la línea
int is_output_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        // Buscar el símbolo '>'
        if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) {
            // Abrir el archivo indicado para redireccionar la salida estándar
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd == -1) { // Verificar si hubo error al abrir el archivo
                perror("Error al abrir el archivo para redirección");
                return -1;
            }
            // Redirigir la salida estándar al archivo
            dup2(fd, STDOUT_FILENO);
            close(fd); // Cerrar el descriptor de archivo
            args[i] = NULL; // Eliminar el token '>' de los argumentos
            return 1;
        }
    }
    return 0; // No hay redirección
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

int is_background(char **args) {
    int i = 0;

    // Buscar el final de la lista de argumentos
    while (args[i] != NULL) {
        i++;
    }

    // Si el último argumento es '&', es un comando en background
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        return 1;           // Background
    }

    return 0; // Foreground
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

void reaper(int signum) {
    signal(SIGCHLD, reaper);  // Reasociar el manejador para capturar futuras señales SIGCHLD
    pid_t pid;
    int status;

    // Bucle para manejar todos los hijos finalizados
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == jobs_list[0].pid) {
            // Si el hijo finalizado era el que estaba en foreground
            #if DEBUGN4
            printf("\033[1;30m[reaper()→ Proceso hijo con PID %d finalizado]\033[0m\n", pid);
            #endif

            jobs_list[0].estado = 'N';  // Resetear el estado del slot de foreground
            jobs_list[0].pid = 0;       // Resetear PID del foreground
            memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);  // Limpia el comando
        } else {
            // Si el hijo finalizado era un comando en background
            int pos = jobs_list_find(pid);  // Buscar el PID en la tabla
            if (pos >= 0) {  // Si se encontró
                #if DEBUGN5
                printf("\033[1;30m[reaper()→ Proceso en background con PID %d (%s) finalizado]\033[0m\n",
                       jobs_list[pos].pid, jobs_list[pos].cmd);
                #endif
                jobs_list_remove(pos);  // Eliminarlo de la tabla
                imprimir_prompt();
            } else {
                // Si el PID no está en la tabla (raro, pero puede pasar en ciertas circunstancias)
                printf("\033[1;31m[reaper()→ Error: Proceso con PID %d no encontrado en jobs_list]\033[0m\n", pid);
            }
        }
    }
}

//NIVEL 5, CONTROLADOR SEÑAL CTRLZ
//void ctrlz(int signum){ parecido a ctrlc, se pasa el proceso en foreground al final de la tabla con
//job_list_add como detenido D, y resetear el job_list[0]}
void ctrlz(int signum) {
    printf("\n");
    signal(SIGTSTP, ctrlz); // Reasociar la señal SIGTSTP a este manejador

    // Verificar si hay un proceso en primer plano
    if (jobs_list[0].pid > 0) {
        // Verificar que el proceso en foreground no sea el shell
        if (strcmp(jobs_list[0].cmd, mi_shell) != 0) {
            // Enviar señal SIGSTOP al proceso en foreground
            if (kill(jobs_list[0].pid, SIGSTOP) == 0) {
                #if DEBUGN5
                printf("\033[1;30m[ctrlz()→ Señal SIGSTOP enviada al proceso con PID %d]\033[0m\n", jobs_list[0].pid);
                #endif

                // Cambiar el estado del proceso a 'D' (detenido)
                jobs_list[0].estado = 'D';

                // Incorporar el proceso detenido a la tabla jobs_list por el final
                jobs_list_add(jobs_list[0].pid, 'D', jobs_list[0].cmd);

                printf("[%d] PID: %d, Estado: %c, Comando: \"%s\"\n",
                   n_job , jobs_list[n_job].pid, jobs_list[n_job].estado, jobs_list[n_job].cmd);

                // Resetear los datos de jobs_list[0] ya que el proceso ha dejado de estar en foreground
                jobs_list[0].pid = 0;
                jobs_list[0].estado = 'N';
                memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);
            } else {
                // Error al enviar la señal SIGSTOP
                fprintf(stderr, "\033[1;31m[ctrlz()→ Error al enviar SIGSTOP al proceso con PID %d]\033[0m\n", jobs_list[0].pid);
            }
        } else {
            // El proceso en foreground es el shell, no se envía SIGSTOP
            #if DEBUGN5
            printf("\033[1;30m[ctrlz()→ Señal SIGSTOP no enviada porque el proceso en foreground es el shell]\033[0m\n");
            #endif
        }
    } else {
        // No hay proceso en foreground
        #if DEBUGN5
        printf("\033[1;30m[ctrlz()→ Señal SIGSTOP no enviada porque no hay proceso en foreground]\033[0m\n");
        #endif
        imprimir_prompt();
    }
}

// Función ctrlc para manejar la señal SIGINT
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

int jobs_list_add(pid_t pid, char estado, char *cmd){
    n_job++;
    if(n_job<N_JOBS){
        // Copiar el comando de forma segura
        strncpy(jobs_list[n_job].cmd, cmd, COMMAND_LINE_SIZE - 1);
        jobs_list[n_job].cmd[COMMAND_LINE_SIZE - 1] = '\0';  // Asegurarse de que la cadena esté terminada en NULL
        jobs_list[n_job].pid=pid;
        jobs_list[n_job].estado=estado;

        // Agregar '&' al comando si el estado es 'E' (Ejecución en segundo plano)
        if (estado == 'E' && cmd[strlen(cmd) - 1] != '&') {
            strncat(jobs_list[n_job].cmd, " &", COMMAND_LINE_SIZE - strlen(jobs_list[n_job].cmd) - 1);
        }

    }else{
        fprintf(stderr, "\033[1;31m[ERROR] Lista de trabajos llena, no se pueden agregar más trabajos.\033[0m\n");
    }
}

int jobs_list_find(pid_t pid) {
    // Recorremos el arreglo de trabajos
    for (int i = 0; i < N_JOBS; i++) {
        if (jobs_list[i].pid == pid) {  // Si encontramos el PID
            return i;  // Retorna la posición donde se encuentra el PID
        }
    }
    return -1;  // Si no encontramos el PID, retornamos -1
}

int jobs_list_remove(int pos) {
    
    // Mover el último trabajo a la posición 'pos'
    jobs_list[pos].pid = jobs_list[n_job].pid;
    jobs_list[pos].estado=jobs_list[n_job].estado;
    strncpy(jobs_list[pos].cmd, jobs_list[n_job].cmd, COMMAND_LINE_SIZE - 1);

    // Limpiar la última posición (opcional, para mayor seguridad)
    memset(&jobs_list[n_job ], 0, sizeof(struct info_job));

    // Decrementar el número de trabajos
    n_job--;

    return 0;  // Retorna 0 si se eliminó correctamente
}



// Función para ejecutar la línea de comando
int execute_line(char *line) {
    char *troceada[ARGS_SIZE];
    parse_args(troceada, line);
    int bg;
    bg=is_background(troceada);
    if(bg==1){
        int i=0;
        while(troceada[i]!=NULL){
            i++;
        }
        troceada[i - 1] = NULL;
        
        
    }
    if (check_internal(troceada) == 0) {  // Verifica si el comando es interno
        pid_t proceso = fork();
        if (proceso < 0) {  // Error en fork
            fprintf(stderr, "\033[1;31m[ERROR EJECUCIÓN PROCESO HIJO]\033[0m\n"); // Error en rojo
            exit(-1);
        }
        
        if (proceso == 0) {  // Proceso hijo
            signal(SIGINT, SIG_IGN);  // Ignorar SIGINT en el hijo
            signal(SIGTSTP,SIG_IGN);
            if (is_output_redirection(troceada) == -1) {
                return 1;  // Error en la redirección
            }  
            execvp(troceada[0], troceada);
            fprintf(stderr, "\033[1;31m%s: No such file or directory\033[0m\n", troceada[0]); // Error en rojo
            exit(-1);
            
        }
        
        if (proceso > 0) {  // Proceso padre (minishell)
            
            // Mensajes de seguimiento en gris entre corchetes
            #if DEBUGN3
            printf("\033[1;30m[PID del proceso padre: %d]\033[0m\n", getpid());
            printf("\033[1;30m[PID del proceso hijo: %d]\033[0m\n", proceso);
            printf("\033[1;30m[Shell: %s]\033[0m\n", mi_shell);
            printf("\033[1;30m[Comando en foreground: %s]\033[0m\n", jobs_list[0].cmd);
            #endif


            //NIVEL 5: SI BKG (si no es una operacion en background) se hace el while(){pause},
            //si lo es, se hace job_list_add()

            if(bg==0){
                jobs_list[0].estado = 'E';  // Establecer estado en 'E' (Ejecutándose)
                jobs_list[0].pid = proceso;  // Guardar PID del proceso hijo
                strncpy(jobs_list[0].cmd, line, COMMAND_LINE_SIZE - 1);  // Guardar el comando ejecutado
                jobs_list[0].cmd[COMMAND_LINE_SIZE - 1] = '\0';
                // Bloquear hasta recibir señal, espera a que el proceso en foreground termine
                while (jobs_list[0].pid != 0) {
                    pause();  // Bloquear hasta recibir una señal (SIGCHLD o SIGINT)
                }
            }else{
                jobs_list_add(proceso,'E',troceada[0]);
                printf("[%d] PID: %d, Estado: %c, Comando: \"%s\"\n",
                   n_job , jobs_list[n_job].pid, jobs_list[n_job].estado, jobs_list[n_job].cmd);
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
    signal(SIGTSTP,ctrlz);
    //NIVEL5 signal(SIGTSTP,ctrlz)
    
    while (1) {
        if (read_line(linea_leida)) {  // Leer la línea de comando
            execute_line(linea_leida); // Ejecutar la línea de comando
        }
    }
    return 0;
}