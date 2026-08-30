ESP-HOSTED SDIO v47.5 RX-STABILITY SHIM
=========================================

v47.5 fixes the missing-private-header problem in v47.4.

Root cause:
PlatformIO did not pass the generated GCC @response file to the compilation of
src/esp_hosted_sdio_shim/sdio_drv.c, so sdio_drv.h was not on the include path.

v47.5:
- installs ONLY patched sdio_drv.c under project src;
- keeps the ESP-Hosted tree outside src;
- explicitly adds every required host/private header directory as -I build_flags;
- removes stale v47 response-file integration;
- moves old shim backup directories outside src;
- writes platformio.ini as UTF-8 without BOM.

Install:
  .\sdio-investigation-v47.5\install-shim.ps1

Then clean build only. If successful:
  .\sdio-investigation-v47.5\verify-shim.ps1

Do not upload unless verifier reports PASS.
