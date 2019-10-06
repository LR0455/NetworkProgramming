#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *env[100];
    
void print(char *output){
    write(1, output, strlen(output));
}
void printline(char *output){
    write(1, output, strlen(output));
    write(1, "\n", strlen("\n"));
}
char** parse(char *input, int len){
    char **parsed_input = malloc(100);
    int col = 0, row = 0;
    
    for (int i = 0 ; i <= len ; i ++){
        if (row == 0)
            parsed_input[col] = malloc(50);

        if (input[i] == '|' || input[i] == 10){
            parsed_input[col][row] = '\0';
            col ++;
            row = 0;
        }
        else 
            parsed_input[col][row ++] = input[i];
    }
    parsed_input[col] = '\0';
    return parsed_input;
}

char** parse_cmd(char *cmd){
    char **parsed_cmd = malloc(100);
    int col = 0, row = 0;
    for (int i = 0 ; i <= strlen(cmd) ; i ++){
        if (row == 0)
            parsed_cmd[col] = malloc(50);

        if (cmd[i] == ' ' || cmd[i] == '\0'){
            parsed_cmd[col][row] = '\0';
            col ++;
            row = 0;
        }
        else 
            parsed_cmd[col][row ++] = cmd[i];
    }
    parsed_cmd[col] = '\0';
    return parsed_cmd;
}

int main(int argc, char* argv[], char* env[]){
    env[0] = malloc(50);
    env[0][0] = 0;
    strcat(env[0], "PATH=");
    
    while(1){
        write(1, "% ", strlen("% "));
        char *input = malloc(150);
        int len = read(0, input, 100);
        char **parsed_input = parse(input, len);
        char **s = parsed_input;
        for( ; *s ; s ++) {
            char **parsed_cmd = parse_cmd(*s);
            char **t = parsed_cmd;
            
            if (strcmp(parsed_cmd[0], "setenv") == 0) {
                strcat(env[0], parsed_cmd[1]);
                strcat(env[0], ";"); 
            }
                
            else if(strcmp(parsed_cmd[0], "printenv") == 0)
                printline(env[0]);
            
            free(parsed_cmd);
        }

        free(input);
        free(parsed_input);
        
    }
}
