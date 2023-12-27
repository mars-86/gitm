#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    int status = init_server(9696);

    if (status) {
        perror("init_server");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
