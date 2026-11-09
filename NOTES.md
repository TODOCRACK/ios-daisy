# Notas de trabajo

- Memoria: el Mi A2 Lite de 4 GB tiene 3,66 GB visibles (MemTotal). La RAM empieza en
  0x10000000 en este modelo; el shim se ejecuta en 0x10080000.
- Almacenamiento: userdata de 54819536384 bytes (107069407 sectores, LBA 0xE60000).
  Sin cabecera f2fs ni ext4 en claro: probablemente cifrado. No es reducible sin la clave.
- iOS: la raíz de 16 GB (HFS+ sensible a mayúsculas, con cryptexes) se construyó en Linux
  con mkiosroot.sh; hubo que usar un Debian virtual porque WSL no tiene el módulo hfsplus.
- Táctil: el FT5446 responde con id 0x5422 tras alimentar L10 (2,85 V) y L6 (1,8 V) por RPM.
  Con un solo núcleo, la entrega de eventos funciona pero es lenta e irregular.
- Pendiente: medir el arranque con 8 núcleos y compararlo; brillo (driver de backlight);
  GPU (firmware específico del dispositivo, extraer de vendor, sin Metal).
- Tarjeta SD: detección por GPIO 133 confirmada, relojes configurados, pero el controlador
  no genera comandos (INT=0x00000000). Causa no identificada.
