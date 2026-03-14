#pragma once

#include <stdbool.h>

void CmdPrintInitMessage();
void CmdPrintPrompt();
void CmdReadInput();
void CmdParseCommand();
void CmdStartProcess(const char* CommandName, const char* ArgumentBuffer, bool Wait);
bool CmdTryRunningBuiltInCommand(const char* CommandName, const char* Arguments);
