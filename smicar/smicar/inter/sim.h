#ifndef PUBLIC_SIMCAR_H_INCLUDED
#define PUBLIC_SIMCAR_H_INCLUDED
#include <stdio.h>

typedef struct SIMCAR_Interpreter_tag SIMCAR_Interpreter;

__declspec(dllexport) struct SIMCAR_Interpreter_tag;

__declspec(dllexport) SIMCAR_Interpreter *SIMCAR_create_interpreter(void);
__declspec(dllexport) void SIMCAR_compile(SIMCAR_Interpreter *interpreter, FILE *fp);
__declspec(dllexport) void SIMCAR_interpret(SIMCAR_Interpreter *interpreter);
__declspec(dllexport) void SIMCAR_dispose_interpreter(SIMCAR_Interpreter *interpreter);

#endif /* PUBLIC_SIMCAR_H_INCLUDED */
