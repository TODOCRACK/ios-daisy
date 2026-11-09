# Volver a Android

Requisito: el bootloader está desbloqueado y el PC ve el móvil en fastboot.

1. Comprobar el hash de la copia de userdata (debe ser 556c7ea7...):
   certutil -hashfile E:\daisy\backup\userdata.img SHA256
2. Con el móvil en fastboot:
   fastboot devices
   fastboot flash userdata E:\daisy\backup\userdata.img
   fastboot reboot
   (tarda 30-60 minutos o más según el USB)
3. Si Android se queja de claves o cifrado tras restaurar, restaurar también ssd y keystore
   (copias en E:\daisy\backup_daisy\). No hace falta si no hay quejas.

Para salir sin escribir nada: mantener el botón de encendido 20 segundos (las imágenes
de prueba solo viven en RAM).
