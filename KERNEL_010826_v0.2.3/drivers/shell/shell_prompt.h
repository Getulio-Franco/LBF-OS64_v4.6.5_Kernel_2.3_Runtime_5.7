#ifndef SHELL_PROMPT_H
#define SHELL_PROMPT_H

/**
 * @brief Retorna a string do caminho atual (ex: "C:\DOS").
 */
char* get_current_path();

/**
 * @brief Atualiza a variável do caminho baseada no argumento do 'cd'.
 * @param arg Nome da pasta ou ".."
 */
void update_shell_path(const char* arg);

#endif
