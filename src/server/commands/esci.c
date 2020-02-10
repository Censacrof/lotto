#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../commands.h"
#include "../../data.h"

// !esci
int esci(int client_sock)
{
    int was_logged = session.username[0] != '\0';

    if (was_logged)
    {
        session.username[0] = '\0';
        session.id[0] = '\0';
        send_response(client_sock, SRESP_CLOSE, "logout eseguito. arrivederci");
    }
    else
        send_response(client_sock, SRESP_CLOSE, "arrivederci");

    return -1;
}