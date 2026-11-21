#pragma once

void SetupPlatformAppId();
void SetupPlatformTaskbarIcon(void* windowHandle, const wchar_t* fallbackIcoPath);
void ForceWindowActive(void* windowHandle);
