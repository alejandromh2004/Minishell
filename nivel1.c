#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define DEBUGN1 1
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define PROMPT '$'

int internal_cd(char **args){
    printf("COMANDO CD\n");
    return 1;
}
int internal_export(char **args){
    printf("COMANDO EXPORT\n");
    return 1;
}
int internal_source(char **args){
    printf("COMANDO SOURCE\n");
    return 1;
}
int internal_jobs(){
    printf("COMANDO JOBS\n");
    return 1;
}
int internal_fg(char **args){
    printf("COMANDO FG\n");
    return 1;
}
int internal_bg(char **args){
    printf("COMANDO BG\n");
    return 1;
}

int check_internal(char **args){
    if(strcmp("exit",args[0])==0){
        printf("¡ADIOS!\n");
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
    args[palabras]=NULL;

    #if DEBUGN1
    printf("\033[90m[Número de tokens: %d]\033[0m\n", palabras);
    for (int i = 0; i < palabras; i++) {
    printf("\033[90m[parse_args Token %d: %s]\033[0m\n", i, args[i]);
    }
    #endif
    
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
            printf("\n¡ADIOS!\n");
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