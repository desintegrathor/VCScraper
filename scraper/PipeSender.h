#pragma once
#include <string>

bool InitPipeSender();
void SendPipeMessage(const std::string& msg);
