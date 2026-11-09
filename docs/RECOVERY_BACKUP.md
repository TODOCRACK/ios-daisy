# Copia de seguridad antes de cualquier prueba

Antes de tocar nada de la memoria interna, guardar fuera del móvil:

1. **Tabla GPT** (solo lectura desde TWRP):
   ```
   dd if=/dev/block/mmcblk0 of=/external_sd/backup/gpt_primario.bin bs=512 count=34
   dd if=/dev/block/mmcblk0 of=/external_sd/backup/gpt_secundario.bin bs=512 skip=122142687 count=33
   ```
   Tamaños esperados: 17408 y 16896 bytes. El primario debe contener `EFI PART` en el byte 512.
   (Para un móvil con otro tamaño de eMMC, calcular `skip = sectores_totales - 33`.)

2. **`userdata`** (lectura, 54,8 GB, tarda 30-60 min con USB 2):
   ```
   adb exec-out "dd if=/dev/block/bootdevice/by-name/userdata bs=4M" > userdata.img
   ```
   Desde `cmd`, no desde PowerShell. Recortar a los bytes exactos de la partición si `dd` añade su resumen al final.

3. **`ssd`, `keystore`, `boot_a/b`, `aboot_a/b`** (copias pequeñas, mismo método con `dd`).

4. **Hashes**: calcular SHA256 de todo y guardarlos junto a las copias. Restaurar solo si el hash coincide.

5. **Restauración**: con el bootloader desbloqueado (`fastboot getvar unlocked` = yes):
   ```
   fastboot flash userdata userdata.img
   fastboot flash partition gpt_primario.bin   (solo si la tabla se ha tocado)
   fastboot reboot
   ```
   Comprobar antes que `fastboot getvar partition-size:userdata` coincide con el tamaño de la copia.

Nota: la partición `userdata` de Android está cifrada en este dispositivo. Sin la copia completa
de `userdata` (y `ssd`/`keystore` si cambian), los datos no son recuperables.
