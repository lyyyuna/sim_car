#ifndef PUBLIC_SIMCAR_H_INCLUDED
#define PUBLIC_SIMCAR_H_INCLUDED
#include <stdio.h>

typedef struct SIMCAR_Interpreter_tag SIMCAR_Interpreter;

extern "C" __declspec(dllimport) struct SIMCAR_Interpreter_tag;

extern "C" __declspec(dllimport) SIMCAR_Interpreter *SIMCAR_create_interpreter(void);
extern "C" __declspec(dllimport) void SIMCAR_compile(SIMCAR_Interpreter *interpreter, FILE *fp);
extern "C" __declspec(dllimport) void SIMCAR_interpret(SIMCAR_Interpreter *interpreter);
extern "C" __declspec(dllimport) void SIMCAR_dispose_interpreter(SIMCAR_Interpreter *interpreter);

#endif /* PUBLIC_SIMCAR_H_INCLUDED */
