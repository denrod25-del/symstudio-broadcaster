# OBS CMake Windows CPack configuration module

include_guard(GLOBAL)

include(cpackconfig_common)

# --- SymStudio branding (overrides obs-studio defaults from cpackconfig_common) ---
set(CPACK_PACKAGE_NAME "SymStudio")

# Add GPLv2 license file to CPack
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/frontend/data/license/gplv2.txt")
set(CPACK_PACKAGE_VERSION "${OBS_VERSION_CANONICAL}")
set(CPACK_PACKAGE_FILE_NAME "SymStudio-${CPACK_PACKAGE_VERSION}-windows-${CMAKE_VS_PLATFORM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
set(CPACK_GENERATOR ZIP NSIS)
set(CPACK_THREADS 0)

# --- NSIS installer configuration (SymStudio) ---
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SymStudio")
set(CPACK_NSIS_PACKAGE_NAME "SymStudio")
set(CPACK_NSIS_DISPLAY_NAME "SymStudio")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\64bit\\\\SymStudio.exe")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/frontend/cmake/windows/obs-studio.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/frontend/cmake/windows/obs-studio.ico")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "64bit\\\\SymStudio.exe")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/obsproject/obs-studio")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\SymStudio.lnk' '$INSTDIR\\\\bin\\\\64bit\\\\SymStudio.exe'
     CreateShortCut '$DESKTOP\\\\SymStudio.lnk' '$INSTDIR\\\\bin\\\\64bit\\\\SymStudio.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\SymStudio.lnk'
     Delete '$DESKTOP\\\\SymStudio.lnk'")

include(CPack)
