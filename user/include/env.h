#pragma once

typedef struct
{
	bool Initted;
	char* Continuation;
}
TOKENIZE_STATE, *PTOKENIZE_STATE;

#define CLEAR_TOKENIZE_STATE(ts) do { \
	(ts)->Initted = false; \
	(ts)->Continuation = NULL; \
} while (0)

char* OSTokenizeString(PTOKENIZE_STATE State, char* String, const char* Separators);

const char* OSDLLGetEnvironmentVariable(const char* Key);

void OSDLLSetEnvironmentDescription(char* Description);

const char* OSDLLGetEnvironmentDescription();
