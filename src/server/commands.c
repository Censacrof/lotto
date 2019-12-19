#include <stdio.h>
#include <stdlib.h>

#include "commands.h"
#include "../common.h"


int execute_command(int client_sock, char *cmd)
{
    printf("%s\n", cmd);
    send_msg(client_sock, cmd);

    return 0;
}