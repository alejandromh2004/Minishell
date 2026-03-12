#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define DEBUGN1 0
#define DEBUGN2 1
#define PROMPT '$'

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
    return 0;
}
int internal_source(char **args){

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

int check_internal(char **args){
    if(strcmp("exit",args[0])==0){
        exit(0);
    }// Verifica si args[0] es uno de los comandos internos y llama a la función correspondiente
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

    // Si no se encuentra el comando en la lista de internos, devuelve 0 (FALSE)
    return 0;
}
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
void imprimir_prompt() {
    char *user = getenv("USER");
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    // Imprime el prompt personalizado
    printf("\033[1;32m%s\033[0m:\033[1;34m%s\033[0m%c ", user, cwd, PROMPT);
    fflush(stdout);  // Forzar la impresión inmediata del prompt
}

char *read_line(char *line) {
    imprimir_prompt();  // Muestra el prompt personalizado

    // Lee la línea de entrada
    if (fgets(line, COMMAND_LINE_SIZE, stdin) != NULL) {
        // Reemplaza el '\n' final con '\0'
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (strlen(line) == 0) {
            return NULL;
        }
        return line;
    } else {
        // Detecta Ctrl+D (EOF)
        if (feof(stdin)) {
            printf("\n");
            exit(0);
        }
        return NULL;  // En caso de error en la lectura
    }
}
int execute_line(char *line){
    char *troceada[ARGS_SIZE];
    parse_args(troceada,line);
    check_internal(troceada); 
}
int main() {
    char linea_leida[COMMAND_LINE_SIZE];

    while (1) {
        if (read_line(linea_leida)) {
            execute_line(linea_leida);
        }
    }

    return 0;
}