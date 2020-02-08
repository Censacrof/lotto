#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../commands.h"
#include "../../data.h"

// !esci
int esci(int client_sock)
{
    session.username[0] = '\0';
    session.id[0] = '\0';

    send_response(client_sock, SRESP_CLOSE, "logout eseguito");
    return -1;
}