#include <stdio.h>
#include "sim.h"
#include "sim_dev.h"
#include "MEM.h"

#pragma comment(lib, "smicar.lib")

int
main(int argc, char **argv)
{
	///*
    SIMCAR_Interpreter     *interpreter;
    FILE *fp;

    if (argc != 2) {
        fprintf(stderr, "usage:%s filename", argv[0]);
        exit(1);
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "%s not found.\n", argv[1]);
        exit(1);
    }
    interpreter = SIMCAR_create_interpreter();
    SIMCAR_compile(interpreter, fp);
    SIMCAR_interpret(interpreter);
    SIMCAR_dispose_interpreter(interpreter);

    MEM_dump_blocks(stdout);
	//*/


    return 0;
}

